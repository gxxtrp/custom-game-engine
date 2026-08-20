#include "engine/renderer/post_process.h"
#include "engine/core/log.h"
#include <cmath>
#include <algorithm>

namespace engine::renderer {

PostProcessSystem::~PostProcessSystem() {
    destroy();
}

bool PostProcessSystem::init() {
    destroy();

    rhi::BufferDesc ubo_desc{
        .size = sizeof(PostProcessUniforms),
        .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::CpuToGpu,
        .debug_name = "PostProcess_UBO"
    };

    if (!m_uniform_buffer.init(ubo_desc)) {
        LOG_ERROR("PostProcess", "Failed to create post-processing uniform buffer");
        return false;
    }

    update_gpu_buffer();
    LOG_INFO("PostProcess", "Initialized Post-Processing System (ACES, AgX, Bloom, Grading)");
    return true;
}

void PostProcessSystem::destroy() {
    m_uniform_buffer.destroy();
}

void PostProcessSystem::update_gpu_buffer() {
    if (m_dirty && m_uniform_buffer.is_valid()) {
        m_uniform_buffer.upload_data(&m_uniforms, sizeof(PostProcessUniforms));
        m_dirty = false;
    }
}

// Stephen Hill's ACES Fit (Approximates ACES RRT + ODT for sRGB)
core::Vec3 PostProcessSystem::aces_fitted(const core::Vec3& v) {
    // sRGB => ACES matrix
    float r = v.x * 0.59719f + v.y * 0.35458f + v.z * 0.04823f;
    float g = v.x * 0.07600f + v.y * 0.90834f + v.z * 0.01566f;
    float b = v.x * 0.02840f + v.y * 0.13383f + v.z * 0.83777f;

    // RRT and ODT fit
    auto rrt_odt = [](float x) {
        float a = x * (x + 0.0245786f) - 0.000090537f;
        float b = x * (0.983729f * x + 0.4329510f) + 0.238081f;
        return a / b;
    };

    r = rrt_odt(r);
    g = rrt_odt(g);
    b = rrt_odt(b);

    // ACES => sRGB matrix
    float out_r =  r * 1.60475f - g * 0.53108f - b * 0.07367f;
    float out_g = -r * 0.10208f + g * 1.10813f - b * 0.00605f;
    float out_b = -r * 0.00327f - g * 0.07276f + b * 1.07602f;

    return core::Vec3(std::clamp(out_r, 0.0f, 1.0f),
                      std::clamp(out_g, 0.0f, 1.0f),
                      std::clamp(out_b, 0.0f, 1.0f));
}

core::Vec3 PostProcessSystem::agx_tonemap(const core::Vec3& v) {
    // AgX logarithmic transform approximation
    constexpr float min_ev = -10.0f;
    constexpr float max_ev = 6.5f;

    auto agx_curve = [](float x) {
        float val = std::clamp((std::log2(std::max(x, 1e-5f)) - min_ev) / (max_ev - min_ev), 0.0f, 1.0f);
        // Sigmoid curve
        return val * val * (3.0f - 2.0f * val);
    };

    return core::Vec3(agx_curve(v.x), agx_curve(v.y), agx_curve(v.z));
}

core::Vec3 PostProcessSystem::rgb_to_ycocg(const core::Vec3& rgb) {
    float y  =  0.25f * rgb.x + 0.50f * rgb.y + 0.25f * rgb.z;
    float co =  0.50f * rgb.x                 - 0.50f * rgb.z;
    float cg = -0.25f * rgb.x + 0.50f * rgb.y - 0.25f * rgb.z;
    return core::Vec3(y, co, cg);
}

core::Vec3 PostProcessSystem::ycocg_to_rgb(const core::Vec3& ycocg) {
    float y  = ycocg.x;
    float co = ycocg.y;
    float cg = ycocg.z;
    float r = y + co - cg;
    float g = y + cg;
    float b = y - co - cg;
    return core::Vec3(r, g, b);
}

} // namespace engine::renderer
