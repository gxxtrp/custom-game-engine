#include "editor/outliner_panel.h"
#include "engine/scene/components.h"
#include "engine/physics/physics_components.h"
#include "engine/audio/audio_components.h"
#include "engine/scripting/script_components.h"
#include "engine/core/log.h"
#include <imgui_internal.h>
#include <format>

namespace editor {

void OutlinerPanel::render(engine::scene::Scene& scene, SelectionContext& selection, CommandHistory& history, bool* is_open) {
    if (is_open && !*is_open) return;

    if (ImGui::Begin("Outliner", is_open)) {
        // --- Search Filter & Add Entity Button ---
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 76.0f);
        ImGui::InputTextWithHint("##OutlinerSearch", "Search hierarchy...", m_filter_buffer, sizeof(m_filter_buffer));
        
        ImGui::SameLine();
        if (ImGui::Button("+ Create", ImVec2(-1, 0))) {
            ImGui::OpenPopup("OutlinerTopCreatePopup");
        }

        if (ImGui::BeginPopup("OutlinerTopCreatePopup")) {
            if (ImGui::MenuItem("Empty Entity")) create_entity_preset(scene, selection, history, "EmptyEntity");
            ImGui::Separator();
            if (ImGui::BeginMenu("3D Objects")) {
                if (ImGui::MenuItem("Cube Mesh")) create_entity_preset(scene, selection, history, "Cube");
                if (ImGui::MenuItem("Sphere Mesh")) create_entity_preset(scene, selection, history, "Sphere");
                if (ImGui::MenuItem("Plane Surface")) create_entity_preset(scene, selection, history, "Plane");
                if (ImGui::MenuItem("Cylinder Mesh")) create_entity_preset(scene, selection, history, "Cylinder");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Lighting")) {
                if (ImGui::MenuItem("Directional Light")) create_entity_preset(scene, selection, history, "DirectionalLight");
                if (ImGui::MenuItem("Point Light")) create_entity_preset(scene, selection, history, "PointLight");
                if (ImGui::MenuItem("Spot Light")) create_entity_preset(scene, selection, history, "SpotLight");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Audio")) {
                if (ImGui::MenuItem("Audio Source")) create_entity_preset(scene, selection, history, "AudioSource");
                if (ImGui::MenuItem("Audio Listener")) create_entity_preset(scene, selection, history, "AudioListener");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Gameplay & Scripting")) {
                if (ImGui::MenuItem("Camera Entity")) create_entity_preset(scene, selection, history, "Camera");
                if (ImGui::MenuItem("Player Controller")) create_entity_preset(scene, selection, history, "PlayerController");
                if (ImGui::MenuItem("Physics Dynamic Box")) create_entity_preset(scene, selection, history, "DynamicBox");
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();

        // --- Entity Hierarchy Viewport Area ---
        ImGui::BeginChild("##OutlinerHierarchyArea", ImVec2(0, -26), false, ImGuiWindowFlags_HorizontalScrollbar);

        std::vector<flecs::entity> root_entities;
        scene.get_world().each([&](flecs::entity e, const engine::scene::TagComponent& tag) {
            if (!e.is_valid() || !e.is_alive()) return;

            // An entity is a root if it has no parent
            if (!e.parent().is_valid()) {
                root_entities.push_back(e);
            }
        });

        // Draw Root Nodes recursively
        for (auto root : root_entities) {
            draw_entity_node(root, scene, selection, history);
        }

        // Drop Target on background (Unparents to Root)
        if (ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->Rect(), ImGui::GetID("##OutlinerHierarchyArea"))) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OUTLINER_ENTITY")) {
                uint64_t dragged_id = *static_cast<const uint64_t*>(payload->Data);
                flecs::entity dragged = scene.get_world().entity(dragged_id);
                if (dragged.is_valid() && dragged.is_alive() && dragged.parent().is_valid()) {
                    engine::assets::UUID dragged_uuid = dragged.has<engine::scene::UUIDComponent>()
                        ? dragged.get<engine::scene::UUIDComponent>().uuid
                        : engine::assets::UUID{};
                    engine::assets::UUID old_parent_uuid = dragged.parent().has<engine::scene::UUIDComponent>()
                        ? dragged.parent().get<engine::scene::UUIDComponent>().uuid
                        : engine::assets::UUID{};
                    history.execute_command(std::make_unique<EntityParentCommand>(
                        scene, dragged_uuid, old_parent_uuid, engine::assets::UUID{}
                    ));
                    LOG_INFO("Outliner", "Unparented entity '{}' to root", dragged.name().c_str());
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Right-Click Context Menu on Empty Background
        if (ImGui::BeginPopupContextWindow("OutlinerBackgroundContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            draw_outliner_context_menu(scene, selection, history);
            ImGui::EndPopup();
        }

        // Click on empty space to deselect
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
            selection.clear();
        }

        ImGui::EndChild();

        // --- Bottom Stats Bar ---
        ImGui::Separator();
        ImGui::TextDisabled("%zu entities in scene | %zu selected", 
                            scene.get_entity_count(), 
                            selection.count());

        // --- Deferred Deletion via CommandHistory ---
        if (m_entity_to_delete != 0) {
            flecs::entity e = scene.get_world().entity(m_entity_to_delete);
            if (e.is_valid() && e.is_alive()) {
                if (selection.is_selected(m_entity_to_delete)) {
                    selection.deselect(e);
                }
                history.execute_command(std::make_unique<EntityDeleteCommand>(scene, e));
                LOG_INFO("Outliner", "Deleted entity ID: {}", m_entity_to_delete);
            }
            m_entity_to_delete = 0;
        }
    }
    ImGui::End();
}

void OutlinerPanel::draw_entity_node(flecs::entity entity, engine::scene::Scene& scene, SelectionContext& selection, CommandHistory& history) {
    if (!entity.is_valid() || !entity.is_alive()) return;

    std::string tag_name = entity.has<engine::scene::TagComponent>()
        ? entity.get<engine::scene::TagComponent>().name
        : entity.name().c_str();

    // Check filter match
    bool matches_filter = (m_filter_buffer[0] == '\0') || 
                          (tag_name.find(m_filter_buffer) != std::string::npos);

    // Count children
    size_t child_count = 0;
    entity.children([&](flecs::entity) { child_count++; });
    bool has_children = (child_count > 0);

    if (!matches_filter && !has_children) {
        return;
    }

    bool is_selected = selection.is_selected(entity);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;
    if (!has_children) flags |= ImGuiTreeNodeFlags_Leaf;

    std::string display_label = std::format("{}  (ID: {})", tag_name, entity.id());
    bool node_open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(entity.id())), flags, "%s", display_label.c_str());

    // 1. Selection logic
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        bool multi = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
        if (multi && is_selected) {
            selection.deselect(entity);
        } else {
            selection.select(entity, multi);
        }
    }

    // 2. Drag Source (Parenting)
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        uint64_t eid = entity.id();
        ImGui::SetDragDropPayload("OUTLINER_ENTITY", &eid, sizeof(uint64_t));
        ImGui::Text("Move '%s'", tag_name.c_str());
        ImGui::EndDragDropSource();
    }

    // 3. Drop Target (Reparent)
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OUTLINER_ENTITY")) {
            uint64_t dragged_id = *static_cast<const uint64_t*>(payload->Data);
            if (dragged_id != entity.id()) {
                flecs::entity dragged = entity.world().entity(dragged_id);
                if (dragged.is_valid() && dragged.is_alive()) {
                    engine::assets::UUID dragged_uuid = dragged.has<engine::scene::UUIDComponent>()
                        ? dragged.get<engine::scene::UUIDComponent>().uuid
                        : engine::assets::UUID{};
                    engine::assets::UUID old_parent_uuid = dragged.parent().is_valid() && dragged.parent().has<engine::scene::UUIDComponent>()
                        ? dragged.parent().get<engine::scene::UUIDComponent>().uuid
                        : engine::assets::UUID{};
                    engine::assets::UUID new_parent_uuid = entity.has<engine::scene::UUIDComponent>()
                        ? entity.get<engine::scene::UUIDComponent>().uuid
                        : engine::assets::UUID{};
                    history.execute_command(std::make_unique<EntityParentCommand>(
                        scene, dragged_uuid, old_parent_uuid, new_parent_uuid
                    ));
                    LOG_INFO("Outliner", "Reparented '{}' under '{}'", dragged.name().c_str(), tag_name);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // 4. Entity Context Menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Create Child Entity")) {
            create_entity_preset(scene, selection, history, "EmptyEntity", entity);
        }
        if (ImGui::BeginMenu("Create Child 3D Object")) {
            if (ImGui::MenuItem("Cube")) create_entity_preset(scene, selection, history, "Cube", entity);
            if (ImGui::MenuItem("Sphere")) create_entity_preset(scene, selection, history, "Sphere", entity);
            if (ImGui::MenuItem("Plane")) create_entity_preset(scene, selection, history, "Plane", entity);
            if (ImGui::MenuItem("Cylinder")) create_entity_preset(scene, selection, history, "Cylinder", entity);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
            engine::scene::Entity copy = scene.create_entity(tag_name + "_Copy");
            if (entity.has<engine::scene::TransformComponent>()) {
                copy.set<engine::scene::TransformComponent>(entity.get<engine::scene::TransformComponent>());
            }
            if (entity.has<engine::scene::MeshRendererComponent>()) {
                copy.set<engine::scene::MeshRendererComponent>(entity.get<engine::scene::MeshRendererComponent>());
            }
            if (entity.has<engine::scene::DirectionalLightComponent>()) {
                copy.set<engine::scene::DirectionalLightComponent>(entity.get<engine::scene::DirectionalLightComponent>());
            }
            if (entity.has<engine::scene::PointLightComponent>()) {
                copy.set<engine::scene::PointLightComponent>(entity.get<engine::scene::PointLightComponent>());
            }
            if (entity.has<engine::scene::CameraComponent>()) {
                copy.set<engine::scene::CameraComponent>(entity.get<engine::scene::CameraComponent>());
            }
            if (entity.parent().is_valid()) {
                copy.get_raw().child_of(entity.parent());
            }
            selection.select(copy.get_raw(), false);
            history.push_executed_command(std::make_unique<EntityCreateCommand>(
                scene, EntitySnapshot::capture(copy.get_raw(), scene)
            ));
        }
        if (entity.parent().is_valid() && ImGui::MenuItem("Unparent (Move to Root)")) {
            engine::assets::UUID entity_uuid = entity.has<engine::scene::UUIDComponent>()
                ? entity.get<engine::scene::UUIDComponent>().uuid
                : engine::assets::UUID{};
            engine::assets::UUID old_parent_uuid = entity.parent().has<engine::scene::UUIDComponent>()
                ? entity.parent().get<engine::scene::UUIDComponent>().uuid
                : engine::assets::UUID{};
            history.execute_command(std::make_unique<EntityParentCommand>(
                scene, entity_uuid, old_parent_uuid, engine::assets::UUID{}
            ));
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete", "Delete")) {
            m_entity_to_delete = entity.id();
        }
        ImGui::EndPopup();
    }

    // 5. Recursive Children Traversal
    if (node_open) {
        entity.children([&](flecs::entity child) {
            draw_entity_node(child, scene, selection, history);
        });
        ImGui::TreePop();
    }
}

void OutlinerPanel::draw_outliner_context_menu(engine::scene::Scene& scene, SelectionContext& selection, CommandHistory& history) {
    if (ImGui::MenuItem("Create Empty Entity")) {
        create_entity_preset(scene, selection, history, "EmptyEntity");
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("3D Objects")) {
        if (ImGui::MenuItem("Cube")) create_entity_preset(scene, selection, history, "Cube");
        if (ImGui::MenuItem("Sphere")) create_entity_preset(scene, selection, history, "Sphere");
        if (ImGui::MenuItem("Plane")) create_entity_preset(scene, selection, history, "Plane");
        if (ImGui::MenuItem("Cylinder")) create_entity_preset(scene, selection, history, "Cylinder");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Lights")) {
        if (ImGui::MenuItem("Directional Light")) create_entity_preset(scene, selection, history, "DirectionalLight");
        if (ImGui::MenuItem("Point Light")) create_entity_preset(scene, selection, history, "PointLight");
        if (ImGui::MenuItem("Spot Light")) create_entity_preset(scene, selection, history, "SpotLight");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Audio")) {
        if (ImGui::MenuItem("Audio Source")) create_entity_preset(scene, selection, history, "AudioSource");
        if (ImGui::MenuItem("Audio Listener")) create_entity_preset(scene, selection, history, "AudioListener");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Camera")) {
        if (ImGui::MenuItem("Perspective Camera")) create_entity_preset(scene, selection, history, "Camera");
        ImGui::EndMenu();
    }
}

void OutlinerPanel::create_entity_preset(engine::scene::Scene& scene, SelectionContext& selection, CommandHistory& history, std::string_view preset, flecs::entity parent) {
    using namespace engine::core;
    using namespace engine::scene;
    using namespace engine::physics;
    using namespace engine::audio;
    using namespace engine::scripting;

    Entity entity;
    if (preset == "Cube") {
        entity = scene.create_entity("Cube");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 0.5f, 0.0f) });
        entity.set<MeshRendererComponent>(MeshRendererComponent{ .cast_shadows = true });
        entity.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Dynamic, .mass = 1.0f });
        entity.set<ColliderComponent>(ColliderComponent{ .shape_type = ColliderShapeType::Box });
    } else if (preset == "Sphere") {
        entity = scene.create_entity("Sphere");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 0.5f, 0.0f) });
        entity.set<MeshRendererComponent>(MeshRendererComponent{ .cast_shadows = true });
        entity.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Dynamic, .mass = 1.0f });
        entity.set<ColliderComponent>(ColliderComponent{ .shape_type = ColliderShapeType::Sphere, .radius = 0.5f });
    } else if (preset == "Plane") {
        entity = scene.create_entity("GroundPlane");
        entity.set<TransformComponent>(TransformComponent{ 
            .position = Vec3(0.0f, -0.5f, 0.0f),
            .scale = Vec3(20.0f, 1.0f, 20.0f)
        });
        entity.set<MeshRendererComponent>(MeshRendererComponent{ .cast_shadows = false });
        entity.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Static });
        entity.set<ColliderComponent>(ColliderComponent{ 
            .shape_type = ColliderShapeType::Box,
            .box_half_extents = Vec3(10.0f, 0.5f, 10.0f)
        });
    } else if (preset == "Cylinder") {
        entity = scene.create_entity("Cylinder");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 1.0f, 0.0f) });
        entity.set<MeshRendererComponent>(MeshRendererComponent{ .cast_shadows = true });
        entity.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Dynamic, .mass = 2.0f });
        entity.set<ColliderComponent>(ColliderComponent{ .shape_type = ColliderShapeType::Cylinder, .radius = 0.5f, .half_height = 1.0f });
    } else if (preset == "DirectionalLight") {
        entity = scene.create_entity("DirectionalLight");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(20.0f, 40.0f, -20.0f) });
        entity.set<DirectionalLightComponent>(DirectionalLightComponent{
            .color = Vec3(1.0f, 0.95f, 0.88f),
            .intensity = 2.0f,
            .cast_shadows = true
        });
    } else if (preset == "PointLight") {
        entity = scene.create_entity("PointLight");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 3.0f, 0.0f) });
        entity.set<PointLightComponent>(PointLightComponent{
            .color = Vec3(0.4f, 0.8f, 1.0f),
            .intensity = 5.0f,
            .radius = 12.0f
        });
    } else if (preset == "SpotLight") {
        entity = scene.create_entity("SpotLight");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 5.0f, 0.0f) });
        entity.set<SpotLightComponent>(SpotLightComponent{
            .color = Vec3(1.0f, 1.0f, 0.9f),
            .intensity = 8.0f,
            .range = 20.0f,
            .inner_cone_angle_deg = 20.0f,
            .outer_cone_angle_deg = 35.0f
        });
    } else if (preset == "Camera") {
        entity = scene.create_entity("Camera");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 2.0f, -5.0f) });
        entity.set<CameraComponent>(CameraComponent{ .fov_deg = 60.0f, .is_primary = false });
    } else if (preset == "AudioSource") {
        entity = scene.create_entity("AudioSource");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 0.0f, 0.0f) });
        entity.set<AudioSourceComponent>(AudioSourceComponent{ .sound_name = "sfx_footstep.wav", .volume = 1.0f });
    } else if (preset == "AudioListener") {
        entity = scene.create_entity("AudioListener");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 0.0f, 0.0f) });
        entity.set<AudioListenerComponent>(AudioListenerComponent{ .is_active = true });
    } else if (preset == "PlayerController") {
        entity = scene.create_entity("PlayerController");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 1.5f, 0.0f) });
        entity.set<MeshRendererComponent>(MeshRendererComponent{ .cast_shadows = true });
        entity.set<AudioSourceComponent>(AudioSourceComponent{ .sound_name = "sfx_footstep.wav", .volume = 0.8f });
        entity.set<ScriptComponent>(ScriptComponent{ .class_name = "PlayerController" });
    } else if (preset == "DynamicBox") {
        entity = scene.create_entity("DynamicPhysicsBox");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(2.0f, 5.0f, 0.0f) });
        entity.set<MeshRendererComponent>(MeshRendererComponent{ .cast_shadows = true });
        entity.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Dynamic, .mass = 5.0f });
        entity.set<ColliderComponent>(ColliderComponent{ .shape_type = ColliderShapeType::Box });
    } else {
        entity = scene.create_entity(preset);
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 0.0f, 0.0f) });
    }

    if (parent.is_valid() && parent.is_alive()) {
        entity.get_raw().child_of(parent);
    }

    selection.select(entity.get_raw(), false);
    history.push_executed_command(std::make_unique<EntityCreateCommand>(
        scene, EntitySnapshot::capture(entity.get_raw(), scene)
    ));
    LOG_INFO("Outliner", "Created entity preset '{}' (ID: {})", preset, entity.get_id());
}

} // namespace editor
