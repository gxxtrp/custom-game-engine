#pragma once

#include "engine/core/subsystem.h"
#include "engine/scene/scene.h"
#include <memory>
#include <string>

namespace engine::scene {

class SceneSubsystem final : public core::ISubsystem {
public:
    SceneSubsystem();
    explicit SceneSubsystem(std::string default_scene_name);
    ~SceneSubsystem() override;

    [[nodiscard]] const char* get_name() const noexcept override {
        return "SceneSubsystem";
    }

    void declare_dependencies(core::SubsystemDependencyBuilder& builder) override;

    bool initialize(core::EngineContext& context) override;

    void tick(core::EngineContext& context, core::ExecutionPhase phase, float dt) override;

    void shutdown(core::EngineContext& context) override;

    [[nodiscard]] bool participates_in_phase(core::ExecutionPhase phase) const noexcept override {
        return phase == core::ExecutionPhase::Simulation || phase == core::ExecutionPhase::PostSimulation;
    }

    [[nodiscard]] Scene& get_active_scene() noexcept { return m_active_scene; }
    [[nodiscard]] const Scene& get_active_scene() const noexcept { return m_active_scene; }

private:
    Scene m_active_scene;
};

} // namespace engine::scene
