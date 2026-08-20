#pragma once

#include "engine/core/config.h"
#include "engine/scene/components.h"
#include <flecs.h>
#include <vector>

namespace engine::scene {

class Scene;

class Entity {
public:
    Entity() = default;
    Entity(flecs::entity entity, Scene* scene) : m_entity(entity), m_scene(scene) {}

    template<typename T, typename... Args>
    Entity& emplace(Args&&... args) {
        m_entity.emplace<T>(std::forward<Args>(args)...);
        return *this;
    }

    template<typename T>
    Entity& set(T component) {
        m_entity.set<T>(component);
        return *this;
    }

    template<typename T>
    const T* get() const {
        return m_entity.try_get<T>();
    }

    template<typename T>
    T* get_mut() {
        if (m_entity.has<T>()) {
            return &m_entity.ensure<T>();
        }
        return nullptr;
    }

    template<typename T>
    T& ensure() {
        return m_entity.ensure<T>();
    }

    template<typename T>
    bool has() const {
        return m_entity.has<T>();
    }

    template<typename T>
    Entity& remove() {
        m_entity.remove<T>();
        return *this;
    }

    assets::UUID get_uuid() const;
    std::string get_name() const;
    void set_name(std::string_view name);

    Entity get_parent() const;
    void set_parent(Entity parent);
    void remove_parent();
    std::vector<Entity> get_children() const;

    void destroy();

    bool is_valid() const {
        return m_entity.is_valid() && m_entity.is_alive();
    }

    flecs::entity get_raw() const { return m_entity; }
    Scene* get_scene() const { return m_scene; }

    uint64_t get_id() const { return m_entity.raw_id(); }
    explicit operator bool() const { return is_valid(); }
    bool operator==(const Entity& other) const { return m_entity == other.m_entity && m_scene == other.m_scene; }
    bool operator!=(const Entity& other) const { return !(*this == other); }

private:
    flecs::entity m_entity{};
    Scene* m_scene{nullptr};
};

} // namespace engine::scene
