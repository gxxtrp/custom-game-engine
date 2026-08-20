#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/rhi/rhi_buffer.h"
#include <vector>
#include <memory>

namespace engine::renderer {

struct alignas(16) GPUDirectionalLight {
    core::Vec3 direction{0.0f, -1.0f, 0.0f};
    float intensity{1.0f};
    core::Vec3 color{1.0f, 1.0f, 1.0f};
    uint32_t cascade_count{4};
    core::Mat4 cascade_view_proj[4];
    core::Vec4 cascade_splits{10.0f, 25.0f, 50.0f, 100.0f};
    uint32_t shadow_map_idx{UINT32_MAX};
    uint32_t padding[3]{0, 0, 0};
};

struct alignas(16) GPUPointLight {
    core::Vec3 position{0.0f, 0.0f, 0.0f};
    float radius{10.0f};
    core::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
    float falloff{1.0f};
    uint32_t shadow_map_idx{UINT32_MAX};
    uint32_t padding[2]{0, 0};
};

struct alignas(16) GPUSpotLight {
    core::Vec3 position{0.0f, 0.0f, 0.0f};
    float range{15.0f};
    core::Vec3 direction{0.0f, -1.0f, 0.0f};
    float intensity{1.0f};
    core::Vec3 color{1.0f, 1.0f, 1.0f};
    float cos_inner{0.9f};
    float cos_outer{0.8f};
    uint32_t shadow_map_idx{UINT32_MAX};
    uint32_t padding[2]{0, 0};
};

struct alignas(16) LightCluster {
    uint32_t offset{0};
    uint32_t point_light_count{0};
    uint32_t spot_light_count{0};
    uint32_t padding{0};
};

struct ClusteredLightingConfig {
    uint32_t grid_dim_x{16};
    uint32_t grid_dim_y{9};
    uint32_t grid_dim_z{24};
    float near_z{0.1f};
    float far_z{100.0f};
};

class ClusteredLightingSystem {
public:
    ClusteredLightingSystem() = default;
    ~ClusteredLightingSystem();

    bool init(const ClusteredLightingConfig& config = {});
    void destroy();

    void update_directional_light(const GPUDirectionalLight& dir_light);
    void set_point_lights(const std::vector<GPUPointLight>& point_lights);
    void set_spot_lights(const std::vector<GPUSpotLight>& spot_lights);

    void cull_lights_cpu(const core::Mat4& view_matrix, const core::Mat4& proj_matrix);

    const rhi::RhiBuffer& get_dir_light_buffer() const { return m_dir_light_buffer; }
    const rhi::RhiBuffer& get_point_light_buffer() const { return m_point_light_buffer; }
    const rhi::RhiBuffer& get_spot_light_buffer() const { return m_spot_light_buffer; }
    const rhi::RhiBuffer& get_cluster_grid_buffer() const { return m_cluster_grid_buffer; }
    const rhi::RhiBuffer& get_cluster_index_list_buffer() const { return m_cluster_index_list_buffer; }

    const ClusteredLightingConfig& get_config() const { return m_config; }
    size_t get_total_clusters() const { return m_clusters.size(); }

private:
    ClusteredLightingConfig m_config{};

    GPUDirectionalLight m_dir_light{};
    std::vector<GPUPointLight> m_point_lights;
    std::vector<GPUSpotLight> m_spot_lights;

    std::vector<LightCluster> m_clusters;
    std::vector<uint32_t> m_light_indices;

    rhi::RhiBuffer m_dir_light_buffer;
    rhi::RhiBuffer m_point_light_buffer;
    rhi::RhiBuffer m_spot_light_buffer;
    rhi::RhiBuffer m_cluster_grid_buffer;
    rhi::RhiBuffer m_cluster_index_list_buffer;
};

} // namespace engine::renderer
