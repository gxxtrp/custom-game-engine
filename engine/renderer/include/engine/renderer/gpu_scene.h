#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/rhi/rhi_buffer.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/material.h"
#include <vector>
#include <memory>

namespace engine::renderer {

struct alignas(16) GPUInstanceData {
    core::Mat4 world_matrix{core::Mat4::identity()};
    core::Mat4 prev_world_matrix{core::Mat4::identity()};
    core::Vec3 bounding_center{0.0f, 0.0f, 0.0f};
    float bounding_radius{1.0f};
    uint32_t mesh_index{0};
    uint32_t submesh_index{0};
    uint32_t material_index{0};
    uint32_t flags{1}; // 1 = visible
};

struct DrawIndexedIndirectCommand {
    uint32_t index_count{0};
    uint32_t instance_count{1};
    uint32_t first_index{0};
    int32_t vertex_offset{0};
    uint32_t first_instance{0};
};

class GpuScene {
public:
    GpuScene() = default;
    ~GpuScene();

    bool init(uint32_t max_instances = 10000);
    void destroy();

    void clear_instances();
    uint32_t add_instance(const GPUInstanceData& instance);
    void update_gpu_buffers();

    // Frustum Culling
    uint32_t cull_frustum(const core::Frustum& frustum, 
                          const std::vector<std::shared_ptr<Mesh>>& meshes,
                          std::vector<DrawIndexedIndirectCommand>& out_visible_draws);

    const rhi::RhiBuffer& get_instance_buffer() const { return m_instance_buffer; }
    const rhi::RhiBuffer& get_indirect_buffer() const { return m_indirect_buffer; }
    size_t get_instance_count() const { return m_instances.size(); }

private:
    uint32_t m_max_instances{10000};
    std::vector<GPUInstanceData> m_instances;
    std::vector<DrawIndexedIndirectCommand> m_indirect_commands;

    rhi::RhiBuffer m_instance_buffer;
    rhi::RhiBuffer m_indirect_buffer;
};

} // namespace engine::renderer
