#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/core/subsystem.h"
#include "engine/core/engine_context.h"
#include "engine/audio/audio_types.h"
#include "engine/audio/audio_components.h"
#include "engine/scene/scene.h"
#include <memory>
#include <string_view>

namespace engine::audio {

struct AudioEngineImpl;

class AudioEngine final : public core::ISubsystem {
public:
    static AudioEngine& instance();

    AudioEngine();
    ~AudioEngine() override;

    // ISubsystem Interface
    [[nodiscard]] const char* get_name() const noexcept override {
        return "AudioEngine";
    }

    void declare_dependencies(core::SubsystemDependencyBuilder& builder) override;
    bool initialize(core::EngineContext& context) override;
    void tick(core::EngineContext& context, core::ExecutionPhase phase, float dt) override;
    void shutdown(core::EngineContext& context) override;

    [[nodiscard]] bool participates_in_phase(core::ExecutionPhase phase) const noexcept override {
        return phase == core::ExecutionPhase::PostSimulation;
    }

    // Direct Lifecycle Management
    bool init(const AudioConfig& config = {});
    void shutdown();
    void update(float dt);

    // Global Controls
    void set_master_volume(float volume);
    float get_master_volume() const { return m_config.master_volume; }
    void set_bus_volume(AudioBus bus, float volume);
    float get_bus_volume(AudioBus bus) const;

    // 3D Listener Controls
    void set_listener(const core::Vec3& position, 
                     const core::Vec3& forward = core::Vec3(0.0f, 0.0f, 1.0f), 
                     const core::Vec3& up = core::Vec3(0.0f, 1.0f, 0.0f), 
                     const core::Vec3& velocity = core::Vec3(0.0f, 0.0f, 0.0f));

    // One-shot Fire-and-Forget Audio
    bool play_sound_2d(std::string_view sound_name, AudioBus bus = AudioBus::SFX, float volume = 1.0f, float pitch = 1.0f);
    bool play_sound_3d(std::string_view sound_name, const core::Vec3& position, AudioBus bus = AudioBus::SFX, float volume = 1.0f, float min_dist = 1.0f, float max_dist = 50.0f);

    // Flecs ECS Synchronization
    void register_scene(scene::Scene& scene);
    void sync_ecs_audio(scene::Scene& scene);

    bool is_initialized() const { return m_initialized; }

private:
    AudioConfig m_config{};
    std::unique_ptr<AudioEngineImpl> m_impl;

    float m_bus_volumes[static_cast<size_t>(AudioBus::Count)]{1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    bool m_initialized{false};
};

} // namespace engine::audio
