#include "editor/inspector_panel.h"
#include "engine/scene/components.h"
#include "engine/physics/physics_components.h"
#include "engine/audio/audio_components.h"
#include "engine/scripting/script_components.h"
#include "engine/core/log.h"
#include <imgui_internal.h>
#include <format>
#include <cmath>

namespace editor {

// Helper conversion functions for Quaternion <-> Euler angles (Degrees)
static engine::core::Vec3 quat_to_euler_deg(const engine::core::Quat& q) {
    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    float roll = std::atan2(sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    float pitch;
    if (std::abs(sinp) >= 1.0f) {
        pitch = std::copysign(engine::core::math::PI / 2.0f, sinp);
    } else {
        pitch = std::asin(sinp);
    }

    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    float yaw = std::atan2(siny_cosp, cosy_cosp);

    return engine::core::Vec3(
        engine::core::math::rad_to_deg(roll),
        engine::core::math::rad_to_deg(pitch),
        engine::core::math::rad_to_deg(yaw)
    );
}

static engine::core::Quat euler_deg_to_quat(const engine::core::Vec3& deg) {
    return engine::core::Quat::from_euler(
        engine::core::math::deg_to_rad(deg.x),
        engine::core::math::deg_to_rad(deg.y),
        engine::core::math::deg_to_rad(deg.z)
    );
}

void InspectorPanel::render(engine::scene::Scene& scene, SelectionContext& selection, bool* is_open) {
    if (is_open && !*is_open) return;

    if (ImGui::Begin("Inspector", is_open)) {
        if (!selection.has_selection() || selection.count() == 0) {
            ImGui::Spacing();
            ImGui::TextDisabled("  No entity selected.");
            ImGui::TextDisabled("  Select an entity from the Outliner or Viewport to inspect properties.");
            ImGui::End();
            return;
        }

        if (selection.count() == 1) {
            draw_single_entity_inspector(selection.get_primary(), scene, selection);
        } else {
            draw_multi_selection_inspector(scene, selection);
        }
    }
    ImGui::End();
}

bool InspectorPanel::draw_vec3_control(const char* label, engine::core::Vec3& values, float reset_value, float speed, float column_width) {
    bool changed = false;
    ImGui::PushID(label);

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, column_width);
    ImGui::Text("%s", label);
    ImGui::NextColumn();

    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));

    float line_height = ImGui::GetFrameHeight();
    ImVec2 button_size = { line_height + 2.0f, line_height };

    // --- X Axis ---
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.20f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.30f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.70f, 0.15f, 0.15f, 1.0f));
    if (ImGui::Button("X", button_size)) {
        values.x = reset_value;
        changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (ImGui::DragFloat("##X", &values.x, speed, 0.0f, 0.0f, "%.2f")) {
        changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // --- Y Axis ---
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.70f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.85f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.60f, 0.20f, 1.0f));
    if (ImGui::Button("Y", button_size)) {
        values.y = reset_value;
        changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (ImGui::DragFloat("##Y", &values.y, speed, 0.0f, 0.0f, "%.2f")) {
        changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // --- Z Axis ---
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.40f, 0.80f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.55f, 0.95f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.35f, 0.70f, 1.0f));
    if (ImGui::Button("Z", button_size)) {
        values.z = reset_value;
        changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (ImGui::DragFloat("##Z", &values.z, speed, 0.0f, 0.0f, "%.2f")) {
        changed = true;
    }
    ImGui::PopItemWidth();

    ImGui::PopStyleVar();
    ImGui::Columns(1);
    ImGui::PopID();

    return changed;
}

void InspectorPanel::draw_single_entity_inspector(flecs::entity entity, engine::scene::Scene& scene, SelectionContext& selection) {
    if (!entity.is_valid() || !entity.is_alive()) {
        selection.clear();
        return;
    }

    // --- 1. Entity Identity Header (Tag, Enabled, UUID) ---
    bool is_active = true;
    ImGui::Checkbox("##EntityEnabled", &is_active);
    ImGui::SameLine();

    if (entity.has<engine::scene::TagComponent>()) {
        auto& tag = entity.ensure<engine::scene::TagComponent>();
        char name_buf[128];
        strncpy_s(name_buf, tag.name.c_str(), sizeof(name_buf));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
        if (ImGui::InputText("##EntityName", name_buf, sizeof(name_buf))) {
            tag.name = name_buf;
        }
    } else {
        std::string entity_name = entity.name().c_str();
        char name_buf[128];
        strncpy_s(name_buf, entity_name.c_str(), sizeof(name_buf));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
        if (ImGui::InputText("##EntityName", name_buf, sizeof(name_buf))) {
            entity.set<engine::scene::TagComponent>(engine::scene::TagComponent{ .name = name_buf });
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("ID: %llu", (unsigned long long)entity.id());

    std::string uuid_str = entity.has<engine::scene::UUIDComponent>() 
        ? entity.get<engine::scene::UUIDComponent>().uuid.to_string() 
        : "N/A";
    ImGui::TextDisabled("UUID: %s", uuid_str.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy")) {
        ImGui::SetClipboardText(uuid_str.c_str());
    }

    ImGui::Separator();

    // --- 2. Render Attached Component Editors ---
    if (entity.has<engine::scene::TransformComponent>()) draw_transform_editor(entity);
    if (entity.has<engine::scene::MeshRendererComponent>()) draw_mesh_renderer_editor(entity);
    if (entity.has<engine::scene::DirectionalLightComponent>()) draw_directional_light_editor(entity);
    if (entity.has<engine::scene::PointLightComponent>()) draw_point_light_editor(entity);
    if (entity.has<engine::scene::SpotLightComponent>()) draw_spot_light_editor(entity);
    if (entity.has<engine::scene::CameraComponent>()) draw_camera_editor(entity);
    if (entity.has<engine::physics::RigidBodyComponent>()) draw_rigidbody_editor(entity);
    if (entity.has<engine::physics::ColliderComponent>()) draw_collider_editor(entity);
    if (entity.has<engine::audio::AudioSourceComponent>()) draw_audio_source_editor(entity);
    if (entity.has<engine::audio::AudioListenerComponent>()) draw_audio_listener_editor(entity);
    if (entity.has<engine::scripting::ScriptComponent>()) draw_script_editor(entity);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // --- 3. "Add Component" Button & Popup ---
    float avail_width = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button("+ Add Component...", ImVec2(avail_width, 32.0f))) {
        ImGui::OpenPopup("AddComponentSearchPopup");
    }

    draw_add_component_popup(entity);
}

void InspectorPanel::draw_multi_selection_inspector(engine::scene::Scene& scene, SelectionContext& selection) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.18f, 0.24f, 1.0f));
    ImGui::BeginChild("##MultiSelectBanner", ImVec2(0, 48), true);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Multi-Selection Active");
    ImGui::TextDisabled("%zu entities selected for batch modification", selection.count());
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Separator();

    // Batch Transform Editor
    if (ImGui::CollapsingHeader("Transform (Batch)", ImGuiTreeNodeFlags_DefaultOpen)) {
        static engine::core::Vec3 delta_pos{0.0f, 0.0f, 0.0f};
        if (draw_vec3_control("Translate Delta", delta_pos, 0.0f, 0.05f)) {
            for (uint64_t eid : selection.get_all_selected()) {
                flecs::entity e = scene.get_world().entity(eid);
                if (e.is_valid() && e.has<engine::scene::TransformComponent>()) {
                    auto& t = e.ensure<engine::scene::TransformComponent>();
                    t.position += delta_pos;
                    t.is_dirty = true;
                }
            }
            delta_pos = engine::core::Vec3(0.0f, 0.0f, 0.0f);
        }

        static float uniform_scale_mult = 1.0f;
        if (ImGui::DragFloat("Scale Multiplier", &uniform_scale_mult, 0.02f, 0.1f, 10.0f)) {
            for (uint64_t eid : selection.get_all_selected()) {
                flecs::entity e = scene.get_world().entity(eid);
                if (e.is_valid() && e.has<engine::scene::TransformComponent>()) {
                    auto& t = e.ensure<engine::scene::TransformComponent>();
                    t.scale *= uniform_scale_mult;
                    t.is_dirty = true;
                }
            }
            uniform_scale_mult = 1.0f;
        }
    }

    // Batch RigidBody Editor
    if (ImGui::CollapsingHeader("Rigid Body (Batch)", ImGuiTreeNodeFlags_DefaultOpen)) {
        static float batch_mass = 1.0f;
        if (ImGui::DragFloat("Set Mass", &batch_mass, 0.1f, 0.01f, 10000.0f)) {
            for (uint64_t eid : selection.get_all_selected()) {
                flecs::entity e = scene.get_world().entity(eid);
                if (e.is_valid() && e.has<engine::physics::RigidBodyComponent>()) {
                    e.ensure<engine::physics::RigidBodyComponent>().mass = batch_mass;
                }
            }
        }
    }
}

void InspectorPanel::draw_transform_editor(flecs::entity entity) {
    bool header_open = ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen);
    
    if (ImGui::BeginPopupContextItem("TransformContextMenu")) {
        if (ImGui::MenuItem("Reset Transform")) {
            auto& t = entity.ensure<engine::scene::TransformComponent>();
            t.position = engine::core::Vec3(0.0f, 0.0f, 0.0f);
            t.rotation = engine::core::Quat::identity();
            t.scale = engine::core::Vec3(1.0f, 1.0f, 1.0f);
            t.is_dirty = true;
        }
        ImGui::EndPopup();
    }

    if (header_open) {
        auto& trans = entity.ensure<engine::scene::TransformComponent>();

        if (draw_vec3_control("Position", trans.position, 0.0f, 0.1f)) {
            trans.is_dirty = true;
        }

        engine::core::Vec3 euler_deg = quat_to_euler_deg(trans.rotation);
        if (draw_vec3_control("Rotation", euler_deg, 0.0f, 0.5f)) {
            trans.rotation = euler_deg_to_quat(euler_deg);
            trans.is_dirty = true;
        }

        if (draw_vec3_control("Scale", trans.scale, 1.0f, 0.05f)) {
            trans.is_dirty = true;
        }
    }
}

void InspectorPanel::draw_mesh_renderer_editor(flecs::entity entity) {
    bool header_open = ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen);

    if (ImGui::BeginPopupContextItem("MeshRendererContextMenu")) {
        if (ImGui::MenuItem("Remove Mesh Renderer")) {
            entity.remove<engine::scene::MeshRendererComponent>();
        }
        ImGui::EndPopup();
    }

    if (header_open) {
        auto& mr = entity.ensure<engine::scene::MeshRendererComponent>();

        ImGui::Text("Mesh UUID: %s", mr.mesh_uuid.to_string().c_str());
        ImGui::Text("Material UUID: %s", mr.material_uuid.to_string().c_str());

        int submesh = static_cast<int>(mr.submesh_index);
        if (ImGui::DragInt("Submesh Index", &submesh, 1, 0, 128)) {
            mr.submesh_index = static_cast<uint32_t>(submesh);
        }

        ImGui::Checkbox("Cast Shadows", &mr.cast_shadows);
        ImGui::SameLine(0, 16.0f);
        ImGui::Checkbox("Receive Shadows", &mr.receive_shadows);
        ImGui::Checkbox("Is Visible", &mr.is_visible);
    }
}

void InspectorPanel::draw_directional_light_editor(flecs::entity entity) {
    bool header_open = ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen);

    if (ImGui::BeginPopupContextItem("DirectionalLightContextMenu")) {
        if (ImGui::MenuItem("Remove Light")) {
            entity.remove<engine::scene::DirectionalLightComponent>();
        }
        ImGui::EndPopup();
    }

    if (header_open) {
        auto& dl = entity.ensure<engine::scene::DirectionalLightComponent>();

        float col[3] = { dl.color.x, dl.color.y, dl.color.z };
        if (ImGui::ColorEdit3("Light Color", col)) {
            dl.color = engine::core::Vec3(col[0], col[1], col[2]);
        }

        ImGui::DragFloat("Intensity (Lux)", &dl.intensity, 0.1f, 0.0f, 100.0f);
        ImGui::Checkbox("Cast Shadows", &dl.cast_shadows);

        int cascades = static_cast<int>(dl.cascade_count);
        if (ImGui::SliderInt("Shadow Cascades", &cascades, 1, 4)) {
            dl.cascade_count = static_cast<uint32_t>(cascades);
        }
    }
}

void InspectorPanel::draw_point_light_editor(flecs::entity entity) {
    bool header_open = ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen);

    if (ImGui::BeginPopupContextItem("PointLightContextMenu")) {
        if (ImGui::MenuItem("Remove Light")) {
            entity.remove<engine::scene::PointLightComponent>();
        }
        ImGui::EndPopup();
    }

    if (header_open) {
        auto& pl = entity.ensure<engine::scene::PointLightComponent>();

        float col[3] = { pl.color.x, pl.color.y, pl.color.z };
        if (ImGui::ColorEdit3("Light Color", col)) {
            pl.color = engine::core::Vec3(col[0], col[1], col[2]);
        }

        ImGui::DragFloat("Intensity (Lumens)", &pl.intensity, 0.1f, 0.0f, 100.0f);
        ImGui::DragFloat("Radius (m)", &pl.radius, 0.2f, 0.1f, 100.0f);
        ImGui::DragFloat("Falloff", &pl.falloff, 0.05f, 0.0f, 5.0f);
        ImGui::Checkbox("Cast Shadows", &pl.cast_shadows);
    }
}

void InspectorPanel::draw_spot_light_editor(flecs::entity entity) {
    bool header_open = ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen);

    if (ImGui::BeginPopupContextItem("SpotLightContextMenu")) {
        if (ImGui::MenuItem("Remove Light")) {
            entity.remove<engine::scene::SpotLightComponent>();
        }
        ImGui::EndPopup();
    }

    if (header_open) {
        auto& sl = entity.ensure<engine::scene::SpotLightComponent>();

        float col[3] = { sl.color.x, sl.color.y, sl.color.z };
        if (ImGui::ColorEdit3("Light Color", col)) {
            sl.color = engine::core::Vec3(col[0], col[1], col[2]);
        }

        ImGui::DragFloat("Intensity", &sl.intensity, 0.1f, 0.0f, 100.0f);
        ImGui::DragFloat("Range (m)", &sl.range, 0.5f, 0.5f, 200.0f);
        ImGui::SliderFloat("Inner Cone Angle", &sl.inner_cone_angle_deg, 1.0f, 89.0f, "%.1f deg");
        ImGui::SliderFloat("Outer Cone Angle", &sl.outer_cone_angle_deg, sl.inner_cone_angle_deg, 90.0f, "%.1f deg");
        ImGui::Checkbox("Cast Shadows", &sl.cast_shadows);
    }
}

void InspectorPanel::draw_camera_editor(flecs::entity entity) {
    bool header_open = ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen);

    if (ImGui::BeginPopupContextItem("CameraContextMenu")) {
        if (ImGui::MenuItem("Remove Camera")) {
            entity.remove<engine::scene::CameraComponent>();
        }
        ImGui::EndPopup();
    }

    if (header_open) {
        auto& cam = entity.ensure<engine::scene::CameraComponent>();

        const char* proj_types[] = { "Perspective", "Orthographic" };
        int proj_idx = cam.is_orthographic ? 1 : 0;
        if (ImGui::Combo("Projection", &proj_idx, proj_types, 2)) {
            cam.is_orthographic = (proj_idx == 1);
        }

        if (!cam.is_orthographic) {
            ImGui::SliderFloat("Field of View", &cam.fov_deg, 20.0f, 120.0f, "%.1f deg");
        } else {
            ImGui::DragFloat("Orthographic Size", &cam.ortho_size, 0.2f, 0.1f, 1000.0f);
        }

        ImGui::DragFloat("Near Clipping Z", &cam.near_z, 0.01f, 0.001f, 10.0f);
        ImGui::DragFloat("Far Clipping Z", &cam.far_z, 10.0f, 10.0f, 10000.0f);
        ImGui::Checkbox("Is Primary Camera", &cam.is_primary);
    }
}

void InspectorPanel::draw_rigidbody_editor(flecs::entity entity) {
    bool header_open = ImGui::CollapsingHeader("Rigid Body (Jolt Physics)", ImGuiTreeNodeFlags_DefaultOpen);

    if (ImGui::BeginPopupContextItem("RigidBodyContextMenu")) {
        if (ImGui::MenuItem("Remove RigidBody")) {
            entity.remove<engine::physics::RigidBodyComponent>();
        }
        ImGui::EndPopup();
    }

    if (header_open) {
        auto& rb = entity.ensure<engine::physics::RigidBodyComponent>();

        const char* motion_types[] = { "Static", "Kinematic", "Dynamic" };
        int current_motion = static_cast<int>(rb.motion_type);
        if (ImGui::Combo("Motion Type", &current_motion, motion_types, 3)) {
            rb.motion_type = static_cast<engine::physics::BodyMotionType>(current_motion);
        }

        if (rb.motion_type == engine::physics::BodyMotionType::Dynamic) {
            ImGui::DragFloat("Mass (kg)", &rb.mass, 0.1f, 0.01f, 10000.0f);
        }

        ImGui::SliderFloat("Friction", &rb.friction, 0.0f, 1.0f);
        ImGui::SliderFloat("Restitution (Bounciness)", &rb.restitution, 0.0f, 1.0f);
        ImGui::DragFloat("Linear Damping", &rb.linear_damping, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Angular Damping", &rb.angular_damping, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Gravity Factor", &rb.gravity_factor, 0.05f, 0.0f, 5.0f);
        ImGui::Checkbox("Is Sensor / Trigger", &rb.is_sensor);
        ImGui::SameLine(0, 16.0f);
        ImGui::Checkbox("Allow Sleeping", &rb.allow_sleeping);
    }
}

void InspectorPanel::draw_collider_editor(flecs::entity entity) {
    bool header_open = ImGui::CollapsingHeader("Collider (Jolt Physics)", ImGuiTreeNodeFlags_DefaultOpen);

    if (ImGui::BeginPopupContextItem("ColliderContextMenu")) {
        if (ImGui::MenuItem("Remove Collider")) {
            entity.remove<engine::physics::ColliderComponent>();
        }
        ImGui::EndPopup();
    }

    if (header_open) {
        auto& col = entity.ensure<engine::physics::ColliderComponent>();

        const char* shapes[] = { "Box", "Sphere", "Capsule", "Cylinder", "Convex Hull", "Mesh" };
        int shape_idx = static_cast<int>(col.shape_type);
        if (ImGui::Combo("Shape Type", &shape_idx, shapes, 6)) {
            col.shape_type = static_cast<engine::physics::ColliderShapeType>(shape_idx);
        }

        if (col.shape_type == engine::physics::ColliderShapeType::Box) {
            draw_vec3_control("Half Extents", col.box_half_extents, 0.5f, 0.05f);
        } else if (col.shape_type == engine::physics::ColliderShapeType::Sphere) {
            ImGui::DragFloat("Radius", &col.radius, 0.05f, 0.01f, 100.0f);
        } else if (col.shape_type == engine::physics::ColliderShapeType::Capsule || 
                   col.shape_type == engine::physics::ColliderShapeType::Cylinder) {
            ImGui::DragFloat("Radius", &col.radius, 0.05f, 0.01f, 100.0f);
            ImGui::DragFloat("Half Height", &col.half_height, 0.05f, 0.01f, 100.0f);
        }

        draw_vec3_control("Center Offset", col.offset, 0.0f, 0.05f);
    }
}

void InspectorPanel::draw_audio_source_editor(flecs::entity entity) {
    bool header_open = ImGui::CollapsingHeader("Audio Source (miniaudio)", ImGuiTreeNodeFlags_DefaultOpen);

    if (ImGui::BeginPopupContextItem("AudioSourceContextMenu")) {
        if (ImGui::MenuItem("Remove Audio Source")) {
            entity.remove<engine::audio::AudioSourceComponent>();
        }
        ImGui::EndPopup();
    }

    if (header_open) {
        auto& audio = entity.ensure<engine::audio::AudioSourceComponent>();

        char sfx_buf[128];
        strncpy_s(sfx_buf, audio.sound_name.c_str(), sizeof(sfx_buf));
        if (ImGui::InputText("Sound File", sfx_buf, sizeof(sfx_buf))) {
            audio.sound_name = sfx_buf;
        }

        const char* busses[] = { "Master", "SFX", "Music", "Voice", "UI" };
        int bus_idx = static_cast<int>(audio.bus);
        if (ImGui::Combo("Audio Bus", &bus_idx, busses, 5)) {
            audio.bus = static_cast<engine::audio::AudioBus>(bus_idx);
        }

        ImGui::SliderFloat("Volume", &audio.volume, 0.0f, 2.0f, "%.2fx");
        ImGui::SliderFloat("Pitch", &audio.pitch, 0.1f, 3.0f, "%.2fx");
        ImGui::Checkbox("Looping", &audio.is_looping);
        ImGui::SameLine(0, 16.0f);
        ImGui::Checkbox("3D Spatial Audio", &audio.is_spatial);
        ImGui::Checkbox("Play on Start", &audio.play_on_start);

        if (audio.is_spatial) {
            ImGui::DragFloat("Min Distance", &audio.min_distance, 0.2f, 0.1f, 100.0f);
            ImGui::DragFloat("Max Distance", &audio.max_distance, 1.0f, audio.min_distance, 500.0f);
            ImGui::DragFloat("Rolloff Factor", &audio.rolloff_factor, 0.05f, 0.0f, 10.0f);
        }
    }
}

void InspectorPanel::draw_audio_listener_editor(flecs::entity entity) {
    bool header_open = ImGui::CollapsingHeader("Audio Listener", ImGuiTreeNodeFlags_DefaultOpen);

    if (ImGui::BeginPopupContextItem("AudioListenerContextMenu")) {
        if (ImGui::MenuItem("Remove Audio Listener")) {
            entity.remove<engine::audio::AudioListenerComponent>();
        }
        ImGui::EndPopup();
    }

    if (header_open) {
        auto& listener = entity.ensure<engine::audio::AudioListenerComponent>();
        ImGui::Checkbox("Listener Active", &listener.is_active);
    }
}

void InspectorPanel::draw_script_editor(flecs::entity entity) {
    bool header_open = ImGui::CollapsingHeader("Lua Script (sol2)", ImGuiTreeNodeFlags_DefaultOpen);

    if (ImGui::BeginPopupContextItem("ScriptContextMenu")) {
        if (ImGui::MenuItem("Remove Script Component")) {
            entity.remove<engine::scripting::ScriptComponent>();
        }
        ImGui::EndPopup();
    }

    if (header_open) {
        auto& script = entity.ensure<engine::scripting::ScriptComponent>();

        char class_buf[128];
        strncpy_s(class_buf, script.class_name.c_str(), sizeof(class_buf));
        if (ImGui::InputText("Script Class", class_buf, sizeof(class_buf))) {
            script.class_name = class_buf;
        }

        char path_buf[256];
        strncpy_s(path_buf, script.script_path.c_str(), sizeof(path_buf));
        if (ImGui::InputText("Script Path", path_buf, sizeof(path_buf))) {
            script.script_path = path_buf;
        }

        ImGui::TextColored(script.is_initialized ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f) : ImVec4(0.8f, 0.6f, 0.2f, 1.0f),
                           "Status: %s", script.is_initialized ? "Active & Initialized" : "Pending Initialization");

        if (ImGui::Button("Reload Script")) {
            LOG_INFO("Inspector", "Reloading Lua script class: {}", script.class_name);
        }
    }
}

void InspectorPanel::draw_add_component_popup(flecs::entity entity) {
    if (ImGui::BeginPopup("AddComponentSearchPopup")) {
        ImGui::SetNextItemWidth(240.0f);
        ImGui::InputTextWithHint("##ComponentSearchFilter", "Search components...", m_component_search, sizeof(m_component_search));

        ImGui::Separator();

        std::string filter = m_component_search;
        auto matches = [&](std::string_view name) {
            if (filter.empty()) return true;
            std::string name_lower(name);
            std::string filter_lower = filter;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);
            return name_lower.find(filter_lower) != std::string::npos;
        };

        // 1. Rendering Components
        if (matches("Mesh Renderer") && !entity.has<engine::scene::MeshRendererComponent>()) {
            if (ImGui::MenuItem("[Rendering] Mesh Renderer")) {
                entity.emplace<engine::scene::MeshRendererComponent>();
                ImGui::CloseCurrentPopup();
            }
        }
        if (matches("Camera") && !entity.has<engine::scene::CameraComponent>()) {
            if (ImGui::MenuItem("[Rendering] Camera")) {
                entity.emplace<engine::scene::CameraComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        // 2. Lighting Components
        if (matches("Directional Light") && !entity.has<engine::scene::DirectionalLightComponent>()) {
            if (ImGui::MenuItem("[Lighting] Directional Light")) {
                entity.emplace<engine::scene::DirectionalLightComponent>();
                ImGui::CloseCurrentPopup();
            }
        }
        if (matches("Point Light") && !entity.has<engine::scene::PointLightComponent>()) {
            if (ImGui::MenuItem("[Lighting] Point Light")) {
                entity.emplace<engine::scene::PointLightComponent>();
                ImGui::CloseCurrentPopup();
            }
        }
        if (matches("Spot Light") && !entity.has<engine::scene::SpotLightComponent>()) {
            if (ImGui::MenuItem("[Lighting] Spot Light")) {
                entity.emplace<engine::scene::SpotLightComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        // 3. Physics Components
        if (matches("Rigid Body") && !entity.has<engine::physics::RigidBodyComponent>()) {
            if (ImGui::MenuItem("[Physics] Rigid Body")) {
                entity.emplace<engine::physics::RigidBodyComponent>();
                ImGui::CloseCurrentPopup();
            }
        }
        if (matches("Collider") && !entity.has<engine::physics::ColliderComponent>()) {
            if (ImGui::MenuItem("[Physics] Collider")) {
                entity.emplace<engine::physics::ColliderComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        // 4. Audio Components
        if (matches("Audio Source") && !entity.has<engine::audio::AudioSourceComponent>()) {
            if (ImGui::MenuItem("[Audio] Audio Source")) {
                entity.emplace<engine::audio::AudioSourceComponent>();
                ImGui::CloseCurrentPopup();
            }
        }
        if (matches("Audio Listener") && !entity.has<engine::audio::AudioListenerComponent>()) {
            if (ImGui::MenuItem("[Audio] Audio Listener")) {
                entity.emplace<engine::audio::AudioListenerComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        // 5. Scripting Components
        if (matches("Lua Script") && !entity.has<engine::scripting::ScriptComponent>()) {
            if (ImGui::MenuItem("[Scripting] Lua Script Component")) {
                entity.emplace<engine::scripting::ScriptComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }
}

} // namespace editor
