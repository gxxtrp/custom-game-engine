#include "engine/renderer/taa.h"
#include "engine/core/log.h"
#include <cmath>
#include <algorithm>

namespace engine::renderer {

TaaSystem::~TaaSystem() {
    destroy();
}

bool TaaSystem::init(uint32_t width, uint32_t height) {
    destroy();
    m_width = width;
    m_height = height;

    for (int i = 0; i < 2; ++i) {
        rhi::TextureDesc desc{
            .width = width,
            .height = height,
            .format = rhi::Format::R16G16B16A16_SFLOAT,
            .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage,
            .debug_name = "TAA_HistoryTexture_" + std::to_string(i)
        };
        if (!m_history_textures[i].init(desc)) {
            LOG_ERROR("TAA", "Failed to create TAA history texture {}", i);
            return false;
        }
    }

    LOG_INFO("TAA", "Initialized TAA System ({}x{}, Double-Buffered History)", width, height);
    return true;
}

void TaaSystem::resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    init(width, height);
}

void TaaSystem::destroy() {
    m_history_textures[0].destroy();
    m_history_textures[1].destroy();
}

float TaaSystem::halton(uint32_t index, uint32_t base) {
    float f = 1.0f;
    float result = 0.0f;
    uint32_t i = index;
    while (i > 0) {
        f = f / static_cast<float>(base);
        result += f * static_cast<float>(i % base);
        i = i / base;
    }
    return result;
}

core::Vec2 TaaSystem::get_jitter(uint32_t frame_index) {
    uint32_t idx = (frame_index % JITTER_PHASE_COUNT) + 1;
    // Map from [0, 1] to [-0.5, 0.5]
    float jx = halton(idx, 2) - 0.5f;
    float jy = halton(idx, 3) - 0.5f;
    return core::Vec2(jx, jy);
}

core::Mat4 TaaSystem::apply_jitter(const core::Mat4& proj, const core::Vec2& jitter, uint32_t width, uint32_t height) {
    core::Mat4 jittered_proj = proj;
    // Apply jitter in NDC space (-1 to +1)
    float delta_x = (jitter.x * 2.0f) / static_cast<float>(width);
    float delta_y = (jitter.y * 2.0f) / static_cast<float>(height);
    jittered_proj.cols[2].x += delta_x;
    jittered_proj.cols[2].y += delta_y;
    return jittered_proj;
}

core::Vec3 TaaSystem::clip_aabb_ycocg(const core::Vec3& history,
                                      const std::array<core::Vec3, 9>& neighborhood,
                                      float gamma) {
    core::Vec3 m1(0.0f, 0.0f, 0.0f);
    core::Vec3 m2(0.0f, 0.0f, 0.0f);

    for (const auto& sample : neighborhood) {
        m1 = m1 + sample;
        m2 = m2 + core::Vec3(sample.x * sample.x, sample.y * sample.y, sample.z * sample.z);
    }

    core::Vec3 mu = m1 * (1.0f / 9.0f);
    core::Vec3 sigma(
        std::sqrt(std::max(0.0f, m2.x / 9.0f - mu.x * mu.x)),
        std::sqrt(std::max(0.0f, m2.y / 9.0f - mu.y * mu.y)),
        std::sqrt(std::max(0.0f, m2.z / 9.0f - mu.z * mu.z))
    );

    core::Vec3 min_bound = mu - sigma * gamma;
    core::Vec3 max_bound = mu + sigma * gamma;

    return core::Vec3(
        std::clamp(history.x, min_bound.x, max_bound.x),
        std::clamp(history.y, min_bound.y, max_bound.y),
        std::clamp(history.z, min_bound.z, max_bound.z)
    );
}

} // namespace engine::renderer
