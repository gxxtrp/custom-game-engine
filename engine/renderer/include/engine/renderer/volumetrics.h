#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/rhi/rhi_texture.h"
#include "engine/rhi/rhi_buffer.h"

namespace engine::renderer {

struct alignas(16) VolumetricFogUniforms {
    core::Vec3 scattering{0.02f, 0.02f, 0.02f};
    float absorption{0.005f};
    core::Vec3 emission{0.0f, 0.0f, 0.0f};
    float anisotropy{0.7f}; // Henyey-Greenstein g factor
    float density_height_falloff{0.1f};
    float base_density{0.05f};
    float near_z{0.1f};
    float far_z{100.0f};
    uint32_t grid_dim_x{160};
    uint32_t grid_dim_y{90};
    uint32_t grid_dim_z{64};
    uint32_t padding{0};
};

class VolumetricFog {
public:
    VolumetricFog() = default;
    ~VolumetricFog();

    bool init(uint32_t dim_x = 160, uint32_t dim_y = 90, uint32_t dim_z = 64);
    void destroy();

    void set_scattering(const core::Vec3& scattering) { m_uniforms.scattering = scattering; m_dirty = true; }
    void set_absorption(float absorption) { m_uniforms.absorption = absorption; m_dirty = true; }
    void set_anisotropy(float g) { m_uniforms.anisotropy = g; m_dirty = true; }
    void set_density(float base, float height_falloff) {
        m_uniforms.base_density = base;
        m_uniforms.density_height_falloff = height_falloff;
        m_dirty = true;
    }

    void update_gpu_buffer();

    static float henyey_greenstein_phase(float cos_theta, float g);
    static float rayleigh_phase(float cos_theta);

    const rhi::RhiTexture& get_volumetric_texture() const { return m_volumetric_texture; }
    const rhi::RhiBuffer& get_uniform_buffer() const { return m_uniform_buffer; }

private:
    VolumetricFogUniforms m_uniforms{};
    rhi::RhiTexture m_volumetric_texture;
    rhi::RhiBuffer m_uniform_buffer;
    bool m_dirty{true};
};

} // namespace engine::renderer
