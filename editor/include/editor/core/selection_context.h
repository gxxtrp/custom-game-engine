#pragma once

#include "engine/scene/entity.h"
#include <flecs.h>
#include <unordered_set>
#include <vector>
#include <functional>

namespace editor {

class SelectionContext {
public:
    using SelectionChangedCallback = std::function<void(const SelectionContext&)>;

    SelectionContext() = default;

    void select(flecs::entity entity, bool multi_select = false);
    void select(engine::scene::Entity entity, bool multi_select = false);
    void deselect(flecs::entity entity);
    void deselect(engine::scene::Entity entity);
    void clear();

    bool is_selected(flecs::entity entity) const;
    bool is_selected(engine::scene::Entity entity) const;
    bool is_selected(uint64_t entity_id) const;

    flecs::entity get_primary() const { return m_primary_entity; }
    uint64_t get_primary_id() const { return m_primary_entity.is_valid() ? m_primary_entity.id() : 0; }
    const std::unordered_set<uint64_t>& get_all_selected() const { return m_selected_entities; }
    size_t count() const { return m_selected_entities.size(); }
    bool has_selection() const { return !m_selected_entities.empty() && m_primary_entity.is_valid(); }

    void add_listener(SelectionChangedCallback callback) {
        m_listeners.push_back(std::move(callback));
    }

private:
    void notify_changed();

    std::unordered_set<uint64_t> m_selected_entities;
    flecs::entity m_primary_entity{flecs::entity::null()};
    std::vector<SelectionChangedCallback> m_listeners;
};

} // namespace editor
