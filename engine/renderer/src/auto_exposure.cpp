#include "engine/renderer/auto_exposure.h"
#include "engine/core/log.h"
#include <cmath>
#include <algorithm>

namespace engine::renderer {

AutoExposureSystem::~AutoExposureSystem() {
    destroy();
}

bool AutoExposureSystem::init() {
    destroy();

    // 1. Histogram Storage Buffer
    rhi::BufferDesc hist_desc{
        .size = HISTOGRAM_BINS * sizeof(uint32_t),
        .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::GpuOnly,
        .debug_name = "AutoExposure_Histogram"
    };

    if (!m_histogram_buffer.init(hist_desc)) {
        LOG_ERROR("AutoExposure", "Failed to create histogram buffer");
        return false;
    }

    // 2. Exposure Uniform / Storage Buffer
    rhi::BufferDesc exp_desc{
        .size = sizeof(GPUAutoExposureData),
        .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::CpuToGpu,
        .debug_name = "AutoExposure_Data"
    };

    if (!m_exposure_buffer.init(exp_desc)) {
        LOG_ERROR("AutoExposure", "Failed to create exposure data buffer");
        return false;
    }

    m_data.adapted_luminance = 1.0f;
    m_data.target_luminance = 1.0f;
    m_data.exposure = 1.0f;
    m_data.delta_time = 0.016f;
    m_exposure_buffer.upload_data(&m_data, sizeof(GPUAutoExposureData));

    LOG_INFO("AutoExposure", "Initialized Auto-Exposure System ({} Bins Log-Histogram)", HISTOGRAM_BINS);
    return true;
}

void AutoExposureSystem::destroy() {
    m_histogram_buffer.destroy();
    m_exposure_buffer.destroy();
}

void AutoExposureSystem::update(float dt) {
    m_data.delta_time = dt;
    m_data.adapted_luminance = compute_temporal_adaptation(
        m_data.adapted_luminance, 
        m_data.target_luminance, 
        dt, 
        m_params.speed_up, 
        m_params.speed_down
    );
    // Key value exposure formula: EV100 = log2(Luma * 100 / 12.5) -> exposure = 1.0 / (1.2 * Luma)
    m_data.exposure = 1.0f / std::max(m_data.adapted_luminance, 1e-3f);

    if (m_exposure_buffer.is_valid()) {
        m_exposure_buffer.upload_data(&m_data, sizeof(GPUAutoExposureData));
    }
}

float AutoExposureSystem::compute_temporal_adaptation(float current, float target, float dt, float speed_up, float speed_down) {
    float speed = (target > current) ? speed_up : speed_down;
    return current + (target - current) * (1.0f - std::exp(-dt * speed));
}

} // namespace engine::renderer
