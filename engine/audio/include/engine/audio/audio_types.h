#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/assets/uuid.h"
#include <cstdint>
#include <string>

namespace engine::audio {

enum class AudioBus : uint8_t {
    Master = 0,
    SFX,
    Music,
    Voice,
    Ambient,
    Count
};

enum class AttenuationModel : uint8_t {
    Inverse = 0,
    Linear,
    Exponential
};

struct AudioConfig {
    uint32_t sample_rate{48000};
    uint32_t channels{2};
    float master_volume{1.0f};
    bool enable_spatial_audio{true};
};

} // namespace engine::audio
