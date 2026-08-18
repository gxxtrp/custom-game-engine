#include "engine/renderer/oit.h"
#include "engine/core/log.h"
#include <cmath>
#include <algorithm>

namespace engine::renderer {

WboitRenderer::~WboitRenderer() {
    destroy();
}

bool WboitRenderer::init(uint32_t width, uint32_t height) {
    destroy();
    m_width = width;
    m_height = height;

    // 1. Accumulation Texture (RGBA16F)
    rhi::TextureDesc accum_desc{
        .width = width,
        .height = height,
        .format = rhi::Format::R16G16B16A16_SFLOAT,
        .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled,
        .debug_name = "WBOIT_AccumulationTexture"
    };

    if (!m_accumulation_texture.init(accum_desc)) {
        LOG_ERROR("OIT", "Failed to create WBOIT accumulation texture");
        return false;
    }

    // 2. Revealage Texture (R8_UNORM)
    rhi::TextureDesc reveal_desc{
        .width = width,
        .height = height,
        .format = rhi::Format::R8_UNORM,
        .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled,
        .debug_name = "WBOIT_RevealageTexture"
    };

    if (!m_revealage_texture.init(reveal_desc)) {
        LOG_ERROR("OIT", "Failed to create WBOIT revealage texture");
        return false;
    }

    LOG_INFO("OIT", "Initialized Weighted Blended OIT ({}x{})", width, height);
    return true;
}

void WboitRenderer::resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    init(width, height);
}

void WboitRenderer::destroy() {
    m_accumulation_texture.destroy();
    m_revealage_texture.destroy();
}

float WboitRenderer::compute_depth_weight(float z, float alpha) {
    // McGuire & Bavoil 2013 / Phenix 2015 WBOIT depth weight function
    float z_norm = std::clamp(z, 0.0f, 1.0f);
    float a = std::min(1.0f, alpha) * 8.0f + 0.01f;
    float b = -z_norm * 0.95f + 1.0f;
    return std::clamp(a * a * a * 1e2f * b * b * b, 1e-2f, 3e3f);
}

} // namespace engine::renderer
