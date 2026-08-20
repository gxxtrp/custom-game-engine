#include "engine/audio/audio_engine.h"
#include "engine/core/log.h"
#include "engine/scene/components.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include <vector>

namespace engine::audio {

struct AudioEngineImpl {
    ma_engine engine{};
    std::vector<ma_sound_group> sound_groups;
};

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    shutdown();
}

AudioEngine& AudioEngine::instance() {
    static AudioEngine s_instance;
    return s_instance;
}

bool AudioEngine::init(const AudioConfig& config) {
    if (m_initialized) return true;
    m_config = config;

    m_impl = std::make_unique<AudioEngineImpl>();

    ma_engine_config engine_config = ma_engine_config_init();
    engine_config.sampleRate = config.sample_rate;
    engine_config.channels = config.channels;

    ma_result res = ma_engine_init(&engine_config, &m_impl->engine);
    if (res != MA_SUCCESS) {
        LOG_FATAL("Audio", "Failed to initialize miniaudio engine: error {}", static_cast<int>(res));
        m_impl.reset();
        return false;
    }

    // Initialize sound groups for each AudioBus
    size_t bus_count = static_cast<size_t>(AudioBus::Count);
    m_impl->sound_groups.resize(bus_count);

    for (size_t i = 0; i < bus_count; ++i) {
        res = ma_sound_group_init(&m_impl->engine, 0, nullptr, &m_impl->sound_groups[i]);
        if (res == MA_SUCCESS) {
            ma_sound_group_set_volume(&m_impl->sound_groups[i], m_bus_volumes[i]);
        }
    }

    set_master_volume(config.master_volume);
    m_initialized = true;

    LOG_INFO("Audio", "Initialized Spatial Audio Engine (miniaudio, {} Hz, {} channels)", 
             config.sample_rate, config.channels);
    return true;
}

void AudioEngine::shutdown() {
    if (!m_initialized || !m_impl) return;

    for (auto& group : m_impl->sound_groups) {
        ma_sound_group_uninit(&group);
    }
    m_impl->sound_groups.clear();

    ma_engine_uninit(&m_impl->engine);
    m_impl.reset();

    m_initialized = false;
    LOG_INFO("Audio", "Spatial Audio Engine shutdown cleanly");
}

void AudioEngine::update(float /*dt*/) {
    // miniaudio operates on its own dedicated high-priority mixing thread
}

void AudioEngine::set_master_volume(float volume) {
    m_config.master_volume = volume;
    if (m_initialized && m_impl) {
        ma_engine_set_volume(&m_impl->engine, volume);
    }
}

void AudioEngine::set_bus_volume(AudioBus bus, float volume) {
    size_t idx = static_cast<size_t>(bus);
    if (idx < static_cast<size_t>(AudioBus::Count)) {
        m_bus_volumes[idx] = volume;
        if (m_initialized && m_impl && idx < m_impl->sound_groups.size()) {
            ma_sound_group_set_volume(&m_impl->sound_groups[idx], volume);
        }
    }
}

float AudioEngine::get_bus_volume(AudioBus bus) const {
    size_t idx = static_cast<size_t>(bus);
    if (idx < static_cast<size_t>(AudioBus::Count)) {
        return m_bus_volumes[idx];
    }
    return 1.0f;
}

void AudioEngine::set_listener(const core::Vec3& position, 
                              const core::Vec3& forward, 
                              const core::Vec3& up, 
                              const core::Vec3& velocity) {
    if (!m_initialized || !m_impl) return;

    ma_engine_listener_set_position(&m_impl->engine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(&m_impl->engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&m_impl->engine, 0, up.x, up.y, up.z);
    ma_engine_listener_set_velocity(&m_impl->engine, 0, velocity.x, velocity.y, velocity.z);
}

bool AudioEngine::play_sound_2d(std::string_view sound_name, AudioBus bus, float volume, float pitch) {
    if (!m_initialized || !m_impl) return false;

    size_t bus_idx = static_cast<size_t>(bus);
    ma_sound_group* group = (bus_idx < m_impl->sound_groups.size()) ? &m_impl->sound_groups[bus_idx] : nullptr;
    ma_result res = ma_engine_play_sound(&m_impl->engine, sound_name.data(), group);
    return res == MA_SUCCESS;
}

bool AudioEngine::play_sound_3d(std::string_view sound_name, const core::Vec3& position, AudioBus bus, float volume, float min_dist, float max_dist) {
    if (!m_initialized || !m_impl) return false;

    size_t bus_idx = static_cast<size_t>(bus);
    ma_sound_group* group = (bus_idx < m_impl->sound_groups.size()) ? &m_impl->sound_groups[bus_idx] : nullptr;

    ma_sound sound;
    ma_result res = ma_sound_init_from_file(&m_impl->engine, sound_name.data(), 0, group, nullptr, &sound);
    if (res != MA_SUCCESS) {
        return false;
    }

    ma_sound_set_position(&sound, position.x, position.y, position.z);
    ma_sound_set_min_distance(&sound, min_dist);
    ma_sound_set_max_distance(&sound, max_dist);
    ma_sound_set_volume(&sound, volume);
    ma_sound_start(&sound);

    return true;
}

void AudioEngine::register_scene(scene::Scene& scene) {
    scene.get_world().component<AudioListenerComponent>();
    scene.get_world().component<AudioSourceComponent>();
}

void AudioEngine::sync_ecs_audio(scene::Scene& scene) {
    if (!m_initialized || !m_impl) return;

    auto& world = scene.get_world();

    // 1. Update Listener
    world.each([this](flecs::entity e, const AudioListenerComponent& listener, const scene::TransformComponent& transform) {
        if (listener.is_active) {
            core::Vec3 fwd = transform.rotation.rotate(listener.forward);
            core::Vec3 up = transform.rotation.rotate(listener.up);
            set_listener(transform.position, fwd, up, listener.velocity);
        }
    });

    // 2. Update Audio Sources
    world.each([this](flecs::entity e, AudioSourceComponent& source, const scene::TransformComponent& transform) {
        if (source.play_on_start && !source.is_playing && !source.sound_name.empty()) {
            if (source.is_spatial) {
                play_sound_3d(source.sound_name, transform.position, source.bus, source.volume, source.min_distance, source.max_distance);
            } else {
                play_sound_2d(source.sound_name, source.bus, source.volume, source.pitch);
            }
            source.is_playing = true;
        }
    });
}

} // namespace engine::audio
