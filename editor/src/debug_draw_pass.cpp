#include "editor/debug_draw_pass.h"
#include "engine/scene/components.h"
#include "engine/physics/physics_components.h"
#include <cmath>
#include <numbers>

namespace editor {

bool DebugDrawPass::world_to_screen(const engine::core::Vec3& world_pos, 
                                    const engine::core::Mat4& vp, 
                                    const engine::core::Vec2& vp_pos, 
                                    const engine::core::Vec2& vp_size, 
                                    ImVec2& out_screen) {
    engine::core::Vec4 clip = vp * engine::core::Vec4(world_pos.x, world_pos.y, world_pos.z, 1.0f);
    if (clip.w <= 0.001f) return false;

    float ndc_x = clip.x / clip.w;
    float ndc_y = clip.y / clip.w;

    out_screen.x = vp_pos.x + (ndc_x * 0.5f + 0.5f) * vp_size.x;
    out_screen.y = vp_pos.y + (1.0f - (ndc_y * 0.5f + 0.5f)) * vp_size.y;
    return true;
}

void DebugDrawPass::draw_wire_box(ImDrawList* draw_list, 
                                  const engine::core::Mat4& vp, 
                                  const engine::core::Vec2& vp_pos, 
                                  const engine::core::Vec2& vp_size,
                                  const engine::core::Vec3& center, 
                                  const engine::core::Vec3& half_extents, 
                                  const engine::core::Quat& rotation, 
                                  ImU32 color) {
    engine::core::Vec3 corners[8] = {
        { -half_extents.x, -half_extents.y, -half_extents.z },
        {  half_extents.x, -half_extents.y, -half_extents.z },
        {  half_extents.x,  half_extents.y, -half_extents.z },
        { -half_extents.x,  half_extents.y, -half_extents.z },
        { -half_extents.x, -half_extents.y,  half_extents.z },
        {  half_extents.x, -half_extents.y,  half_extents.z },
        {  half_extents.x,  half_extents.y,  half_extents.z },
        { -half_extents.x,  half_extents.y,  half_extents.z },
    };

    ImVec2 screen_pts[8];
    bool valid[8];

    for (int i = 0; i < 8; ++i) {
        engine::core::Vec3 world_pt = center + rotation.rotate(corners[i]);
        valid[i] = world_to_screen(world_pt, vp, vp_pos, vp_size, screen_pts[i]);
    }

    int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    for (int i = 0; i < 12; ++i) {
        int i0 = edges[i][0];
        int i1 = edges[i][1];
        if (valid[i0] && valid[i1]) {
            draw_list->AddLine(screen_pts[i0], screen_pts[i1], color, 1.5f);
        }
    }
}

void DebugDrawPass::draw_wire_sphere(ImDrawList* draw_list, 
                                     const engine::core::Mat4& vp, 
                                     const engine::core::Vec2& vp_pos, 
                                     const engine::core::Vec2& vp_size,
                                     const engine::core::Vec3& center, 
                                     float radius, 
                                     ImU32 color) {
    constexpr int segments = 16;
    constexpr float step = (2.0f * std::numbers::pi_v<float>) / static_cast<float>(segments);

    // 1. Circle in XZ plane
    for (int i = 0; i < segments; ++i) {
        float a0 = static_cast<float>(i) * step;
        float a1 = static_cast<float>(i + 1) * step;
        engine::core::Vec3 p0 = center + engine::core::Vec3(std::cos(a0) * radius, 0.0f, std::sin(a0) * radius);
        engine::core::Vec3 p1 = center + engine::core::Vec3(std::cos(a1) * radius, 0.0f, std::sin(a1) * radius);
        ImVec2 s0, s1;
        if (world_to_screen(p0, vp, vp_pos, vp_size, s0) && world_to_screen(p1, vp, vp_pos, vp_size, s1)) {
            draw_list->AddLine(s0, s1, color, 1.2f);
        }
    }

    // 2. Circle in XY plane
    for (int i = 0; i < segments; ++i) {
        float a0 = static_cast<float>(i) * step;
        float a1 = static_cast<float>(i + 1) * step;
        engine::core::Vec3 p0 = center + engine::core::Vec3(std::cos(a0) * radius, std::sin(a0) * radius, 0.0f);
        engine::core::Vec3 p1 = center + engine::core::Vec3(std::cos(a1) * radius, std::sin(a1) * radius, 0.0f);
        ImVec2 s0, s1;
        if (world_to_screen(p0, vp, vp_pos, vp_size, s0) && world_to_screen(p1, vp, vp_pos, vp_size, s1)) {
            draw_list->AddLine(s0, s1, color, 1.2f);
        }
    }
}

void DebugDrawPass::render_debug_overlay(engine::scene::Scene& scene,
                                        const engine::ui::EditorCamera& camera,
                                        const engine::core::Vec2& viewport_pos,
                                        const engine::core::Vec2& viewport_size,
                                        const SelectionContext& selection) {
    if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (!draw_list) return;

    engine::core::Mat4 view = camera.get_view_matrix();
    engine::core::Mat4 proj = camera.get_projection_matrix(viewport_size.x / viewport_size.y);
    engine::core::Mat4 vp = proj * view;

    // 1. Draw Physics Colliders Wireframe
    if (m_show_physics_wireframes) {
        scene.get_world().each([&](flecs::entity e, const engine::physics::ColliderComponent& col, const engine::scene::TransformComponent& transform) {
            bool is_selected = selection.is_selected(e);
            ImU32 color = is_selected ? IM_COL32(80, 255, 120, 240) : IM_COL32(40, 200, 80, 140);

            engine::core::Vec3 center = transform.position + transform.rotation.rotate(col.offset);
            engine::core::Quat rot = transform.rotation * col.rotation;

            switch (col.shape_type) {
                case engine::physics::ColliderShapeType::Box: {
                    engine::core::Vec3 half_ext = engine::core::Vec3(
                        col.box_half_extents.x * transform.scale.x,
                        col.box_half_extents.y * transform.scale.y,
                        col.box_half_extents.z * transform.scale.z
                    );
                    draw_wire_box(draw_list, vp, viewport_pos, viewport_size, center, half_ext, rot, color);
                    break;
                }
                case engine::physics::ColliderShapeType::Sphere: {
                    float r = col.radius * std::max({ transform.scale.x, transform.scale.y, transform.scale.z });
                    draw_wire_sphere(draw_list, vp, viewport_pos, viewport_size, center, r, color);
                    break;
                }
                default: {
                    draw_wire_box(draw_list, vp, viewport_pos, viewport_size, center, col.box_half_extents, rot, color);
                    break;
                }
            }
        });
    }

    // 2. Draw Point Light Radius Wireframe
    if (m_show_light_bounds) {
        scene.get_world().each([&](flecs::entity e, const engine::scene::PointLightComponent& light, const engine::scene::TransformComponent& transform) {
            if (selection.is_selected(e)) {
                ImU32 color = IM_COL32(255, 220, 80, 200);
                draw_wire_sphere(draw_list, vp, viewport_pos, viewport_size, transform.position, light.radius, color);
            }
        });
    }
}

} // namespace editor
