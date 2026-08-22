#pragma once

#include "engine/core/config.h"
#include "engine/renderer/post_process.h"
#include <cstdint>
#include <string_view>

namespace engine::renderer {

enum class QualityPreset : uint8_t {
    Low = 0,
    Medium,
    High,
    Ultra,
    Custom
};

struct GraphicsSettings {
    QualityPreset quality_preset{QualityPreset::High};

    // 1. Shadows
    bool enable_shadows{true};
    uint32_t shadow_resolution{2048};
    uint32_t shadow_cascade_count{4};
    bool shadow_pcf_soft{true};
    float shadow_bias{0.002f};

    // 2. Shading & Post-Processing
    bool enable_post_process{true};
    ToneMapper tone_mapper{ToneMapper::ACES};
    float exposure{1.0f};
    bool enable_bloom{true};
    float bloom_intensity{0.05f};
    bool enable_vignette{true};
    float vignette_intensity{0.25f};
    bool enable_taa{true};
    bool enable_auto_exposure{true};
    bool enable_volumetric_fog{true};

    // 3. Optimization & Culling
    bool enable_frustum_culling{true};

    void apply_preset(QualityPreset preset) {
        quality_preset = preset;
        switch (preset) {
            case QualityPreset::Low:
                enable_shadows = false;
                shadow_resolution = 1024;
                shadow_cascade_count = 1;
                shadow_pcf_soft = false;
                enable_bloom = false;
                enable_vignette = false;
                enable_taa = false;
                enable_auto_exposure = false;
                enable_volumetric_fog = false;
                enable_frustum_culling = true;
                break;
            case QualityPreset::Medium:
                enable_shadows = true;
                shadow_resolution = 1024;
                shadow_cascade_count = 2;
                shadow_pcf_soft = true;
                enable_bloom = false;
                enable_vignette = true;
                enable_taa = true;
                enable_auto_exposure = true;
                enable_volumetric_fog = false;
                enable_frustum_culling = true;
                break;
            case QualityPreset::High:
                enable_shadows = true;
                shadow_resolution = 2048;
                shadow_cascade_count = 4;
                shadow_pcf_soft = true;
                enable_bloom = true;
                enable_vignette = true;
                enable_taa = true;
                enable_auto_exposure = true;
                enable_volumetric_fog = true;
                enable_frustum_culling = true;
                break;
            case QualityPreset::Ultra:
                enable_shadows = true;
                shadow_resolution = 4096;
                shadow_cascade_count = 4;
                shadow_pcf_soft = true;
                enable_bloom = true;
                enable_vignette = true;
                enable_taa = true;
                enable_auto_exposure = true;
                enable_volumetric_fog = true;
                enable_frustum_culling = true;
                break;
            case QualityPreset::Custom:
                break;
        }
    }
};

} // namespace engine::renderer
