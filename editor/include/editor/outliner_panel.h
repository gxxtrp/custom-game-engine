#pragma once

#include "engine/scene/scene.h"
#include "editor/selection_context.h"
#include <imgui.h>
#include <string>
#include <string_view>
#include <vector>

namespace editor {

class OutlinerPanel {
public:
    OutlinerPanel() = default;
    ~OutlinerPanel() = default;

    void render(engine::scene::Scene& scene, SelectionContext& selection, bool* is_open = nullptr);

private:
    void draw_entity_node(flecs::entity entity, engine::scene::Scene& scene, SelectionContext& selection);
    void draw_outliner_context_menu(engine::scene::Scene& scene, SelectionContext& selection);
    void create_entity_preset(engine::scene::Scene& scene, SelectionContext& selection, std::string_view preset, flecs::entity parent = flecs::entity::null());

    char m_filter_buffer[128]{""};
    uint64_t m_entity_to_delete{0};
};

} // namespace editor
