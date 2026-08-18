#include "engine/scene/entity.h"
#include "engine/scene/scene.h"

namespace engine::scene {

assets::UUID Entity::get_uuid() const {
    if (const auto* u = get<UUIDComponent>()) {
        return u->uuid;
    }
    return assets::UUID{};
}

std::string Entity::get_name() const {
    if (const auto* tag = get<TagComponent>()) {
        return tag->name;
    }
    return m_entity.name().c_str();
}

void Entity::set_name(std::string_view name) {
    if (auto* tag = get_mut<TagComponent>()) {
        tag->name = name;
    } else {
        set(TagComponent{ .name = std::string(name) });
    }
    m_entity.set_name(std::string(name).c_str());
}

Entity Entity::get_parent() const {
    flecs::entity p = m_entity.parent();
    if (p.is_valid() && p.is_alive()) {
        return Entity(p, m_scene);
    }
    return Entity{};
}

void Entity::set_parent(Entity parent) {
    if (parent.is_valid()) {
        m_entity.child_of(parent.get_raw());
    } else {
        remove_parent();
    }
    if (auto* t = get_mut<TransformComponent>()) {
        t->is_dirty = true;
    }
}

void Entity::remove_parent() {
    flecs::entity p = m_entity.parent();
    if (p.is_valid()) {
        m_entity.remove(flecs::ChildOf, p);
    }
    if (auto* t = get_mut<TransformComponent>()) {
        t->is_dirty = true;
    }
}

std::vector<Entity> Entity::get_children() const {
    std::vector<Entity> children;
    m_entity.children([&](flecs::entity child) {
        children.emplace_back(child, m_scene);
    });
    return children;
}

void Entity::destroy() {
    if (m_scene && is_valid()) {
        m_scene->destroy_entity(*this);
    }
}

} // namespace engine::scene
