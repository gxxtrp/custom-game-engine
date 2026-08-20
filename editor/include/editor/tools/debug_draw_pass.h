#pragma once

#include "engine/scene/scene.h"
#include "engine/core/math.h"
#include "engine/ui/editor_camera.h"
#include "editor/core/selection_context.h"
#include <imgui.h>

namespace editor {

class DebugDrawPass {
public:
    DebugDrawPass() = default;
    ~DebugDrawPass() = default;

    void render_debug_overlay(engine::scene::Scene& scene,
                             const engine::ui::EditorCamera& camera,
                             const engine::core::Vec2& viewport_pos,
                             const engine::core::Vec2& viewport_size,
                             const SelectionContext& selection);

    bool is_physics_wireframe_enabled() const { return m_show_physics_wireframes; }
    void set_physics_wireframe_enabled(bool enabled) { m_show_physics_wireframes = enabled; }

    bool is_light_bounds_enabled() const { return m_show_light_bounds; }
    void set_light_bounds_enabled(bool enabled) { m_show_light_bounds = enabled; }

private:
    void draw_wire_box(ImDrawList* draw_list, 
                       const engine::core::Mat4& vp, 
                       const engine::core::Vec2& vp_pos, 
                       const engine::core::Vec2& vp_size,
                       const engine::core::Vec3& center, 
                       const engine::core::Vec3& half_extents, 
                       const engine::core::Quat& rotation, 
                       ImU32 color);

    void draw_wire_sphere(ImDrawList* draw_list, 
                          const engine::core::Mat4& vp, 
                          const engine::core::Vec2& vp_pos, 
                          const engine::core::Vec2& vp_size,
                          const engine::core::Vec3& center, 
                          float radius, 
                          ImU32 color);

    bool world_to_screen(const engine::core::Vec3& world_pos, 
                         const engine::core::Mat4& vp, 
                         const engine::core::Vec2& vp_pos, 
                         const engine::core::Vec2& vp_size, 
                         ImVec2& out_screen);

    bool m_show_physics_wireframes{true};
    bool m_show_light_bounds{true};
};

} // namespace editor
