#include "engine/renderer/gpu_scene.h"
#include "engine/core/log.h"

namespace engine::renderer {

GpuScene::~GpuScene() {
    destroy();
}

bool GpuScene::init(uint32_t max_instances) {
    destroy();
    m_max_instances = max_instances;

    // 1. Instance Storage Buffer
    rhi::BufferDesc inst_desc{
        .size = m_max_instances * sizeof(GPUInstanceData),
        .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::CpuToGpu,
        .debug_name = "GpuScene_InstanceBuffer"
    };

    if (!m_instance_buffer.init(inst_desc)) {
        LOG_ERROR("GpuScene", "Failed to create instance buffer");
        return false;
    }

    // 2. Indirect Draw Command Buffer
    rhi::BufferDesc ind_desc{
        .size = m_max_instances * sizeof(DrawIndexedIndirectCommand),
        .usage = rhi::BufferUsage::Indirect | rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::CpuToGpu,
        .debug_name = "GpuScene_IndirectBuffer"
    };

    if (!m_indirect_buffer.init(ind_desc)) {
        LOG_ERROR("GpuScene", "Failed to create indirect draw buffer");
        return false;
    }

    return true;
}

void GpuScene::destroy() {
    m_instance_buffer.destroy();
    m_indirect_buffer.destroy();
    m_instances.clear();
    m_indirect_commands.clear();
}

void GpuScene::clear_instances() {
    m_instances.clear();
    m_indirect_commands.clear();
}

uint32_t GpuScene::add_instance(const GPUInstanceData& instance) {
    if (m_instances.size() >= m_max_instances) {
        LOG_ERROR("GpuScene", "Exceeded max GPU instances ({})", m_max_instances);
        return UINT32_MAX;
    }
    uint32_t index = static_cast<uint32_t>(m_instances.size());
    m_instances.push_back(instance);
    return index;
}

void GpuScene::update_gpu_buffers() {
    if (!m_instances.empty() && m_instance_buffer.is_valid()) {
        m_instance_buffer.upload_data(m_instances.data(), m_instances.size() * sizeof(GPUInstanceData));
    }
}

uint32_t GpuScene::cull_frustum(const core::Frustum& frustum, 
                               const std::vector<std::shared_ptr<Mesh>>& meshes,
                               std::vector<DrawIndexedIndirectCommand>& out_visible_draws) {
    out_visible_draws.clear();
    m_indirect_commands.clear();

    uint32_t visible_count = 0;

    for (size_t inst_idx = 0; inst_idx < m_instances.size(); ++inst_idx) {
        const auto& inst = m_instances[inst_idx];
        if (!(inst.flags & 1)) continue; // Hidden

        // Transform bounding sphere center to world space
        core::Vec4 world_center_v4 = inst.world_matrix * core::Vec4(inst.bounding_center, 1.0f);
        core::Vec3 world_center(world_center_v4.x, world_center_v4.y, world_center_v4.z);

        // Approximate maximum world scale
        float max_scale = std::max({
            core::Vec3(inst.world_matrix.cols[0].x, inst.world_matrix.cols[0].y, inst.world_matrix.cols[0].z).length(),
            core::Vec3(inst.world_matrix.cols[1].x, inst.world_matrix.cols[1].y, inst.world_matrix.cols[1].z).length(),
            core::Vec3(inst.world_matrix.cols[2].x, inst.world_matrix.cols[2].y, inst.world_matrix.cols[2].z).length()
        });
        float world_radius = inst.bounding_radius * max_scale;

        // Frustum vs Sphere culling test
        if (!frustum.intersects_sphere(world_center, world_radius)) {
            continue; // Culled!
        }

        // Build indirect draw call
        if (inst.mesh_index < meshes.size() && meshes[inst.mesh_index]) {
            const auto& mesh = meshes[inst.mesh_index];
            if (inst.submesh_index < mesh->get_submeshes().size()) {
                const auto& submesh = mesh->get_submeshes()[inst.submesh_index];

                DrawIndexedIndirectCommand cmd{};
                cmd.index_count = submesh.index_count;
                cmd.instance_count = 1;
                cmd.first_index = submesh.first_index;
                cmd.vertex_offset = static_cast<int32_t>(submesh.first_vertex);
                cmd.first_instance = static_cast<uint32_t>(inst_idx);

                m_indirect_commands.push_back(cmd);
                out_visible_draws.push_back(cmd);
                visible_count++;
            }
        }
    }

    if (!m_indirect_commands.empty() && m_indirect_buffer.is_valid()) {
        m_indirect_buffer.upload_data(m_indirect_commands.data(), m_indirect_commands.size() * sizeof(DrawIndexedIndirectCommand));
    }

    return visible_count;
}

} // namespace engine::renderer
