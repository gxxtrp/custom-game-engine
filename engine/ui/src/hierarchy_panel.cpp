#include "engine/ui/panels/hierarchy_panel.h"
#include "engine/ui/editor_ui.h"
#include "engine/scene/components.h"
#include <imgui.h>

namespace engine::ui {

void HierarchyPanel::render(scene::Scene& scene) {
    ImGui::Begin("Scene Hierarchy");

    if (ImGui::Button("+ Add Entity")) {
        scene.create_entity("NewEntity");
    }

    ImGui::Separator();

    uint64_t selected_id = EditorUI::instance().get_selected_entity();
    uint64_t entity_to_delete = 0;

    scene.get_world().each([&](flecs::entity e, const scene::TagComponent& tag) {
        if (!e.is_valid() || !e.is_alive()) return;

        bool is_selected = (e.id() == selected_id);
        ImGuiTreeNodeFlags flags = (is_selected ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        bool opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(e.id())), flags, "%s", tag.name.c_str());

        if (ImGui::IsItemClicked()) {
            EditorUI::instance().set_selected_entity(e.id());
        }

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Entity")) {
                entity_to_delete = e.id();
            }
            ImGui::EndPopup();
        }

        if (opened) {
            ImGui::TreePop();
        }
    });

    // Perform deletion outside the active table iteration
    if (entity_to_delete != 0) {
        if (EditorUI::instance().get_selected_entity() == entity_to_delete) {
            EditorUI::instance().set_selected_entity(0);
        }
        flecs::entity e = scene.get_world().entity(entity_to_delete);
        if (e.is_valid() && e.is_alive()) {
            e.destruct();
        }
    }

    ImGui::End();
}

} // namespace engine::ui
