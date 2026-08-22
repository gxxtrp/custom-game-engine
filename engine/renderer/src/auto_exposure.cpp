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
        .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::TransferSrc | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::GpuOnly,
        .debug_name = "AutoExposure_Histogram"
    };

    if (!m_histogram_buffer.init(hist_desc)) {
        LOG_ERROR("AutoExposure", "Failed to create histogram buffer");
        return false;
    }

    // 1b. Host-visible readback buffer (histogram copy destination)
    rhi::BufferDesc readback_desc{
        .size = HISTOGRAM_BINS * sizeof(uint32_t),
        .usage = rhi::BufferUsage::TransferDst,
        .memory_usage = rhi::MemoryUsage::GpuToCpu,
        .debug_name = "AutoExposure_Readback"
    };

    if (!m_readback_buffer.init(readback_desc)) {
        LOG_ERROR("AutoExposure", "Failed to create histogram readback buffer");
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
    m_readback_buffer.destroy();
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

bool AutoExposureSystem::readback_histogram(std::vector<uint32_t>& out_bins) {
    if (!m_readback_buffer.is_valid()) return false;

    void* mapped = m_readback_buffer.map();
    if (!mapped) return false;

    out_bins.assign(static_cast<const uint32_t*>(mapped),
                    static_cast<const uint32_t*>(mapped) + HISTOGRAM_BINS);
    m_readback_buffer.unmap();
    return true;
}

float AutoExposureSystem::compute_average_luminance(const std::vector<uint32_t>& bins,
                                                    float min_log_luma,
                                                    float max_log_luma) {
    uint64_t total = 0;
    double weighted_sum = 0.0;
    const double log_range = static_cast<double>(max_log_luma - min_log_luma);

    for (uint32_t i = 0; i < static_cast<uint32_t>(bins.size()); ++i) {
        total += bins[i];
        // Bin center in log-luma space, converted back to linear luminance.
        double log_center = min_log_luma + (static_cast<double>(i) + 0.5) * log_range / 256.0;
        weighted_sum += static_cast<double>(bins[i]) * std::exp2(log_center);
    }
    if (total == 0) return 1.0f;
    return static_cast<float>(weighted_sum / static_cast<double>(total));
}

} // namespace engine::renderer
