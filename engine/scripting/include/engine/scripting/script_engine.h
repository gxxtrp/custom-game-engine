#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/scripting/script_types.h"
#include "engine/scripting/script_components.h"
#include "engine/scene/scene.h"
#include <memory>
#include <string_view>

namespace engine::scripting {

class ScriptEngine {
public:
    static ScriptEngine& instance();

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
    ScriptEngine() = default;
    ~ScriptEngine();

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
