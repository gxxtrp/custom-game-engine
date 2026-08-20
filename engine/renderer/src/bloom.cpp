#include "engine/renderer/bloom.h"
#include "engine/core/log.h"
#include <cmath>
#include <algorithm>

namespace engine::renderer {

BloomSystem::~BloomSystem() {
    destroy();
}

bool BloomSystem::init(uint32_t width, uint32_t height) {
    destroy();
    m_width = width;
    m_height = height;

    m_mips.resize(MIP_COUNT);

    uint32_t mip_w = width / 2;
    uint32_t mip_h = height / 2;

    for (uint32_t i = 0; i < MIP_COUNT; ++i) {
        mip_w = std::max(1u, mip_w);
        mip_h = std::max(1u, mip_h);

        rhi::TextureDesc desc{
            .width = mip_w,
            .height = mip_h,
            .format = rhi::Format::R16G16B16A16_SFLOAT,
            .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage,
            .debug_name = "Bloom_Mip_" + std::to_string(i)
        };

        if (!m_mips[i].init(desc)) {
            LOG_ERROR("Bloom", "Failed to create bloom mip level {}", i);
            return false;
        }

        mip_w /= 2;
        mip_h /= 2;
    }

    LOG_INFO("Bloom", "Initialized Physically-Based Bloom ({} mip chain levels)", MIP_COUNT);
    return true;
}

void BloomSystem::resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    init(width, height);
}

void BloomSystem::destroy() {
    for (auto& mip : m_mips) {
        mip.destroy();
    }
    m_mips.clear();
}

core::Vec3 BloomSystem::compute_prefilter(const core::Vec3& color, float threshold, float knee) {
    float luma = 0.2126f * color.x + 0.7152f * color.y + 0.0722f * color.z;
    float soft = std::clamp(luma - threshold + knee, 0.0f, 2.0f * knee);
    soft = (soft * soft) / (4.0f * std::max(knee, 1e-4f));
    float weight = std::max(soft, luma - threshold) / std::max(luma, 1e-4f);
    return color * std::max(0.0f, weight);
}

float BloomSystem::karis_average(const core::Vec3& color) {
    float luma = 0.2126f * color.x + 0.7152f * color.y + 0.0722f * color.z;
    return 1.0f / (1.0f + luma);
}

} // namespace engine::renderer
