#include "engine/renderer/lighting.h"
#include "engine/core/log.h"
#include <cmath>

namespace engine::renderer {

ClusteredLightingSystem::~ClusteredLightingSystem() {
    destroy();
}

bool ClusteredLightingSystem::init(const ClusteredLightingConfig& config) {
    destroy();
    m_config = config;

    uint32_t total_clusters = config.grid_dim_x * config.grid_dim_y * config.grid_dim_z;
    m_clusters.resize(total_clusters);

    // 1. Directional Light Buffer
    rhi::BufferDesc dir_desc{
        .size = sizeof(GPUDirectionalLight),
        .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::CpuToGpu,
        .debug_name = "GPUDirectionalLight_UBO"
    };
    m_dir_light_buffer.init(dir_desc);

    // 2. Point Light Buffer (up to 1024)
    rhi::BufferDesc point_desc{
        .size = 1024 * sizeof(GPUPointLight),
        .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::CpuToGpu,
        .debug_name = "GPUPointLights_SSBO"
    };
    m_point_light_buffer.init(point_desc);

    // 3. Spot Light Buffer (up to 1024)
    rhi::BufferDesc spot_desc{
        .size = 1024 * sizeof(GPUSpotLight),
        .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::CpuToGpu,
        .debug_name = "GPUSpotLights_SSBO"
    };
    m_spot_light_buffer.init(spot_desc);

    // 4. Cluster Grid Buffer
    rhi::BufferDesc grid_desc{
        .size = total_clusters * sizeof(LightCluster),
        .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::CpuToGpu,
        .debug_name = "LightClusterGrid_SSBO"
    };
    m_cluster_grid_buffer.init(grid_desc);

    // 5. Cluster Index List Buffer (e.g. up to 64K indices)
    rhi::BufferDesc idx_desc{
        .size = 65536 * sizeof(uint32_t),
        .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::CpuToGpu,
        .debug_name = "LightClusterIndexList_SSBO"
    };
    m_cluster_index_list_buffer.init(idx_desc);

    LOG_INFO("Lighting", "Initialized Clustered Lighting System with {} froxel clusters ({}x{}x{})",
             total_clusters, config.grid_dim_x, config.grid_dim_y, config.grid_dim_z);
    return true;
}

void ClusteredLightingSystem::destroy() {
    m_dir_light_buffer.destroy();
    m_point_light_buffer.destroy();
    m_spot_light_buffer.destroy();
    m_cluster_grid_buffer.destroy();
    m_cluster_index_list_buffer.destroy();
    m_clusters.clear();
    m_light_indices.clear();
    m_point_lights.clear();
    m_spot_lights.clear();
}

void ClusteredLightingSystem::update_directional_light(const GPUDirectionalLight& dir_light) {
    m_dir_light = dir_light;
    if (m_dir_light_buffer.is_valid()) {
        m_dir_light_buffer.upload_data(&m_dir_light, sizeof(GPUDirectionalLight));
    }
}

void ClusteredLightingSystem::set_point_lights(const std::vector<GPUPointLight>& point_lights) {
    m_point_lights = point_lights;
    if (!m_point_lights.empty() && m_point_light_buffer.is_valid()) {
        m_point_light_buffer.upload_data(m_point_lights.data(), m_point_lights.size() * sizeof(GPUPointLight));
    }
}

void ClusteredLightingSystem::set_spot_lights(const std::vector<GPUSpotLight>& spot_lights) {
    m_spot_lights = spot_lights;
    if (!m_spot_lights.empty() && m_spot_light_buffer.is_valid()) {
        m_spot_light_buffer.upload_data(m_spot_lights.data(), m_spot_lights.size() * sizeof(GPUSpotLight));
    }
}

void ClusteredLightingSystem::cull_lights_cpu(const core::Mat4& view_matrix, const core::Mat4& /*proj_matrix*/) {
    m_light_indices.clear();

    uint32_t cluster_idx = 0;
    for (uint32_t z = 0; z < m_config.grid_dim_z; ++z) {
        // Calculate z near and z far for this slice (logarithmic depth distribution)
        float z_near = m_config.near_z * std::pow(m_config.far_z / m_config.near_z, static_cast<float>(z) / m_config.grid_dim_z);
        float z_far = m_config.near_z * std::pow(m_config.far_z / m_config.near_z, static_cast<float>(z + 1) / m_config.grid_dim_z);

        for (uint32_t y = 0; y < m_config.grid_dim_y; ++y) {
            for (uint32_t x = 0; x < m_config.grid_dim_x; ++x) {
                LightCluster cluster{};
                cluster.offset = static_cast<uint32_t>(m_light_indices.size());

                // Test Point Lights
                for (uint32_t pl_idx = 0; pl_idx < static_cast<uint32_t>(m_point_lights.size()); ++pl_idx) {
                    const auto& pl = m_point_lights[pl_idx];
                    core::Vec4 view_pos_v4 = view_matrix * core::Vec4(pl.position, 1.0f);
                    float view_z = view_pos_v4.z;

                    if (view_z + pl.radius >= z_near && view_z - pl.radius <= z_far) {
                        m_light_indices.push_back(pl_idx);
                        cluster.point_light_count++;
                    }
                }

                // Test Spot Lights
                for (uint32_t sl_idx = 0; sl_idx < static_cast<uint32_t>(m_spot_lights.size()); ++sl_idx) {
                    const auto& sl = m_spot_lights[sl_idx];
                    core::Vec4 view_pos_v4 = view_matrix * core::Vec4(sl.position, 1.0f);
                    float view_z = view_pos_v4.z;

                    if (view_z + sl.range >= z_near && view_z - sl.range <= z_far) {
                        m_light_indices.push_back(sl_idx);
                        cluster.spot_light_count++;
                    }
                }

                m_clusters[cluster_idx++] = cluster;
            }
        }
    }

    // Upload cluster data
    if (m_cluster_grid_buffer.is_valid()) {
        m_cluster_grid_buffer.upload_data(m_clusters.data(), m_clusters.size() * sizeof(LightCluster));
    }
    if (!m_light_indices.empty() && m_cluster_index_list_buffer.is_valid()) {
        m_cluster_index_list_buffer.upload_data(m_light_indices.data(), m_light_indices.size() * sizeof(uint32_t));
    }
}

} // namespace engine::renderer
