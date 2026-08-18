#include "engine/renderer/shadow_map.h"
#include "engine/core/log.h"
#include <cmath>

namespace engine::renderer {

CascadedShadowMap::~CascadedShadowMap() {
    destroy();
}

bool CascadedShadowMap::init(uint32_t resolution) {
    destroy();
    m_resolution = resolution;

    rhi::TextureDesc desc{
        .width = resolution,
        .height = resolution,
        .depth = 1,
        .mip_levels = 1,
        .array_layers = CASCADE_COUNT,
        .format = rhi::Format::D32_SFLOAT,
        .dimension = rhi::TextureDimension::Texture2DArray,
        .usage = rhi::TextureUsage::DepthAttachment | rhi::TextureUsage::Sampled,
        .debug_name = "CSM_DepthTextureArray"
    };

    if (!m_shadow_texture.init(desc)) {
        LOG_ERROR("ShadowMap", "Failed to create CSM depth texture array ({}x{}x{})", resolution, resolution, CASCADE_COUNT);
        return false;
    }

    LOG_INFO("ShadowMap", "Initialized Cascaded Shadow Map ({}x{}, {} cascades)", resolution, resolution, CASCADE_COUNT);
    return true;
}

void CascadedShadowMap::destroy() {
    m_shadow_texture.destroy();
}

core::Vec4 CascadedShadowMap::get_cascade_splits() const {
    return core::Vec4(m_cascades[0].split_depth, m_cascades[1].split_depth, m_cascades[2].split_depth, m_cascades[3].split_depth);
}

void CascadedShadowMap::update_cascades(const core::Vec3& light_dir,
                                       const core::Mat4& camera_view,
                                       float fov_y_rad,
                                       float aspect_ratio,
                                       float near_z,
                                       float far_z,
                                       float lambda) {
    core::Mat4 cam_inv_view = camera_view.inverted();

    float tan_half_fov = std::tan(fov_y_rad * 0.5f);

    float cascade_splits[CASCADE_COUNT + 1];
    cascade_splits[0] = near_z;

    // Calculate split depths with Practical Split Scheme
    for (uint32_t i = 1; i <= CASCADE_COUNT; ++i) {
        float p = static_cast<float>(i) / static_cast<float>(CASCADE_COUNT);
        float log_split = near_z * std::pow(far_z / near_z, p);
        float lin_split = near_z + (far_z - near_z) * p;
        cascade_splits[i] = lambda * log_split + (1.0f - lambda) * lin_split;
    }

    for (uint32_t i = 0; i < CASCADE_COUNT; ++i) {
        float prev_split = cascade_splits[i];
        float cur_split = cascade_splits[i + 1];
        m_cascades[i].split_depth = cur_split;

        // Calculate 8 corners of the frustum slice in camera space
        float xn = prev_split * tan_half_fov * aspect_ratio;
        float yn = prev_split * tan_half_fov;
        float xf = cur_split * tan_half_fov * aspect_ratio;
        float yf = cur_split * tan_half_fov;

        core::Vec3 corners[8] = {
            // Near slice
            core::Vec3(-xn,  yn, prev_split),
            core::Vec3( xn,  yn, prev_split),
            core::Vec3( xn, -yn, prev_split),
            core::Vec3(-xn, -yn, prev_split),
            // Far slice
            core::Vec3(-xf,  yf, cur_split),
            core::Vec3( xf,  yf, cur_split),
            core::Vec3( xf, -yf, cur_split),
            core::Vec3(-xf, -yf, cur_split)
        };

        // Transform corners to world space & compute center
        core::Vec3 frustum_center(0.0f, 0.0f, 0.0f);
        for (int c = 0; c < 8; ++c) {
            core::Vec4 world_pt_v4 = cam_inv_view * core::Vec4(corners[c], 1.0f);
            corners[c] = core::Vec3(world_pt_v4.x, world_pt_v4.y, world_pt_v4.z);
            frustum_center = frustum_center + corners[c];
        }
        frustum_center = frustum_center * (1.0f / 8.0f);

        // Light view matrix
        core::Vec3 light_norm = light_dir.normalized();
        core::Vec3 light_pos = frustum_center - light_norm * 50.0f;
        core::Mat4 light_view = core::Mat4::look_at(light_pos, frustum_center, core::Vec3(0.0f, 1.0f, 0.0f));

        // Find bounding box in light space
        float min_x = 1e30f, max_x = -1e30f;
        float min_y = 1e30f, max_y = -1e30f;
        float min_z = 1e30f, max_z = -1e30f;

        for (int c = 0; c < 8; ++c) {
            core::Vec4 light_pt_v4 = light_view * core::Vec4(corners[c], 1.0f);
            min_x = std::min(min_x, light_pt_v4.x);
            max_x = std::max(max_x, light_pt_v4.x);
            min_y = std::min(min_y, light_pt_v4.y);
            max_y = std::max(max_y, light_pt_v4.y);
            min_z = std::min(min_z, light_pt_v4.z);
            max_z = std::max(max_z, light_pt_v4.z);
        }

        // Stabilize shadows with Texel Snapping to prevent shimmering
        float world_units_per_texel = (max_x - min_x) / static_cast<float>(m_resolution);
        min_x = std::floor(min_x / world_units_per_texel) * world_units_per_texel;
        max_x = std::floor(max_x / world_units_per_texel) * world_units_per_texel;

        world_units_per_texel = (max_y - min_y) / static_cast<float>(m_resolution);
        min_y = std::floor(min_y / world_units_per_texel) * world_units_per_texel;
        max_y = std::floor(max_y / world_units_per_texel) * world_units_per_texel;

        m_cascades[i].shadow_texel_size = world_units_per_texel;

        // Vulkan depth [0, 1] orthographic projection
        core::Mat4 light_proj = core::Mat4::orthographic(min_x, max_x, min_y, max_y, min_z - 50.0f, max_z + 50.0f);
        m_cascades[i].view_proj = light_proj * light_view;
    }
}

} // namespace engine::renderer
