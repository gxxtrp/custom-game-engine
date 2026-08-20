#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/rhi/rhi_buffer.h"

namespace engine::renderer {

struct AutoExposureParams {
    float min_log_luma{-8.0f};
    float max_log_luma{4.0f};
    float speed_up{3.0f};
    float speed_down{1.0f};
    float low_percentile{0.1f};
    float high_percentile{0.9f};
};

struct alignas(16) GPUAutoExposureData {
    float adapted_luminance{1.0f};
    float target_luminance{1.0f};
    float exposure{1.0f};
    float delta_time{0.016f};
};

class AutoExposureSystem {
public:
    static constexpr uint32_t HISTOGRAM_BINS = 256;

    AutoExposureSystem() = default;
    ~AutoExposureSystem();

    bool init();
    void destroy();

    void update(float dt);

    static float compute_temporal_adaptation(float current_luma, float target_luma, float dt, float speed_up, float speed_down);

    const rhi::RhiBuffer& get_histogram_buffer() const { return m_histogram_buffer; }
    const rhi::RhiBuffer& get_exposure_buffer() const { return m_exposure_buffer; }
    const GPUAutoExposureData& get_data() const { return m_data; }

private:
    AutoExposureParams m_params{};
    GPUAutoExposureData m_data{};

    rhi::RhiBuffer m_histogram_buffer;
    rhi::RhiBuffer m_exposure_buffer;
};

} // namespace engine::renderer
