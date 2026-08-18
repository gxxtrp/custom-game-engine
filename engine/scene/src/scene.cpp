#include "engine/scene/scene.h"
#include "engine/core/log.h"
#include <functional>

namespace engine::scene {

Scene::Scene(std::string name) : m_name(std::move(name)) {
    register_components();
    register_systems();
}

Scene::~Scene() {
    clear();
}

void Scene::register_components() {
    m_world.component<TagComponent>("TagComponent");
    m_world.component<UUIDComponent>("UUIDComponent");
    m_world.component<TransformComponent>("TransformComponent");
    m_world.component<WorldTransformComponent>("WorldTransformComponent");
    m_world.component<MeshRendererComponent>("MeshRendererComponent");
    m_world.component<DirectionalLightComponent>("DirectionalLightComponent");
    m_world.component<PointLightComponent>("PointLightComponent");
    m_world.component<SpotLightComponent>("SpotLightComponent");
    m_world.component<CameraComponent>("CameraComponent");
}

void Scene::register_systems() {
    // Systems can be registered or ticked in update
}

Entity Scene::create_entity(std::string_view name, assets::UUID uuid) {
    if (!uuid.is_valid()) uuid = assets::UUID::generate();

    flecs::entity e = m_world.entity(std::string(name).c_str())
        .set<TagComponent>({ std::string(name) })
        .set<UUIDComponent>({ uuid })
        .set<TransformComponent>({})
        .set<WorldTransformComponent>({});

    m_uuid_to_entity[uuid] = e;
    return Entity(e, this);
}

Entity Scene::create_child_entity(Entity parent, std::string_view name, assets::UUID uuid) {
    Entity child = create_entity(name, uuid);
    if (parent.is_valid()) {
        child.set_parent(parent);
    }
    return child;
}

void Scene::destroy_entity(Entity entity) {
    if (!entity.is_valid()) return;

    // Destroy children recursively
    for (auto child : entity.get_children()) {
        destroy_entity(child);
    }

    assets::UUID uuid = entity.get_uuid();
    if (uuid.is_valid()) {
        m_uuid_to_entity.erase(uuid);
    }

    entity.get_raw().destruct();
}

Entity Scene::find_entity_by_uuid(assets::UUID uuid) {
    auto it = m_uuid_to_entity.find(uuid);
    if (it != m_uuid_to_entity.end() && it->second.is_alive()) {
        return Entity(it->second, this);
    }
    return Entity{};
}

Entity Scene::find_entity_by_name(std::string_view name) {
    flecs::entity e = m_world.lookup(std::string(name).c_str());
    if (e.is_valid() && e.is_alive()) {
        return Entity(e, this);
    }

    Entity found{};
    m_world.query_builder<const TagComponent>()
        .build()
        .each([&](flecs::entity entity, const TagComponent& tag) {
            if (tag.name == name) {
                found = Entity(entity, this);
            }
        });

    return found;
}

bool Scene::get_primary_camera(CameraComponent& out_camera, WorldTransformComponent& out_transform, Entity* out_entity) {
    bool found = false;
    m_world.query_builder<const CameraComponent, const WorldTransformComponent>()
        .build()
        .each([&](flecs::entity e, const CameraComponent& cam, const WorldTransformComponent& wt) {
            if (cam.is_primary && !found) {
                out_camera = cam;
                out_transform = wt;
                if (out_entity) *out_entity = Entity(e, this);
                found = true;
            }
        });
    return found;
}

static void propagate_transform(Entity entity, const core::Mat4& parent_matrix, bool parent_dirty) {
    auto* transform = entity.get_mut<TransformComponent>();
    auto* world_transform = entity.get_mut<WorldTransformComponent>();

    if (!transform || !world_transform) return;

    bool is_dirty = transform->is_dirty || parent_dirty;
    if (is_dirty) {
        core::Mat4 local = transform->get_local_matrix();
        world_transform->matrix = parent_matrix * local;
        transform->is_dirty = false;
    }

    for (auto child : entity.get_children()) {
        propagate_transform(child, world_transform->matrix, is_dirty);
    }
}

void Scene::update_transforms() {
    // Propagate from root entities (entities without a parent that has TransformComponent)
    m_world.query_builder<TransformComponent, WorldTransformComponent>()
        .build()
        .each([&](flecs::entity e, TransformComponent& t, WorldTransformComponent& wt) {
            flecs::entity p = e.parent();
            if (!p.is_valid() || !p.has<TransformComponent>()) {
                Entity root(e, this);
                if (t.is_dirty) {
                    wt.matrix = t.get_local_matrix();
                    t.is_dirty = false;
                }
                for (auto child : root.get_children()) {
                    propagate_transform(child, wt.matrix, false);
                }
            }
        });
}

void Scene::update(float dt) {
    m_world.progress(dt);
    update_transforms();
}

void Scene::clear() {
    m_uuid_to_entity.clear();
    m_world.reset();
    register_components();
    register_systems();
}

size_t Scene::get_entity_count() const {
    size_t count = 0;
    const_cast<flecs::world&>(m_world).query_builder<const TagComponent>()
        .build()
        .each([&](flecs::entity, const TagComponent&) {
            count++;
        });
    return count;
}

} // namespace engine::scene
