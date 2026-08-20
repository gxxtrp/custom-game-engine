#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/rhi/rhi_texture.h"
#include <array>
#include <vector>

namespace engine::renderer {

struct CascadeInfo {
    core::Mat4 view_proj;
    float split_depth{0.0f};
    float shadow_texel_size{0.0f};
};

class CascadedShadowMap {
public:
    static constexpr uint32_t CASCADE_COUNT = 4;

    CascadedShadowMap() = default;
    ~CascadedShadowMap();

    bool init(uint32_t resolution = 2048);
    void destroy();

    void update_cascades(const core::Vec3& light_dir,
                         const core::Mat4& camera_view,
                         float fov_y_rad,
                         float aspect_ratio,
                         float near_z,
                         float far_z,
                         float lambda = 0.85f);

    const std::array<CascadeInfo, CASCADE_COUNT>& get_cascades() const { return m_cascades; }
    core::Vec4 get_cascade_splits() const;

    const rhi::RhiTexture& get_shadow_texture() const { return m_shadow_texture; }
    uint32_t get_resolution() const { return m_resolution; }

private:
    uint32_t m_resolution{2048};
    std::array<CascadeInfo, CASCADE_COUNT> m_cascades{};
    rhi::RhiTexture m_shadow_texture;
};

} // namespace engine::renderer
