#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/core/subsystem.h"
#include "engine/core/engine_context.h"
#include "engine/scripting/script_types.h"
#include "engine/scripting/script_components.h"
#include "engine/scene/scene.h"
#include <memory>
#include <string_view>

namespace engine::scripting {

class ScriptEngine final : public core::ISubsystem {
public:
    static ScriptEngine& instance();

    ScriptEngine();
    ~ScriptEngine() override;

    // ISubsystem Interface
    [[nodiscard]] const char* get_name() const noexcept override {
        return "ScriptEngine";
    }

    void declare_dependencies(core::SubsystemDependencyBuilder& builder) override;
    bool initialize(core::EngineContext& context) override;
    void tick(core::EngineContext& context, core::ExecutionPhase phase, float dt) override;
    void shutdown(core::EngineContext& context) override;

    [[nodiscard]] bool participates_in_phase(core::ExecutionPhase phase) const noexcept override {
        return phase == core::ExecutionPhase::Simulation;
    }

    // Direct Lifecycle Management
    bool init();
    void shutdown();

    ScriptResult execute_string(std::string_view code);
    ScriptResult execute_file(std::string_view file_path);

    // Flecs ECS Synchronization
    void register_scene(scene::Scene& scene);
    void sync_ecs_scripts(scene::Scene& scene, float dt);

    sol::state& get_state() { return m_lua; }
    bool is_initialized() const { return m_initialized; }

private:
    void bind_core_math();
    void bind_logging();
    void bind_input();
    void bind_audio();
    void bind_physics();
    void bind_scene_ecs();

    sol::state m_lua;
    bool m_initialized{false};
};

} // namespace engine::scripting
