#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/rhi/rhi_texture.h"
#include "engine/rhi/rhi_buffer.h"

namespace engine::renderer {

struct ReSTIRSample {
    uint32_t light_index{UINT32_MAX};
    core::Vec3 sample_dir{0.0f, 1.0f, 0.0f};
    core::Vec3 radiance{0.0f, 0.0f, 0.0f};
    float pdf{0.0f};
};

struct alignas(16) Reservoir {
    ReSTIRSample sample{};
    float w_sum{0.0f};
    uint32_t M{0};
    float W{0.0f};
    uint32_t padding{0};

    void update(const ReSTIRSample& new_sample, float weight, float rand_val) {
        w_sum += weight;
        M += 1;
        if (rand_val <= (weight / w_sum)) {
            sample = new_sample;
        }
    }

    void merge(const Reservoir& other, float target_pdf, float rand_val) {
        float weight = target_pdf * other.W * static_cast<float>(other.M);
        w_sum += weight;
        M += other.M;
        if (w_sum > 0.0f && rand_val <= (weight / w_sum)) {
            sample = other.sample;
        }
    }

    void finalize(float target_pdf) {
        if (M > 0 && target_pdf > 0.0f) {
            W = w_sum / (static_cast<float>(M) * target_pdf);
        } else {
            W = 0.0f;
        }
    }
};

class ReSTIRSystem {
public:
    ReSTIRSystem() = default;
    ~ReSTIRSystem();

    bool init(uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);
    void destroy();

    const rhi::RhiTexture& get_normal_roughness_guide() const { return m_normal_roughness_guide; }
    const rhi::RhiTexture& get_motion_vectors_guide() const { return m_motion_vectors_guide; }
    const rhi::RhiTexture& get_radiance_output() const { return m_radiance_output; }

private:
    uint32_t m_width{1280};
    uint32_t m_height{720};

    // Denoising & Guide Targets (NRD compatible)
    rhi::RhiTexture m_normal_roughness_guide;
    rhi::RhiTexture m_motion_vectors_guide;
    rhi::RhiTexture m_radiance_output;

    rhi::RhiBuffer m_temporal_reservoir_buffer;
    rhi::RhiBuffer m_spatial_reservoir_buffer;
};

} // namespace engine::renderer
