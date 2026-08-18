#include "editor/selection_context.h"

namespace editor {

void SelectionContext::select(flecs::entity entity, bool multi_select) {
    if (!multi_select) {
        m_selected_entities.clear();
    }

    if (entity.is_valid() && entity.is_alive()) {
        m_selected_entities.insert(entity.id());
        m_primary_entity = entity;
    } else {
        if (!multi_select) {
            m_primary_entity = flecs::entity::null();
        }
    }

    notify_changed();
}

void SelectionContext::select(engine::scene::Entity entity, bool multi_select) {
    select(entity.get_raw(), multi_select);
}

void SelectionContext::deselect(flecs::entity entity) {
    if (!entity.is_valid()) return;

    m_selected_entities.erase(entity.id());
    if (m_primary_entity.id() == entity.id()) {
        if (m_selected_entities.empty()) {
            m_primary_entity = flecs::entity::null();
        } else {
            m_primary_entity = entity.world().entity(*m_selected_entities.begin());
        }
    }

    notify_changed();
}

void SelectionContext::deselect(engine::scene::Entity entity) {
    deselect(entity.get_raw());
}

void SelectionContext::clear() {
    if (m_selected_entities.empty() && !m_primary_entity.is_valid()) return;

    m_selected_entities.clear();
    m_primary_entity = flecs::entity::null();
    notify_changed();
}

bool SelectionContext::is_selected(flecs::entity entity) const {
    if (!entity.is_valid()) return false;
    return m_selected_entities.contains(entity.id());
}

bool SelectionContext::is_selected(engine::scene::Entity entity) const {
    return is_selected(entity.get_raw());
}

bool SelectionContext::is_selected(uint64_t entity_id) const {
    return m_selected_entities.contains(entity_id);
}

void SelectionContext::notify_changed() {
    for (const auto& listener : m_listeners) {
        if (listener) {
            listener(*this);
        }
    }
}

} // namespace editor
