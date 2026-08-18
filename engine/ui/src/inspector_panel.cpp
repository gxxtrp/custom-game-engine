#include "engine/ui/panels/inspector_panel.h"
#include "engine/ui/editor_ui.h"
#include "engine/scene/components.h"
#include "engine/physics/physics_components.h"
#include "engine/audio/audio_components.h"
#include "engine/scripting/script_components.h"
#include <imgui.h>

namespace engine::ui {

void InspectorPanel::render(scene::Scene& scene) {
    ImGui::Begin("Inspector");

    uint64_t selected_id = EditorUI::instance().get_selected_entity();
    if (selected_id == 0) {
        ImGui::TextDisabled("No entity selected.");
        ImGui::End();
        return;
    }

    flecs::entity e = scene.get_world().entity(selected_id);
    if (!e.is_valid()) {
        ImGui::TextDisabled("Selected entity is no longer valid.");
        ImGui::End();
        return;
    }

    // 1. Tag & UUID
    if (e.has<scene::TagComponent>()) {
        auto& tag = e.ensure<scene::TagComponent>();
        char buffer[256];
        strncpy_s(buffer, tag.name.c_str(), sizeof(buffer));
        if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
            tag.name = buffer;
        }
    }

    ImGui::Separator();

    // 2. Transform Component
    if (e.has<scene::TransformComponent>()) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& t = e.ensure<scene::TransformComponent>();
            float pos[3] = { t.position.x, t.position.y, t.position.z };
            if (ImGui::DragFloat3("Position", pos, 0.1f)) {
                t.position = core::Vec3(pos[0], pos[1], pos[2]);
                t.is_dirty = true;
            }

            float scale[3] = { t.scale.x, t.scale.y, t.scale.z };
            if (ImGui::DragFloat3("Scale", scale, 0.05f, 0.01f, 100.0f)) {
                t.scale = core::Vec3(scale[0], scale[1], scale[2]);
                t.is_dirty = true;
            }
        }
    }

    // 3. RigidBody Component
    if (e.has<physics::RigidBodyComponent>()) {
        if (ImGui::CollapsingHeader("Rigid Body", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& rb = e.ensure<physics::RigidBodyComponent>();
            const char* motions[] = { "Static", "Kinematic", "Dynamic" };
            int motion_idx = static_cast<int>(rb.motion_type);
            if (ImGui::Combo("Motion Type", &motion_idx, motions, 3)) {
                rb.motion_type = static_cast<physics::BodyMotionType>(motion_idx);
            }
            ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.01f, 1000.0f);
            ImGui::SliderFloat("Friction", &rb.friction, 0.0f, 1.0f);
            ImGui::SliderFloat("Restitution", &rb.restitution, 0.0f, 1.0f);
        }
    }

    // 4. Audio Source Component
    if (e.has<audio::AudioSourceComponent>()) {
        if (ImGui::CollapsingHeader("Audio Source", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& audio = e.ensure<audio::AudioSourceComponent>();
            ImGui::SliderFloat("Volume", &audio.volume, 0.0f, 2.0f);
            ImGui::SliderFloat("Pitch", &audio.pitch, 0.1f, 3.0f);
            ImGui::Checkbox("Looping", &audio.is_looping);
            ImGui::Checkbox("3D Spatial", &audio.is_spatial);
        }
    }

    // 5. Script Component
    if (e.has<scripting::ScriptComponent>()) {
        if (ImGui::CollapsingHeader("Script (Lua)", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& sc = e.ensure<scripting::ScriptComponent>();
            ImGui::Text("Class: %s", sc.class_name.c_str());
            ImGui::Text("Initialized: %s", sc.is_initialized ? "Yes" : "No");
        }
    }

    ImGui::End();
}

} // namespace engine::ui
