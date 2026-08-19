#pragma once

#include "engine/core/math.h"
#include "engine/scene/scene.h"
#include "engine/ui/editor_camera.h"
#include "editor/selection_context.h"
#include "editor/command_history.h"
#include "editor/debug_draw_pass.h"
#include <vulkan/vulkan.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <string>

namespace editor {

enum class ViewportShadingMode {
    Lit = 0,
    Unlit = 1,
    Wireframe = 2,
    Normals = 3,
    RoughnessMetallic = 4
};

class SceneViewport {
public:
    SceneViewport();
    ~SceneViewport();

    void destroy();
    void set_texture(VkSampler sampler, VkImageView image_view, VkImageLayout layout);
    void render(engine::scene::Scene& scene, SelectionContext& selection, CommandHistory& history, float dt, bool* is_open = nullptr);

    void focus_on_selection(SelectionContext& selection);
    void focus_on_entity(flecs::entity entity);

    engine::ui::EditorCamera& get_camera() { return m_camera; }
    const engine::ui::EditorCamera& get_camera() const { return m_camera; }

    engine::core::Vec2 get_size() const { return m_size; }
    engine::core::Vec2 get_position() const { return m_position; }
    bool is_hovered() const { return m_is_hovered; }
    bool is_focused() const { return m_is_focused; }

    ImGuizmo::OPERATION get_gizmo_operation() const { return m_gizmo_op; }
    void set_gizmo_operation(ImGuizmo::OPERATION op) { m_gizmo_op = op; }

    ImGuizmo::MODE get_gizmo_mode() const { return m_gizmo_mode; }
    void set_gizmo_mode(ImGuizmo::MODE mode) { m_gizmo_mode = mode; }

    bool is_snapping_enabled() const { return m_snap_enabled; }
    void set_snapping_enabled(bool enabled) { m_snap_enabled = enabled; }

    ViewportShadingMode get_shading_mode() const { return m_shading_mode; }
    void set_shading_mode(ViewportShadingMode mode) { m_shading_mode = mode; }

    DebugDrawPass& get_debug_draw_pass() { return m_debug_draw_pass; }
    const DebugDrawPass& get_debug_draw_pass() const { return m_debug_draw_pass; }

private:
    void render_overlay_bar();
    void render_gizmo(engine::scene::Scene& scene, SelectionContext& selection, CommandHistory& history);
    void perform_raycast_picking(engine::scene::Scene& scene, SelectionContext& selection, const ImVec2& mouse_pos_in_viewport);

    engine::ui::EditorCamera m_camera;
    VkDescriptorSet m_texture_descriptor{VK_NULL_HANDLE};

    engine::core::Vec2 m_size{1280.0f, 720.0f};
    engine::core::Vec2 m_position{0.0f, 0.0f};
    bool m_is_hovered{false};
    bool m_is_focused{false};

    ImGuizmo::OPERATION m_gizmo_op{ImGuizmo::TRANSLATE};
    ImGuizmo::MODE m_gizmo_mode{ImGuizmo::WORLD};
    bool m_snap_enabled{false};
    float m_snap_translation{0.5f};
    float m_snap_rotation{45.0f};
    float m_snap_scale{0.5f};

    bool m_is_using_gizmo{false};
    engine::scene::TransformComponent m_gizmo_drag_start_transform{};
    engine::assets::UUID m_gizmo_drag_entity_uuid{};

    ViewportShadingMode m_shading_mode{ViewportShadingMode::Lit};
    DebugDrawPass m_debug_draw_pass;
};

} // namespace editor
