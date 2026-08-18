#include "engine/scripting/script_engine.h"
#include "engine/core/log.h"
#include "engine/vfs/vfs.h"
#include "engine/input/input_manager.h"
#include "engine/audio/audio_engine.h"
#include "engine/physics/physics_system.h"
#include "engine/scene/components.h"
#include "engine/scene/entity.h"

namespace engine::scripting {

// Script Entity Wrapper for Lua
struct ScriptEntityWrapper {
    uint64_t entity_id{0};
    scene::Scene* scene{nullptr};

    std::string get_name() const {
        if (!scene) return "Unknown";
        flecs::entity e = scene->get_world().entity(entity_id);
        if (e.is_valid()) {
            return e.name().c_str();
        }
        return "Invalid";
    }

    core::Vec3 get_position() const {
        if (!scene) return core::Vec3(0.0f, 0.0f, 0.0f);
        flecs::entity e = scene->get_world().entity(entity_id);
        if (e.is_valid() && e.has<scene::TransformComponent>()) {
            return e.get<scene::TransformComponent>().position;
        }
        return core::Vec3(0.0f, 0.0f, 0.0f);
    }

    void set_position(float x, float y, float z) {
        if (!scene) return;
        flecs::entity e = scene->get_world().entity(entity_id);
        if (e.is_valid() && e.has<scene::TransformComponent>()) {
            auto& t = e.ensure<scene::TransformComponent>();
            t.position = core::Vec3(x, y, z);
            t.is_dirty = true;
        }
    }

    void translate(float dx, float dy, float dz) {
        if (!scene) return;
        flecs::entity e = scene->get_world().entity(entity_id);
        if (e.is_valid() && e.has<scene::TransformComponent>()) {
            auto& t = e.ensure<scene::TransformComponent>();
            t.position += core::Vec3(dx, dy, dz);
            t.is_dirty = true;
        }
    }
};

ScriptEngine& ScriptEngine::instance() {
    static ScriptEngine s_instance;
    return s_instance;
}

ScriptEngine::~ScriptEngine() {
    shutdown();
}

bool ScriptEngine::init() {
    if (m_initialized) return true;

    try {
        m_lua.open_libraries(
            sol::lib::base,
            sol::lib::package,
            sol::lib::coroutine,
            sol::lib::string,
            sol::lib::os,
            sol::lib::math,
            sol::lib::table,
            sol::lib::debug
        );

        bind_core_math();
        bind_logging();
        bind_input();
        bind_audio();
        bind_physics();
        bind_scene_ecs();

        m_initialized = true;
        LOG_INFO("Scripting", "Initialized Lua Scripting Subsystem with Sol2 bindings");
        return true;
    } catch (const std::exception& e) {
        LOG_FATAL("Scripting", "Failed to initialize Lua state: {}", e.what());
        return false;
    }
}

void ScriptEngine::shutdown() {
    if (!m_initialized) return;

    m_lua = sol::state();
    m_initialized = false;
    LOG_INFO("Scripting", "Lua Scripting Subsystem shutdown cleanly");
}

void ScriptEngine::bind_core_math() {
    // Vec2
    m_lua.new_usertype<core::Vec2>("Vec2",
        sol::constructors<core::Vec2(), core::Vec2(float, float)>(),
        "x", &core::Vec2::x,
        "y", &core::Vec2::y,
        sol::meta_function::addition, [](const core::Vec2& a, const core::Vec2& b) { return a + b; },
        sol::meta_function::subtraction, [](const core::Vec2& a, const core::Vec2& b) { return a - b; },
        sol::meta_function::multiplication, [](const core::Vec2& a, float s) { return a * s; }
    );

    // Vec3
    m_lua.new_usertype<core::Vec3>("Vec3",
        sol::constructors<core::Vec3(), core::Vec3(float, float, float)>(),
        "x", &core::Vec3::x,
        "y", &core::Vec3::y,
        "z", &core::Vec3::z,
        "length", &core::Vec3::length,
        "normalized", &core::Vec3::normalized,
        sol::meta_function::addition, [](const core::Vec3& a, const core::Vec3& b) { return a + b; },
        sol::meta_function::subtraction, [](const core::Vec3& a, const core::Vec3& b) { return a - b; },
        sol::meta_function::multiplication, [](const core::Vec3& a, float s) { return a * s; }
    );

    // Vec4
    m_lua.new_usertype<core::Vec4>("Vec4",
        sol::constructors<core::Vec4(), core::Vec4(float, float, float, float)>(),
        "x", &core::Vec4::x,
        "y", &core::Vec4::y,
        "z", &core::Vec4::z,
        "w", &core::Vec4::w
    );

    // Quat
    m_lua.new_usertype<core::Quat>("Quat",
        sol::constructors<core::Quat(), core::Quat(float, float, float, float)>(),
        "x", &core::Quat::x,
        "y", &core::Quat::y,
        "z", &core::Quat::z,
        "w", &core::Quat::w,
        "identity", &core::Quat::identity
    );
}

void ScriptEngine::bind_logging() {
    auto log_table = m_lua.create_named_table("Log");
    log_table.set_function("info", [](std::string_view msg) {
        LOG_INFO("Lua", "{}", msg);
    });
    log_table.set_function("warn", [](std::string_view msg) {
        LOG_WARN("Lua", "{}", msg);
    });
    log_table.set_function("error", [](std::string_view msg) {
        LOG_ERROR("Lua", "{}", msg);
    });
}

void ScriptEngine::bind_input() {
    auto input_table = m_lua.create_named_table("Input");
    input_table.set_function("is_action_down", [](std::string_view action_name) -> bool {
        return input::InputManager::instance().is_action_down(action_name);
    });
    input_table.set_function("is_action_just_pressed", [](std::string_view action_name) -> bool {
        return input::InputManager::instance().is_action_just_pressed(action_name);
    });
    input_table.set_function("get_axis", [](std::string_view axis_name) -> float {
        return input::InputManager::instance().get_axis(axis_name);
    });
    input_table.set_function("get_mouse_x", []() -> float {
        return input::InputManager::instance().get_mouse_position().x;
    });
    input_table.set_function("get_mouse_y", []() -> float {
        return input::InputManager::instance().get_mouse_position().y;
    });
}

void ScriptEngine::bind_audio() {
    auto audio_table = m_lua.create_named_table("Audio");
    audio_table.set_function("play_sound_2d", [](std::string_view name, float volume, float pitch) -> bool {
        return audio::AudioEngine::instance().play_sound_2d(name, audio::AudioBus::SFX, volume, pitch);
    });
    audio_table.set_function("play_sound_3d", [](std::string_view name, float x, float y, float z, float volume, float min_dist, float max_dist) -> bool {
        return audio::AudioEngine::instance().play_sound_3d(name, core::Vec3(x, y, z), audio::AudioBus::SFX, volume, min_dist, max_dist);
    });
    audio_table.set_function("set_master_volume", [](float volume) {
        audio::AudioEngine::instance().set_master_volume(volume);
    });
}

void ScriptEngine::bind_physics() {
    auto physics_table = m_lua.create_named_table("Physics");
    physics_table.set_function("raycast", [](float ox, float oy, float oz, float dx, float dy, float dz, float max_dist) {
        physics::RaycastHit hit;
        bool has_hit = physics::PhysicsSystem::instance().raycast(
            core::Vec3(ox, oy, oz),
            core::Vec3(dx, dy, dz),
            max_dist,
            hit
        );
        return std::make_tuple(has_hit, hit.fraction, hit.position.x, hit.position.y, hit.position.z);
    });
}

void ScriptEngine::bind_scene_ecs() {
    m_lua.new_usertype<ScriptEntityWrapper>("Entity",
        "get_name", &ScriptEntityWrapper::get_name,
        "get_position", &ScriptEntityWrapper::get_position,
        "set_position", &ScriptEntityWrapper::set_position,
        "translate", &ScriptEntityWrapper::translate
    );
}

ScriptResult ScriptEngine::execute_string(std::string_view code) {
    if (!m_initialized) {
        return { false, "ScriptEngine not initialized" };
    }

    try {
        auto result = m_lua.script(code);
        if (!result.valid()) {
            sol::error err = result;
            return { false, err.what() };
        }
        return { true, "" };
    } catch (const sol::error& err) {
        return { false, err.what() };
    } catch (const std::exception& e) {
        return { false, e.what() };
    }
}

ScriptResult ScriptEngine::execute_file(std::string_view file_path) {
    std::string script_str;
    if (!vfs::VFS::instance().read_string(file_path, script_str)) {
        return { false, "Failed to read file from VFS: " + std::string(file_path) };
    }

    return execute_string(script_str);
}

void ScriptEngine::register_scene(scene::Scene& scene) {
    scene.get_world().component<ScriptComponent>();
}

void ScriptEngine::sync_ecs_scripts(scene::Scene& scene, float dt) {
    if (!m_initialized) return;

    auto& world = scene.get_world();
    world.each([this, &scene, dt](flecs::entity e, ScriptComponent& sc) {
        ScriptEntityWrapper wrapper{
            .entity_id = e.id(),
            .scene = &scene
        };

        if (!sc.is_initialized) {
            if (!sc.script_source.empty()) {
                auto res = execute_string(sc.script_source);
                if (res.success) {
                    sol::object obj = m_lua[sc.class_name];
                    if (obj.is<sol::table>()) {
                        sol::table cls = obj.as<sol::table>();
                        sc.self_table = m_lua.create_table();
                        
                        sol::table mt = m_lua.create_table();
                        mt[sol::meta_function::index] = cls;
                        sc.self_table[sol::metatable_key] = mt;

                        sol::function init_fn = cls["on_init"];
                        if (init_fn.valid()) {
                            init_fn(sc.self_table, wrapper);
                        }
                        sc.is_valid = true;
                    }
                } else {
                    LOG_ERROR("Scripting", "Script error on entity '{}': {}", e.name().c_str(), res.error_message);
                }
            }
            sc.is_initialized = true;
        }

        if (sc.is_valid && sc.self_table.valid()) {
            sol::function update_fn = sc.self_table["on_update"];
            if (update_fn.valid()) {
                update_fn(sc.self_table, wrapper, dt);
            }
        }
    });
}

} // namespace engine::scripting
