#pragma once

#include "engine/scene/scene.h"
#include "editor/selection_context.h"
#include <imgui.h>
#include <string>

namespace editor {

class InspectorPanel {
public:
    InspectorPanel() = default;
    ~InspectorPanel() = default;

    void render(engine::scene::Scene& scene, SelectionContext& selection, bool* is_open = nullptr);

private:
    void draw_single_entity_inspector(flecs::entity entity, engine::scene::Scene& scene, SelectionContext& selection);
    void draw_multi_selection_inspector(engine::scene::Scene& scene, SelectionContext& selection);
    void draw_add_component_popup(flecs::entity entity);

    // Component section drawers
    void draw_transform_editor(flecs::entity entity);
    void draw_mesh_renderer_editor(flecs::entity entity);
    void draw_directional_light_editor(flecs::entity entity);
    void draw_point_light_editor(flecs::entity entity);
    void draw_spot_light_editor(flecs::entity entity);
    void draw_camera_editor(flecs::entity entity);
    void draw_rigidbody_editor(flecs::entity entity);
    void draw_collider_editor(flecs::entity entity);
    void draw_audio_source_editor(flecs::entity entity);
    void draw_audio_listener_editor(flecs::entity entity);
    void draw_script_editor(flecs::entity entity);

    // Helper widget for Vec3 with colored X, Y, Z buttons
    bool draw_vec3_control(const char* label, engine::core::Vec3& values, float reset_value = 0.0f, float speed = 0.1f, float column_width = 100.0f);

    char m_component_search[64]{""};
};

} // namespace editor
