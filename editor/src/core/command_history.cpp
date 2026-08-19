#include "editor/core/command_history.h"
#include "engine/core/log.h"

namespace editor {

// --- EntitySnapshot Implementation ---
EntitySnapshot EntitySnapshot::capture(flecs::entity entity, engine::scene::Scene& scene) {
    EntitySnapshot snapshot;
    if (!entity.is_valid() || !entity.is_alive()) return snapshot;

    if (entity.has<engine::scene::UUIDComponent>()) {
        snapshot.uuid = entity.get<engine::scene::UUIDComponent>().uuid;
    } else {
        snapshot.uuid = engine::assets::UUID::generate();
    }

    if (entity.has<engine::scene::TagComponent>()) {
        snapshot.name = entity.get<engine::scene::TagComponent>().name;
    } else {
        snapshot.name = entity.name().c_str();
    }

    if (entity.parent().is_valid() && entity.parent().has<engine::scene::UUIDComponent>()) {
        snapshot.parent_uuid = entity.parent().get<engine::scene::UUIDComponent>().uuid;
    }

    if (entity.has<engine::scene::TransformComponent>()) {
        snapshot.transform = entity.get<engine::scene::TransformComponent>();
    }
    if (entity.has<engine::scene::MeshRendererComponent>()) {
        snapshot.mesh_renderer = entity.get<engine::scene::MeshRendererComponent>();
    }
    if (entity.has<engine::scene::DirectionalLightComponent>()) {
        snapshot.directional_light = entity.get<engine::scene::DirectionalLightComponent>();
    }
    if (entity.has<engine::scene::PointLightComponent>()) {
        snapshot.point_light = entity.get<engine::scene::PointLightComponent>();
    }
    if (entity.has<engine::scene::SpotLightComponent>()) {
        snapshot.spot_light = entity.get<engine::scene::SpotLightComponent>();
    }
    if (entity.has<engine::scene::CameraComponent>()) {
        snapshot.camera = entity.get<engine::scene::CameraComponent>();
    }
    if (entity.has<engine::physics::RigidBodyComponent>()) {
        snapshot.rigidbody = entity.get<engine::physics::RigidBodyComponent>();
    }
    if (entity.has<engine::physics::ColliderComponent>()) {
        snapshot.collider = entity.get<engine::physics::ColliderComponent>();
    }
    if (entity.has<engine::audio::AudioSourceComponent>()) {
        snapshot.audio_source = entity.get<engine::audio::AudioSourceComponent>();
    }
    if (entity.has<engine::audio::AudioListenerComponent>()) {
        snapshot.audio_listener = entity.get<engine::audio::AudioListenerComponent>();
    }
    if (entity.has<engine::scripting::ScriptComponent>()) {
        snapshot.script = entity.get<engine::scripting::ScriptComponent>();
    }

    return snapshot;
}

flecs::entity EntitySnapshot::restore(engine::scene::Scene& scene) const {
    engine::scene::Entity entity = scene.find_entity_by_uuid(uuid);
    if (!entity.is_valid()) {
        entity = scene.create_entity(name, uuid);
    } else {
        entity.set_name(name);
    }

    if (transform.has_value()) entity.set<engine::scene::TransformComponent>(*transform);
    if (mesh_renderer.has_value()) entity.set<engine::scene::MeshRendererComponent>(*mesh_renderer);
    if (directional_light.has_value()) entity.set<engine::scene::DirectionalLightComponent>(*directional_light);
    if (point_light.has_value()) entity.set<engine::scene::PointLightComponent>(*point_light);
    if (spot_light.has_value()) entity.set<engine::scene::SpotLightComponent>(*spot_light);
    if (camera.has_value()) entity.set<engine::scene::CameraComponent>(*camera);
    if (rigidbody.has_value()) entity.set<engine::physics::RigidBodyComponent>(*rigidbody);
    if (collider.has_value()) entity.set<engine::physics::ColliderComponent>(*collider);
    if (audio_source.has_value()) entity.set<engine::audio::AudioSourceComponent>(*audio_source);
    if (audio_listener.has_value()) entity.set<engine::audio::AudioListenerComponent>(*audio_listener);
    if (script.has_value()) entity.set<engine::scripting::ScriptComponent>(*script);

    if (parent_uuid.is_valid()) {
        engine::scene::Entity parent = scene.find_entity_by_uuid(parent_uuid);
        if (parent.is_valid()) {
            entity.set_parent(parent);
        }
    }

    return entity.get_raw();
}

// --- EntityTransformCommand ---
EntityTransformCommand::EntityTransformCommand(engine::scene::Scene& scene,
                                               engine::assets::UUID entity_uuid,
                                               const engine::scene::TransformComponent& old_transform,
                                               const engine::scene::TransformComponent& new_transform)
    : m_scene(scene), m_uuid(entity_uuid), m_old_transform(old_transform), m_new_transform(new_transform) {}

void EntityTransformCommand::execute() {
    auto entity = m_scene.find_entity_by_uuid(m_uuid);
    if (entity.is_valid()) {
        entity.set<engine::scene::TransformComponent>(m_new_transform);
        if (auto* t = entity.get_mut<engine::scene::TransformComponent>()) {
            t->is_dirty = true;
        }
    }
}

void EntityTransformCommand::undo() {
    auto entity = m_scene.find_entity_by_uuid(m_uuid);
    if (entity.is_valid()) {
        entity.set<engine::scene::TransformComponent>(m_old_transform);
        if (auto* t = entity.get_mut<engine::scene::TransformComponent>()) {
            t->is_dirty = true;
        }
    }
}

// --- EntityCreateCommand ---
EntityCreateCommand::EntityCreateCommand(engine::scene::Scene& scene, const EntitySnapshot& snapshot)
    : m_scene(scene), m_snapshot(snapshot) {}

void EntityCreateCommand::execute() {
    flecs::entity e = m_snapshot.restore(m_scene);
    m_created_entity_id = e.is_valid() ? e.id() : 0;
}

void EntityCreateCommand::undo() {
    auto entity = m_scene.find_entity_by_uuid(m_snapshot.uuid);
    if (entity.is_valid()) {
        m_scene.destroy_entity(entity);
    }
}

// --- EntityDeleteCommand ---
EntityDeleteCommand::EntityDeleteCommand(engine::scene::Scene& scene, flecs::entity entity)
    : m_scene(scene), m_snapshot(EntitySnapshot::capture(entity, scene)) {}

void EntityDeleteCommand::execute() {
    auto entity = m_scene.find_entity_by_uuid(m_snapshot.uuid);
    if (entity.is_valid()) {
        m_scene.destroy_entity(entity);
    }
}

void EntityDeleteCommand::undo() {
    m_snapshot.restore(m_scene);
}

// --- EntityParentCommand ---
EntityParentCommand::EntityParentCommand(engine::scene::Scene& scene,
                                         engine::assets::UUID entity_uuid,
                                         engine::assets::UUID old_parent_uuid,
                                         engine::assets::UUID new_parent_uuid)
    : m_scene(scene),
      m_entity_uuid(entity_uuid),
      m_old_parent_uuid(old_parent_uuid),
      m_new_parent_uuid(new_parent_uuid) {}

void EntityParentCommand::execute() {
    auto entity = m_scene.find_entity_by_uuid(m_entity_uuid);
    if (!entity.is_valid()) return;

    if (m_new_parent_uuid.is_valid()) {
        auto parent = m_scene.find_entity_by_uuid(m_new_parent_uuid);
        if (parent.is_valid()) {
            entity.set_parent(parent);
        }
    } else {
        entity.remove_parent();
    }
}

void EntityParentCommand::undo() {
    auto entity = m_scene.find_entity_by_uuid(m_entity_uuid);
    if (!entity.is_valid()) return;

    if (m_old_parent_uuid.is_valid()) {
        auto parent = m_scene.find_entity_by_uuid(m_old_parent_uuid);
        if (parent.is_valid()) {
            entity.set_parent(parent);
        }
    } else {
        entity.remove_parent();
    }
}

// --- CommandHistory ---
CommandHistory::CommandHistory(size_t max_history)
    : m_max_history(max_history) {}

void CommandHistory::execute_command(std::unique_ptr<IEditorCommand> cmd) {
    if (!cmd) return;

    // Discard redo history beyond current cursor
    if (m_cursor < m_history.size()) {
        m_history.erase(m_history.begin() + m_cursor, m_history.end());
    }

    LOG_INFO("CommandHistory", "Executing command: '{}'", cmd->get_name());
    cmd->execute();

    m_history.push_back(std::move(cmd));
    m_cursor++;

    if (m_history.size() > m_max_history) {
        m_history.pop_front();
        m_cursor--;
    }

    notify_changed();
}

void CommandHistory::push_executed_command(std::unique_ptr<IEditorCommand> cmd) {
    if (!cmd) return;

    // Discard redo history beyond current cursor
    if (m_cursor < m_history.size()) {
        m_history.erase(m_history.begin() + m_cursor, m_history.end());
    }

    LOG_INFO("CommandHistory", "Recorded executed command: '{}'", cmd->get_name());
    m_history.push_back(std::move(cmd));
    m_cursor++;

    if (m_history.size() > m_max_history) {
        m_history.pop_front();
        m_cursor--;
    }

    notify_changed();
}

bool CommandHistory::undo() {
    if (!can_undo()) return false;

    m_cursor--;
    auto& cmd = m_history[m_cursor];
    LOG_INFO("CommandHistory", "Undo: '{}'", cmd->get_name());
    cmd->undo();

    notify_changed();
    return true;
}

bool CommandHistory::redo() {
    if (!can_redo()) return false;

    auto& cmd = m_history[m_cursor];
    LOG_INFO("CommandHistory", "Redo: '{}'", cmd->get_name());
    cmd->execute();
    m_cursor++;

    notify_changed();
    return true;
}

std::string CommandHistory::get_undo_name() const {
    if (can_undo()) {
        return m_history[m_cursor - 1]->get_name();
    }
    return "";
}

std::string CommandHistory::get_redo_name() const {
    if (can_redo()) {
        return m_history[m_cursor]->get_name();
    }
    return "";
}

void CommandHistory::clear() {
    m_history.clear();
    m_cursor = 0;
    notify_changed();
}

void CommandHistory::notify_changed() {
    for (const auto& listener : m_listeners) {
        if (listener) {
            listener(*this);
        }
    }
}

} // namespace editor
