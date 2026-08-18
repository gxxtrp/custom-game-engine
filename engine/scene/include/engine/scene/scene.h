#pragma once

#include "engine/core/config.h"
#include "engine/scene/components.h"
#include "engine/scene/entity.h"
#include <flecs.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>

namespace engine::scene {

class Scene {
public:
    Scene(std::string name = "Untitled Scene");
    ~Scene();

    Entity create_entity(std::string_view name = "Entity", assets::UUID uuid = assets::UUID::generate());
    Entity create_child_entity(Entity parent, std::string_view name = "Entity", assets::UUID uuid = assets::UUID::generate());
    void destroy_entity(Entity entity);

    Entity find_entity_by_uuid(assets::UUID uuid);
    Entity find_entity_by_name(std::string_view name);

    bool get_primary_camera(CameraComponent& out_camera, WorldTransformComponent& out_transform, Entity* out_entity = nullptr);

    void update(float dt);
    void clear();

    std::string_view get_name() const { return m_name; }
    void set_name(std::string_view name) { m_name = name; }

    flecs::world& get_world() { return m_world; }
    const flecs::world& get_world() const { return m_world; }

    size_t get_entity_count() const;

private:
    void register_components();
    void register_systems();
    void update_transforms();

    std::string m_name;
    flecs::world m_world;
    std::unordered_map<assets::UUID, flecs::entity> m_uuid_to_entity;
};

} // namespace engine::scene
