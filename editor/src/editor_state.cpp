#include "editor/editor_state.h"
#include "engine/scene/map_serializer.h"
#include "engine/scene/components.h"
#include "engine/physics/physics_system.h"
#include "engine/physics/physics_components.h"
#include "engine/audio/audio_engine.h"
#include "engine/audio/audio_components.h"
#include "engine/scripting/script_engine.h"
#include "engine/scripting/script_components.h"
#include "engine/core/log.h"

namespace editor {

EditorStateManager::EditorStateManager() = default;
EditorStateManager::~EditorStateManager() = default;

void EditorStateManager::set_mode(EditorMode mode) {
    if (m_mode == mode) return;

    EditorMode prev = m_mode;
    m_mode = mode;

    for (const auto& listener : m_mode_listeners) {
        if (listener) {
            listener(prev, mode);
        }
    }
}

void EditorStateManager::snapshot_scene(engine::scene::Scene& active_scene) {
    m_scene_snapshot_toml.clear();
    if (engine::scene::MapSerializer::serialize_to_toml(active_scene, m_scene_snapshot_toml)) {
        LOG_INFO("EditorState", "Saved pristine scene snapshot ({} bytes)", m_scene_snapshot_toml.size());
    } else {
        LOG_ERROR("EditorState", "Failed to create scene memory snapshot!");
    }
}

void EditorStateManager::cleanup_scene_physics(engine::scene::Scene& active_scene) {
    active_scene.get_world().each([](flecs::entity, engine::physics::RigidBodyComponent& rb) {
        if (rb.is_registered && !rb.body_id.IsInvalid()) {
            engine::physics::PhysicsSystem::instance().destroy_body(rb.body_id);
            rb.is_registered = false;
            rb.body_id = JPH::BodyID();
        }
    });
}

void EditorStateManager::restore_scene(engine::scene::Scene& active_scene, SelectionContext& selection) {
    if (m_scene_snapshot_toml.empty()) {
        LOG_WARN("EditorState", "No scene snapshot available to restore");
        return;
    }

    selection.clear();
    active_scene.clear();

    if (engine::scene::MapSerializer::deserialize_from_toml(m_scene_snapshot_toml, active_scene)) {
        LOG_INFO("EditorState", "Restored pristine pre-play scene snapshot successfully ({} entities)",
                 active_scene.get_entity_count());
    } else {
        LOG_ERROR("EditorState", "Failed to restore scene from memory snapshot!");
    }

    // Re-register subsystem ECS schemas
    engine::physics::PhysicsSystem::instance().register_scene(active_scene);
    engine::audio::AudioEngine::instance().register_scene(active_scene);
    engine::scripting::ScriptEngine::instance().register_scene(active_scene);

    m_scene_snapshot_toml.clear();
}

void EditorStateManager::start_play(engine::scene::Scene& active_scene, SelectionContext& selection) {
    if (m_mode == EditorMode::Paused && m_pre_pause_mode == EditorMode::Play) {
        resume();
        return;
    }

    if (m_mode == EditorMode::Edit) {
        snapshot_scene(active_scene);
        set_mode(EditorMode::Play);

        // Initialize Physics & Scripts for Play-In-Editor
        engine::physics::PhysicsSystem::instance().sync_to_physics(active_scene);
        engine::audio::AudioEngine::instance().sync_ecs_audio(active_scene);
        engine::scripting::ScriptEngine::instance().sync_ecs_scripts(active_scene, 0.0f);

        LOG_INFO("EditorState", "Entered Play-In-Editor (PIE) Mode");
    }
}

void EditorStateManager::start_simulate(engine::scene::Scene& active_scene, SelectionContext& selection) {
    if (m_mode == EditorMode::Paused && m_pre_pause_mode == EditorMode::Simulate) {
        resume();
        return;
    }

    if (m_mode == EditorMode::Edit) {
        snapshot_scene(active_scene);
        set_mode(EditorMode::Simulate);

        // Initialize Physics & Scripts for Simulation
        engine::physics::PhysicsSystem::instance().sync_to_physics(active_scene);
        engine::audio::AudioEngine::instance().sync_ecs_audio(active_scene);
        engine::scripting::ScriptEngine::instance().sync_ecs_scripts(active_scene, 0.0f);

        LOG_INFO("EditorState", "Entered Physics/Script Simulation Mode");
    }
}

void EditorStateManager::pause() {
    if (m_mode == EditorMode::Play || m_mode == EditorMode::Simulate) {
        m_pre_pause_mode = m_mode;
        set_mode(EditorMode::Paused);
        LOG_INFO("EditorState", "Simulation Paused");
    }
}

void EditorStateManager::resume() {
    if (m_mode == EditorMode::Paused) {
        set_mode(m_pre_pause_mode);
        LOG_INFO("EditorState", "Simulation Resumed ({})", 
                 m_pre_pause_mode == EditorMode::Play ? "Play" : "Simulate");
    }
}

void EditorStateManager::stop(engine::scene::Scene& active_scene, SelectionContext& selection) {
    if (m_mode == EditorMode::Edit) return;

    cleanup_scene_physics(active_scene);
    restore_scene(active_scene, selection);
    set_mode(EditorMode::Edit);

    LOG_INFO("EditorState", "Stopped Simulation & Returned to Edit Mode");
}

void EditorStateManager::step_frame(engine::scene::Scene& active_scene, float dt) {
    if (m_mode == EditorMode::Edit || m_mode == EditorMode::Paused) {
        engine::physics::PhysicsSystem::instance().sync_to_physics(active_scene);
        engine::physics::PhysicsSystem::instance().update(dt);
        engine::physics::PhysicsSystem::instance().sync_from_physics(active_scene);

        engine::audio::AudioEngine::instance().sync_ecs_audio(active_scene);
        engine::audio::AudioEngine::instance().update(dt);

        engine::scripting::ScriptEngine::instance().sync_ecs_scripts(active_scene, dt);
        active_scene.update(dt);

        LOG_INFO("EditorState", "Stepped simulation frame (dt: {:.4f}s)", dt);
    }
}

void EditorStateManager::update(engine::scene::Scene& active_scene, float dt) {
    if (m_mode == EditorMode::Play || m_mode == EditorMode::Simulate) {
        if (dt > 0.0f && dt < 0.1f) {
            engine::physics::PhysicsSystem::instance().sync_to_physics(active_scene);
            engine::physics::PhysicsSystem::instance().update(dt);
            engine::physics::PhysicsSystem::instance().sync_from_physics(active_scene);

            engine::audio::AudioEngine::instance().sync_ecs_audio(active_scene);
            engine::audio::AudioEngine::instance().update(dt);

            engine::scripting::ScriptEngine::instance().sync_ecs_scripts(active_scene, dt);
            active_scene.update(dt);
        }
    }
}

} // namespace editor
