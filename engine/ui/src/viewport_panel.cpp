#include "engine/ui/panels/viewport_panel.h"
#include "engine/ui/editor_ui.h"
#include "engine/scene/components.h"
#include "engine/physics/physics_system.h"
#include "engine/input/input_manager.h"
#include <imgui_impl_vulkan.h>
#include <ImGuizmo.h>

namespace engine::ui {

ViewportPanel::ViewportPanel() = default;

ViewportPanel::~ViewportPanel() {
    if (m_texture_descriptor != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(m_texture_descriptor);
        m_texture_descriptor = VK_NULL_HANDLE;
    }
}

void ViewportPanel::set_texture(VkSampler sampler, VkImageView image_view, VkImageLayout layout) {
    if (m_texture_descriptor != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(m_texture_descriptor);
        m_texture_descriptor = VK_NULL_HANDLE;
    }

    if (image_view != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE) {
        m_texture_descriptor = ImGui_ImplVulkan_AddTexture(sampler, image_view, layout);
    }
}

void ViewportPanel::render_toolbar() {
    ImGui::SetCursorPos(ImVec2(10.0f, 10.0f));

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.16f, 0.19f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.28f, 0.35f, 0.95f));

    if (ImGui::Button(m_gizmo_op == GizmoOperation::Translate ? "[T] Translate" : "Translate")) {
        m_gizmo_op = GizmoOperation::Translate;
    }
    ImGui::SameLine();
    if (ImGui::Button(m_gizmo_op == GizmoOperation::Rotate ? "[R] Rotate" : "Rotate")) {
        m_gizmo_op = GizmoOperation::Rotate;
    }
    ImGui::SameLine();
    if (ImGui::Button(m_gizmo_op == GizmoOperation::Scale ? "[S] Scale" : "Scale")) {
        m_gizmo_op = GizmoOperation::Scale;
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("|");
    ImGui::SameLine();

    if (ImGui::Button(m_gizmo_mode == GizmoMode::World ? "World Space" : "Local Space")) {
        m_gizmo_mode = (m_gizmo_mode == GizmoMode::World) ? GizmoMode::Local : GizmoMode::World;
    }

    ImGui::SameLine();
    if (ImGui::Checkbox("Snap", &m_snap_enabled)) {
        // Toggle snap
    }

    ImGui::PopStyleColor(2);
}

void ViewportPanel::render_gizmo(scene::Scene& scene) {
    uint64_t selected_id = EditorUI::instance().get_selected_entity();
    if (selected_id == 0) return;

    flecs::entity e = scene.get_world().entity(selected_id);
    if (!e.is_valid() || !e.has<scene::TransformComponent>()) return;

    auto& transform = e.ensure<scene::TransformComponent>();

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(m_position.x, m_position.y, m_size.x, m_size.y);

    core::Mat4 view = m_camera.get_view_matrix();
    core::Mat4 proj = m_camera.get_projection_matrix(m_size.x / m_size.y);

    // Matrix to transform
    core::Mat4 model = transform.get_local_matrix();

    // Map Gizmo Operation
    ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
    if (m_gizmo_op == GizmoOperation::Rotate) op = ImGuizmo::ROTATE;
    else if (m_gizmo_op == GizmoOperation::Scale) op = ImGuizmo::SCALE;
    else if (m_gizmo_op == GizmoOperation::Universal) op = ImGuizmo::UNIVERSAL;

    ImGuizmo::MODE mode = (m_gizmo_mode == GizmoMode::World) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

    float* snap = m_snap_enabled ? m_snap_values : nullptr;
    if (m_gizmo_op == GizmoOperation::Rotate && m_snap_enabled) {
        m_snap_values[0] = 45.0f;
    } else if (m_snap_enabled) {
        m_snap_values[0] = 0.5f;
        m_snap_values[1] = 0.5f;
        m_snap_values[2] = 0.5f;
    }

    // Direct float pointers for ImGuizmo
    float model_matrix[16];
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            model_matrix[c * 4 + r] = model.cols[c][r];
        }
    }

    float view_matrix[16];
    float proj_matrix[16];
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            view_matrix[c * 4 + r] = view.cols[c][r];
            proj_matrix[c * 4 + r] = proj.cols[c][r];
        }
    }

    if (ImGuizmo::Manipulate(view_matrix, proj_matrix, op, mode, model_matrix, nullptr, snap)) {
        core::Mat4 updated_model;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                updated_model.cols[c][r] = model_matrix[c * 4 + r];
            }
        }

        core::Vec3 new_pos, new_scale;
        core::Quat new_rot;
        updated_model.decompose(new_pos, new_rot, new_scale);

        transform.position = new_pos;
        transform.rotation = new_rot;
        transform.scale = new_scale;
        transform.is_dirty = true;
    }
}

void ViewportPanel::handle_mouse_picking(scene::Scene& scene, const ImVec2& mouse_pos_in_viewport) {
    if (mouse_pos_in_viewport.x < 0.0f || mouse_pos_in_viewport.y < 0.0f ||
        mouse_pos_in_viewport.x > m_size.x || mouse_pos_in_viewport.y > m_size.y) {
        return;
    }

    core::Ray ray = m_camera.screen_pos_to_world_ray(
        core::Vec2(mouse_pos_in_viewport.x, mouse_pos_in_viewport.y),
        m_size
    );

    physics::RaycastHit hit;
    if (physics::PhysicsSystem::instance().raycast(ray.origin, ray.direction, 1000.0f, hit)) {
        // Find entity matching hit body
        scene.get_world().each([&hit](flecs::entity e, const physics::RigidBodyComponent& rb) {
            if (rb.is_registered && rb.body_id == hit.body_id) {
                EditorUI::instance().set_selected_entity(e.id());
            }
        });
    }
}

void ViewportPanel::render(scene::Scene* active_scene, float dt) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    m_is_focused = ImGui::IsWindowFocused();
    m_is_hovered = ImGui::IsWindowHovered();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x > 0.0f && avail.y > 0.0f) {
        m_size = { avail.x, avail.y };
    }

    ImVec2 win_pos = ImGui::GetWindowPos();
    ImVec2 content_min = ImGui::GetWindowContentRegionMin();
    m_position = { win_pos.x + content_min.x, win_pos.y + content_min.y };

    // Update Camera based on user interaction
    if (m_is_hovered) {
        ImGuiIO& io = ImGui::GetIO();
        core::Vec2 mouse_delta(io.MouseDelta.x, io.MouseDelta.y);
        bool is_orbiting = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        bool is_panning = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
        float scroll = io.MouseWheel;

        m_camera.on_update(dt, mouse_delta, is_orbiting, is_panning, scroll);
    }

    // Render Viewport Texture
    if (m_texture_descriptor != VK_NULL_HANDLE) {
        ImGui::Image(reinterpret_cast<ImTextureID>(m_texture_descriptor), avail);
    } else {
        ImVec2 center = ImGui::GetCursorScreenPos();
        center.x += avail.x * 0.5f - 80.0f;
        center.y += avail.y * 0.5f - 10.0f;
        ImGui::SetCursorScreenPos(center);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ 3D Scene Viewport ]");
    }

    // Render Toolbar & Gizmo
    render_toolbar();

    if (active_scene) {
        render_gizmo(*active_scene);

        // Mouse picking on left click
        if (m_is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
            ImVec2 mouse_screen = ImGui::GetMousePos();
            ImVec2 mouse_in_viewport(mouse_screen.x - m_position.x, mouse_screen.y - m_position.y);
            handle_mouse_picking(*active_scene, mouse_in_viewport);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace engine::ui
