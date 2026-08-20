#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/ui/editor_camera.h"
#include "engine/scene/scene.h"
#include <vulkan/vulkan.h>
#include <imgui.h>
#include <ImGuizmo.h>

namespace engine::ui {

enum class GizmoOperation {
    Translate,
    Rotate,
    Scale,
    Universal
};

enum class GizmoMode {
    Local,
    World
};

class ViewportPanel {
public:
    ViewportPanel();
    ~ViewportPanel();

    void set_texture(VkSampler sampler, VkImageView image_view, VkImageLayout layout);
    void render(scene::Scene* active_scene = nullptr, float dt = 0.016f);

    EditorCamera& get_camera() { return m_camera; }
    const EditorCamera& get_camera() const { return m_camera; }

    core::Vec2 get_size() const { return m_size; }
    core::Vec2 get_position() const { return m_position; }
    bool is_hovered() const { return m_is_hovered; }
    bool is_focused() const { return m_is_focused; }

    void set_gizmo_operation(GizmoOperation op) { m_gizmo_op = op; }
    void set_gizmo_mode(GizmoMode mode) { m_gizmo_mode = mode; }
    void set_snapping_enabled(bool enabled) { m_snap_enabled = enabled; }

private:
    void render_toolbar();
    void render_gizmo(scene::Scene& scene);
    void handle_mouse_picking(scene::Scene& scene, const ImVec2& mouse_pos_in_viewport);

    EditorCamera m_camera;
    VkDescriptorSet m_texture_descriptor{VK_NULL_HANDLE};

    core::Vec2 m_size{1280.0f, 720.0f};
    core::Vec2 m_position{0.0f, 0.0f};
    bool m_is_hovered{false};
    bool m_is_focused{false};

    GizmoOperation m_gizmo_op{GizmoOperation::Translate};
    GizmoMode m_gizmo_mode{GizmoMode::World};
    bool m_snap_enabled{false};
    float m_snap_values[3]{0.5f, 0.5f, 0.5f};
};

} // namespace engine::ui
