#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/assets/uuid.h"
#include "engine/audio/audio_types.h"

namespace engine::audio {

struct AudioListenerComponent {
    bool is_active{true};
    core::Vec3 forward{0.0f, 0.0f, 1.0f};
    core::Vec3 up{0.0f, 1.0f, 0.0f};
    core::Vec3 velocity{0.0f, 0.0f, 0.0f};
};

struct AudioSourceComponent {
    assets::UUID sound_asset_uuid;
    std::string sound_name{"Sound"};

    AudioBus bus{AudioBus::SFX};
    float volume{1.0f};
    float pitch{1.0f};
    bool is_looping{false};
    bool is_spatial{true}; // 3D positional vs 2D direct
    bool play_on_start{false};

    // 3D Distance Attenuation
    float min_distance{1.0f};
    float max_distance{50.0f};
    float rolloff_factor{1.0f};
    AttenuationModel attenuation{AttenuationModel::Inverse};

    // Cone Directionality
    float inner_cone_angle_deg{360.0f};
    float outer_cone_angle_deg{360.0f};
    float outer_cone_gain{0.0f};

    bool is_playing{false};
    uint64_t internal_handle{0};
};

} // namespace engine::audio
