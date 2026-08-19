#include "editor/panels/scene_viewport.h"
#include "editor/assets/prefab_manager.h"
#include "editor/assets/asset_importer.h"
#include "engine/scene/components.h"
#include "engine/physics/physics_system.h"
#include "engine/physics/physics_components.h"
#include "engine/core/log.h"
#include <imgui_impl_vulkan.h>
#include <format>
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace editor {

// Helper for Ray - Sphere intersection
static bool ray_intersects_sphere(const engine::core::Ray& ray, const engine::core::Vec3& center, float radius, float& out_t) {
    engine::core::Vec3 oc = ray.origin - center;
    float b = 2.0f * (oc.x * ray.direction.x + oc.y * ray.direction.y + oc.z * ray.direction.z);
    float c = (oc.x * oc.x + oc.y * oc.y + oc.z * oc.z) - radius * radius;
    float discriminant = b * b - 4.0f * c;
    if (discriminant < 0.0f) return false;
    float t = (-b - std::sqrt(discriminant)) / 2.0f;
    if (t < 0.0f) {
        t = (-b + std::sqrt(discriminant)) / 2.0f;
    }
    if (t >= 0.0f) {
        out_t = t;
        return true;
    }
    return false;
}

// Helper for Ray - AABB intersection
static bool ray_intersects_aabb(const engine::core::Ray& ray, const engine::core::Vec3& min_bounds, const engine::core::Vec3& max_bounds, float& out_t) {
    float t1 = (min_bounds.x - ray.origin.x) / (std::abs(ray.direction.x) > 1e-6f ? ray.direction.x : 1e-6f);
    float t2 = (max_bounds.x - ray.origin.x) / (std::abs(ray.direction.x) > 1e-6f ? ray.direction.x : 1e-6f);
    float t3 = (min_bounds.y - ray.origin.y) / (std::abs(ray.direction.y) > 1e-6f ? ray.direction.y : 1e-6f);
    float t4 = (max_bounds.y - ray.origin.y) / (std::abs(ray.direction.y) > 1e-6f ? ray.direction.y : 1e-6f);
    float t5 = (min_bounds.z - ray.origin.z) / (std::abs(ray.direction.z) > 1e-6f ? ray.direction.z : 1e-6f);
    float t6 = (max_bounds.z - ray.origin.z) / (std::abs(ray.direction.z) > 1e-6f ? ray.direction.z : 1e-6f);

    float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
    float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

    if (tmax < 0.0f || tmin > tmax) return false;
    out_t = (tmin < 0.0f) ? tmax : tmin;
    return true;
}

SceneViewport::SceneViewport() = default;

SceneViewport::~SceneViewport() {
    destroy();
}

void SceneViewport::destroy() {
    if (m_texture_descriptor != VK_NULL_HANDLE) {
        if (ImGui::GetCurrentContext() != nullptr) {
            ImGui_ImplVulkan_RemoveTexture(m_texture_descriptor);
        }
        m_texture_descriptor = VK_NULL_HANDLE;
    }
}

void SceneViewport::set_texture(VkSampler sampler, VkImageView image_view, VkImageLayout layout) {
    if (m_texture_descriptor != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(m_texture_descriptor);
        m_texture_descriptor = VK_NULL_HANDLE;
    }

    if (image_view != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE) {
        m_texture_descriptor = ImGui_ImplVulkan_AddTexture(sampler, image_view, layout);
    }
}

void SceneViewport::focus_on_entity(flecs::entity entity) {
    if (!entity.is_valid() || !entity.is_alive()) return;

    if (entity.has<engine::scene::TransformComponent>()) {
        const auto& trans = entity.get<engine::scene::TransformComponent>();
        float radius = std::max({ trans.scale.x, trans.scale.y, trans.scale.z, 1.0f }) * 2.5f;
        m_camera.set_focal_point(trans.position);
        m_camera.set_distance(std::clamp(radius, 2.0f, 50.0f));
        LOG_INFO("Viewport", "Focused camera on entity '{}'", entity.name().c_str());
    }
}

void SceneViewport::focus_on_selection(SelectionContext& selection) {
    if (selection.has_selection()) {
        focus_on_entity(selection.get_primary());
    }
}

void SceneViewport::render_overlay_bar() {
    ImGui::SetCursorPos(ImVec2(12.0f, 12.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.09f, 0.12f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.28f, 0.35f, 0.60f));

    if (ImGui::BeginChild("##ViewportOverlayBar", ImVec2(560.0f, 36.0f), true, ImGuiWindowFlags_NoScrollbar)) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

        // 1. Shading Mode
        const char* shading_modes[] = { " Lit (PBR) ", " Unlit / Albedo ", " Wireframe ", " Normals ", " Roughness / Metallic " };
        int current_shading = static_cast<int>(m_shading_mode);
        ImGui::SetNextItemWidth(160.0f);
        if (ImGui::Combo("##ShadingMode", &current_shading, shading_modes, IM_ARRAYSIZE(shading_modes))) {
            m_shading_mode = static_cast<ViewportShadingMode>(current_shading);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // 2. Gizmo Mode Toggle (World vs Local)
        if (ImGui::Button(m_gizmo_mode == ImGuizmo::WORLD ? "World" : "Local")) {
            m_gizmo_mode = (m_gizmo_mode == ImGuizmo::WORLD) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // 3. Snapping Toggle
        ImGui::Checkbox("Snap", &m_snap_enabled);
        if (m_snap_enabled) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);
            if (m_gizmo_op == ImGuizmo::ROTATE) {
                ImGui::DragFloat("##SnapRot", &m_snap_rotation, 5.0f, 5.0f, 90.0f, "%.0f°");
            } else {
                ImGui::DragFloat("##SnapPos", &m_snap_translation, 0.1f, 0.1f, 10.0f, "%.1fm");
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // 4. Viewport Resolution
        ImGui::TextColored(ImVec4(0.4f, 0.75f, 1.0f, 0.9f), "%.0f x %.0f", m_size.x, m_size.y);

        ImGui::EndChild();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void SceneViewport::render_gizmo(engine::scene::Scene& scene, SelectionContext& selection, CommandHistory& history) {
    if (!selection.has_selection() || m_gizmo_op == static_cast<ImGuizmo::OPERATION>(0)) return;

    flecs::entity primary = selection.get_primary();
    if (!primary.is_valid() || !primary.is_alive() || !primary.has<engine::scene::TransformComponent>()) {
        return;
    }

    auto& transform = primary.ensure<engine::scene::TransformComponent>();

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(m_position.x, m_position.y, m_size.x, m_size.y);

    engine::core::Mat4 view = m_camera.get_view_matrix();
    engine::core::Mat4 proj = m_camera.get_projection_matrix(m_size.x / m_size.y);
    engine::core::Mat4 model = transform.get_local_matrix();

    float snap_values[3]{ m_snap_translation, m_snap_translation, m_snap_translation };
    if (m_gizmo_op == ImGuizmo::ROTATE) {
        snap_values[0] = m_snap_rotation;
        snap_values[1] = m_snap_rotation;
        snap_values[2] = m_snap_rotation;
    } else if (m_gizmo_op == ImGuizmo::SCALE) {
        snap_values[0] = m_snap_scale;
        snap_values[1] = m_snap_scale;
        snap_values[2] = m_snap_scale;
    }

    float* snap_ptr = m_snap_enabled ? snap_values : nullptr;

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

    // Capture initial transform before starting gizmo drag
    if (!m_is_using_gizmo && ImGuizmo::IsOver()) {
        m_gizmo_drag_start_transform = transform;
        m_gizmo_drag_entity_uuid = primary.has<engine::scene::UUIDComponent>() 
            ? primary.get<engine::scene::UUIDComponent>().uuid 
            : engine::assets::UUID{};
    }

    if (ImGuizmo::Manipulate(view_matrix, proj_matrix, m_gizmo_op, m_gizmo_mode, model_matrix, nullptr, snap_ptr)) {
        if (!m_is_using_gizmo) {
            m_is_using_gizmo = true;
        }

        engine::core::Mat4 updated_model;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                updated_model.cols[c][r] = model_matrix[c * 4 + r];
            }
        }

        engine::core::Vec3 new_pos, new_scale;
        engine::core::Quat new_rot;
        updated_model.decompose(new_pos, new_rot, new_scale);

        transform.position = new_pos;
        transform.rotation = new_rot;
        transform.scale = new_scale;
        transform.is_dirty = true;
    }

    // When gizmo drag finishes, record Undo/Redo command
    if (m_is_using_gizmo && !ImGuizmo::IsUsing()) {
        m_is_using_gizmo = false;
        if (m_gizmo_drag_entity_uuid.is_valid()) {
            history.push_executed_command(std::make_unique<EntityTransformCommand>(
                scene, m_gizmo_drag_entity_uuid, m_gizmo_drag_start_transform, transform
            ));
        }
    }
}

void SceneViewport::perform_raycast_picking(engine::scene::Scene& scene, SelectionContext& selection, const ImVec2& mouse_pos_in_viewport) {
    if (mouse_pos_in_viewport.x < 0.0f || mouse_pos_in_viewport.y < 0.0f ||
        mouse_pos_in_viewport.x > m_size.x || mouse_pos_in_viewport.y > m_size.y) {
        return;
    }

    engine::core::Ray ray = m_camera.screen_pos_to_world_ray(
        engine::core::Vec2(mouse_pos_in_viewport.x, mouse_pos_in_viewport.y),
        m_size
    );

    uint64_t hit_entity_id = 0;
    float closest_t = 10000.0f;

    // 1. Physics Raycast via Jolt
    engine::physics::RaycastHit hit;
    if (engine::physics::PhysicsSystem::instance().raycast(ray.origin, ray.direction, 1000.0f, hit)) {
        scene.get_world().each([&hit, &hit_entity_id, &closest_t](flecs::entity e, const engine::physics::RigidBodyComponent& rb) {
            if (rb.is_registered && rb.body_id == hit.body_id) {
                hit_entity_id = e.id();
                closest_t = hit.fraction * 1000.0f;
            }
        });
    }

    // 2. Geometry Bounding Box / Sphere Raycast Fallback
    scene.get_world().each([&](flecs::entity e, const engine::scene::TransformComponent& t) {
        if (!e.is_valid() || !e.is_alive()) return;

        float t_hit = 0.0f;
        if (e.has<engine::physics::ColliderComponent>() && 
            e.get<engine::physics::ColliderComponent>().shape_type == engine::physics::ColliderShapeType::Box) {
            const auto& col = e.get<engine::physics::ColliderComponent>();
            engine::core::Vec3 min_b = t.position + col.offset - col.box_half_extents;
            engine::core::Vec3 max_b = t.position + col.offset + col.box_half_extents;
            if (ray_intersects_aabb(ray, min_b, max_b, t_hit)) {
                if (t_hit < closest_t) {
                    closest_t = t_hit;
                    hit_entity_id = e.id();
                }
            }
        } else {
            float radius = std::max({ t.scale.x, t.scale.y, t.scale.z }) * 0.5f;
            if (ray_intersects_sphere(ray, t.position, radius, t_hit)) {
                if (t_hit < closest_t) {
                    closest_t = t_hit;
                    hit_entity_id = e.id();
                }
            }
        }
    });

    if (hit_entity_id != 0) {
        flecs::entity selected = scene.get_world().entity(hit_entity_id);
        bool multi = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
        selection.select(selected, multi);
        LOG_INFO("Viewport", "Raycast selected entity: '{}' (ID: {})", selected.name().c_str(), hit_entity_id);
    } else {
        if (!ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) {
            selection.clear();
        }
    }
}

void SceneViewport::render(engine::scene::Scene& scene, SelectionContext& selection, CommandHistory& history, float dt, bool* is_open) {
    if (is_open && !*is_open) return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::Begin("Viewport", is_open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        m_is_focused = ImGui::IsWindowFocused();
        m_is_hovered = ImGui::IsWindowHovered();

        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x > 0.0f && avail.y > 0.0f) {
            m_size = { avail.x, avail.y };
        }

        ImVec2 win_pos = ImGui::GetWindowPos();
        ImVec2 content_min = ImGui::GetWindowContentRegionMin();
        m_position = { win_pos.x + content_min.x, win_pos.y + content_min.y };

        // 1. Update Camera controls when viewport is hovered
        if (m_is_hovered) {
            ImGuiIO& io = ImGui::GetIO();
            engine::core::Vec2 mouse_delta(io.MouseDelta.x, io.MouseDelta.y);
            bool is_orbiting = ImGui::IsMouseDown(ImGuiMouseButton_Right);
            bool is_panning = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
            float scroll = io.MouseWheel;

            m_camera.on_update(dt, mouse_delta, is_orbiting, is_panning, scroll);

            // Handle 'F' shortcut to focus on selection
            if (ImGui::IsKeyPressed(ImGuiKey_F) && !io.WantTextInput) {
                focus_on_selection(selection);
            }
        }

        // 2. Render Viewport Texture
        if (m_texture_descriptor != VK_NULL_HANDLE) {
            ImGui::Image(reinterpret_cast<ImTextureID>(m_texture_descriptor), avail);
        } else {
            // Elegant background grid surface
            ImVec2 center_text_pos = ImVec2(std::max(10.0f, avail.x * 0.5f - 140.0f), std::max(10.0f, avail.y * 0.5f - 12.0f));
            ImGui::SetCursorPos(center_text_pos);
            ImGui::TextColored(ImVec4(0.40f, 0.50f, 0.60f, 0.70f), "[ 3D Vulkan 1.3 Viewport Surface ]");
        }

        // 3. Render Overlay Bar
        render_overlay_bar();

        // 4. Render Physics & Debug Wireframe Pass
        m_debug_draw_pass.render_debug_overlay(scene, m_camera, m_position, m_size, selection);

        // 5. Render ImGuizmo Manipulator
        render_gizmo(scene, selection, history);

        // 5. Handle Mouse Raycast Picking on Left Click
        if (m_is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) {
            ImVec2 mouse_screen = ImGui::GetMousePos();
            ImVec2 mouse_in_viewport(mouse_screen.x - m_position.x, mouse_screen.y - m_position.y);
            perform_raycast_picking(scene, selection, mouse_in_viewport);
        }

        // 6. Handle Drag-and-Drop from Content Browser into 3D Viewport
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                const char* asset_vpath = static_cast<const char*>(payload->Data);
                std::string vpath_str(asset_vpath);
                LOG_INFO("Viewport", "Dropped asset onto viewport: '{}'", vpath_str);

                std::filesystem::path p(vpath_str);
                std::string stem = p.stem().string();
                std::string ext = p.extension().string();

                if (vpath_str.ends_with(".prefab")) {
                    flecs::entity spawned = PrefabManager::instantiate_prefab(vpath_str, scene, engine::core::Vec3(0.0f, 0.0f, 0.0f));
                    if (spawned.is_valid()) {
                        selection.select(spawned, false);
                        history.push_executed_command(std::make_unique<EntityCreateCommand>(
                            scene, EntitySnapshot::capture(spawned, scene)
                        ));
                    }
                } else if (ext == ".glb" || ext == ".gltf" || ext == ".obj" || ext == ".fbx") {
                    engine::scene::Entity entity = scene.create_entity(stem);
                    entity.set<engine::scene::TransformComponent>(engine::scene::TransformComponent{ .position = engine::core::Vec3(0.0f, 0.0f, 0.0f) });
                    entity.set<engine::scene::MeshRendererComponent>(engine::scene::MeshRendererComponent{ .cast_shadows = true });
                    selection.select(entity.get_raw(), false);
                    history.push_executed_command(std::make_unique<EntityCreateCommand>(
                        scene, EntitySnapshot::capture(entity.get_raw(), scene)
                    ));
                } else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") {
                    engine::scene::Entity entity = scene.create_entity(stem);
                    entity.set<engine::scene::TransformComponent>(engine::scene::TransformComponent{ .position = engine::core::Vec3(0.0f, 0.0f, 0.0f) });
                    entity.set<engine::audio::AudioSourceComponent>(engine::audio::AudioSourceComponent{ .sound_name = p.filename().string() });
                    selection.select(entity.get_raw(), false);
                    history.push_executed_command(std::make_unique<EntityCreateCommand>(
                        scene, EntitySnapshot::capture(entity.get_raw(), scene)
                    ));
                }
            }
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace editor
