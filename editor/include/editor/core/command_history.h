#pragma once

#include "engine/scene/scene.h"
#include "engine/scene/components.h"
#include "engine/physics/physics_components.h"
#include "engine/audio/audio_components.h"
#include "engine/scripting/script_components.h"
#include <flecs.h>
#include <string>
#include <string_view>
#include <memory>
#include <deque>
#include <vector>
#include <optional>
#include <functional>

namespace editor {

// --- Entity Snapshot Structure for Complete Undo/Redo Restoration ---
struct EntitySnapshot {
    engine::assets::UUID uuid{};
    std::string name{"Entity"};
    engine::assets::UUID parent_uuid{};

    std::optional<engine::scene::TransformComponent> transform;
    std::optional<engine::scene::MeshRendererComponent> mesh_renderer;
    std::optional<engine::scene::DirectionalLightComponent> directional_light;
    std::optional<engine::scene::PointLightComponent> point_light;
    std::optional<engine::scene::SpotLightComponent> spot_light;
    std::optional<engine::scene::CameraComponent> camera;
    std::optional<engine::physics::RigidBodyComponent> rigidbody;
    std::optional<engine::physics::ColliderComponent> collider;
    std::optional<engine::audio::AudioSourceComponent> audio_source;
    std::optional<engine::audio::AudioListenerComponent> audio_listener;
    std::optional<engine::scripting::ScriptComponent> script;

    static EntitySnapshot capture(flecs::entity entity, engine::scene::Scene& scene);
    flecs::entity restore(engine::scene::Scene& scene) const;
};

// --- Base Command Interface ---
class IEditorCommand {
public:
    virtual ~IEditorCommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string get_name() const = 0;
};

// --- 1. Entity Transform Command (Gizmo & Precision Drag) ---
class EntityTransformCommand : public IEditorCommand {
public:
    EntityTransformCommand(engine::scene::Scene& scene,
                           engine::assets::UUID entity_uuid,
                           const engine::scene::TransformComponent& old_transform,
                           const engine::scene::TransformComponent& new_transform);

    void execute() override;
    void undo() override;
    std::string get_name() const override { return "Transform Entity"; }

private:
    engine::scene::Scene& m_scene;
    engine::assets::UUID m_uuid;
    engine::scene::TransformComponent m_old_transform;
    engine::scene::TransformComponent m_new_transform;
};

// --- 2. Entity Creation Command ---
class EntityCreateCommand : public IEditorCommand {
public:
    EntityCreateCommand(engine::scene::Scene& scene, const EntitySnapshot& snapshot);

    void execute() override;
    void undo() override;
    std::string get_name() const override { return "Create Entity"; }

private:
    engine::scene::Scene& m_scene;
    EntitySnapshot m_snapshot;
    uint64_t m_created_entity_id{0};
};

// --- 3. Entity Deletion Command ---
class EntityDeleteCommand : public IEditorCommand {
public:
    EntityDeleteCommand(engine::scene::Scene& scene, flecs::entity entity);

    void execute() override;
    void undo() override;
    std::string get_name() const override { return "Delete Entity"; }

private:
    engine::scene::Scene& m_scene;
    EntitySnapshot m_snapshot;
};

// --- 4. Entity Reparenting Command ---
class EntityParentCommand : public IEditorCommand {
public:
    EntityParentCommand(engine::scene::Scene& scene,
                        engine::assets::UUID entity_uuid,
                        engine::assets::UUID old_parent_uuid,
                        engine::assets::UUID new_parent_uuid);

    void execute() override;
    void undo() override;
    std::string get_name() const override { return "Reparent Entity"; }

private:
    engine::scene::Scene& m_scene;
    engine::assets::UUID m_entity_uuid;
    engine::assets::UUID m_old_parent_uuid;
    engine::assets::UUID m_new_parent_uuid;
};

// --- 5. Generic Custom Lambda Command ---
class CustomEditorCommand : public IEditorCommand {
public:
    CustomEditorCommand(std::string name, std::function<void()> do_action, std::function<void()> undo_action)
        : m_name(std::move(name)), m_do(std::move(do_action)), m_undo(std::move(undo_action)) {}

    void execute() override { if (m_do) m_do(); }
    void undo() override { if (m_undo) m_undo(); }
    std::string get_name() const override { return m_name; }

private:
    std::string m_name;
    std::function<void()> m_do;
    std::function<void()> m_undo;
};

// --- Command History Manager ---
class CommandHistory {
public:
    using HistoryChangedCallback = std::function<void(const CommandHistory&)>;

    explicit CommandHistory(size_t max_history = 100);
    ~CommandHistory() = default;

    void execute_command(std::unique_ptr<IEditorCommand> cmd);
    void push_executed_command(std::unique_ptr<IEditorCommand> cmd);

    bool undo();
    bool redo();

    bool can_undo() const { return m_cursor > 0; }
    bool can_redo() const { return m_cursor < m_history.size(); }

    std::string get_undo_name() const;
    std::string get_redo_name() const;

    void clear();
    size_t size() const { return m_history.size(); }
    size_t get_cursor() const { return m_cursor; }

    void add_listener(HistoryChangedCallback callback) {
        m_listeners.push_back(std::move(callback));
    }

private:
    void notify_changed();

    std::deque<std::unique_ptr<IEditorCommand>> m_history;
    size_t m_cursor{0};
    size_t m_max_history{100};
    std::vector<HistoryChangedCallback> m_listeners;
};

} // namespace editor
