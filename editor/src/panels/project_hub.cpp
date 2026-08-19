#include "editor/panels/project_hub.h"
#include "engine/core/log.h"
#include "engine/assets/uuid.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include <format>

namespace editor {

bool ProjectHub::create_project_from_template(const std::string& directory, 
                                             const std::string& name, 
                                             ProjectTemplate tmpl) {
    try {
        std::filesystem::path root(directory);
        std::filesystem::create_directories(root / "assets");
        std::filesystem::create_directories(root / "config");
        std::filesystem::create_directories(root / "maps");
        std::filesystem::create_directories(root / "scripts");

        // 1. Write project.toml
        std::string proj_id = engine::assets::UUID::generate().to_string();
        std::ofstream proj_file(root / "project.toml");
        if (!proj_file.is_open()) {
            LOG_ERROR("ProjectHub", "Failed to create project.toml at {}", (root / "project.toml").string());
            return false;
        }

        proj_file << "[display]\n"
                  << "fullscreen = false\n"
                  << "height = 720\n"
                  << "vsync = true\n"
                  << "width = 1280\n\n"
                  << "[physics]\n"
                  << "fixed_timestep = 0.01666666753590107\n"
                  << "gravity = [ 0.0, -9.8100004196167, 0.0 ]\n\n"
                  << "[project]\n"
                  << "default_map = 'maps/default.map'\n"
                  << "engine_version = '1.0.0'\n"
                  << "id = '" << proj_id << "'\n"
                  << "name = '" << name << "'\n"
                  << "version = '0.1.0'\n\n"
                  << "[rendering]\n"
                  << "ray_tracing = true\n"
                  << "shadow_quality = 'high'\n";
        proj_file.close();

        // 2. Write default map
        std::ofstream map_file(root / "maps" / "default.map");
        if (!map_file.is_open()) {
            LOG_ERROR("ProjectHub", "Failed to create default.map at {}", (root / "maps" / "default.map").string());
            return false;
        }

        if (tmpl == ProjectTemplate::Blank3D) {
            map_file << "[map]\n"
                     << "name = 'Blank 3D Level'\n"
                     << "version = '1.0.0'\n\n"
                     << "[[entities]]\n"
                     << "name = 'SunLight'\n"
                     << "uuid = '" << engine::assets::UUID::generate().to_string() << "'\n"
                     << "[entities.directional_light]\n"
                     << "cascade_count = 4\n"
                     << "cast_shadows = true\n"
                     << "color = [1.0, 0.98, 0.92]\n"
                     << "intensity = 1.5\n"
                     << "[entities.transform]\n"
                     << "position = [15.0, 30.0, -15.0]\n"
                     << "rotation = [0.0, 0.0, 0.0, 1.0]\n"
                     << "scale = [1.0, 1.0, 1.0]\n\n"
                     << "[[entities]]\n"
                     << "name = 'MainCamera'\n"
                     << "uuid = '" << engine::assets::UUID::generate().to_string() << "'\n"
                     << "[entities.camera]\n"
                     << "far_z = 1000.0\n"
                     << "fov_deg = 60.0\n"
                     << "is_orthographic = false\n"
                     << "is_primary = true\n"
                     << "near_z = 0.1\n"
                     << "ortho_size = 10.0\n"
                     << "[entities.transform]\n"
                     << "position = [0.0, 2.0, -6.0]\n"
                     << "rotation = [0.0, 0.0, 0.0, 1.0]\n"
                     << "scale = [1.0, 1.0, 1.0]\n\n"
                     << "[[entities]]\n"
                     << "name = 'GroundPlane'\n"
                     << "uuid = '" << engine::assets::UUID::generate().to_string() << "'\n"
                     << "[entities.mesh_renderer]\n"
                     << "cast_shadows = false\n"
                     << "is_visible = true\n"
                     << "material_uuid = '00000000-0000-0000-0000-000000000000'\n"
                     << "mesh_uuid = '00000000-0000-0000-0000-000000000000'\n"
                     << "receive_shadows = true\n"
                     << "submesh_index = 0\n"
                     << "[entities.transform]\n"
                     << "position = [0.0, -0.05, 0.0]\n"
                     << "rotation = [0.0, 0.0, 0.0, 1.0]\n"
                     << "scale = [30.0, 0.1, 30.0]\n";
        } else {
            // Physics Sandbox Template
            map_file << "[map]\n"
                     << "name = 'Physics Sandbox Level'\n"
                     << "version = '1.0.0'\n\n"
                     << "[[entities]]\n"
                     << "name = 'SunLight'\n"
                     << "uuid = '" << engine::assets::UUID::generate().to_string() << "'\n"
                     << "[entities.directional_light]\n"
                     << "cascade_count = 4\n"
                     << "cast_shadows = true\n"
                     << "color = [1.0, 0.98, 0.92]\n"
                     << "intensity = 1.5\n"
                     << "[entities.transform]\n"
                     << "position = [15.0, 30.0, -15.0]\n"
                     << "rotation = [0.0, 0.0, 0.0, 1.0]\n"
                     << "scale = [1.0, 1.0, 1.0]\n\n"
                     << "[[entities]]\n"
                     << "name = 'MainCamera'\n"
                     << "uuid = '" << engine::assets::UUID::generate().to_string() << "'\n"
                     << "[entities.camera]\n"
                     << "far_z = 1000.0\n"
                     << "fov_deg = 60.0\n"
                     << "is_orthographic = false\n"
                     << "is_primary = true\n"
                     << "near_z = 0.1\n"
                     << "ortho_size = 10.0\n"
                     << "[entities.transform]\n"
                     << "position = [0.0, 3.0, -8.0]\n"
                     << "rotation = [0.0, 0.0, 0.0, 1.0]\n"
                     << "scale = [1.0, 1.0, 1.0]\n\n"
                     << "[[entities]]\n"
                     << "name = 'GroundPlane'\n"
                     << "uuid = '" << engine::assets::UUID::generate().to_string() << "'\n"
                     << "[entities.mesh_renderer]\n"
                     << "cast_shadows = false\n"
                     << "is_visible = true\n"
                     << "material_uuid = '00000000-0000-0000-0000-000000000000'\n"
                     << "mesh_uuid = '00000000-0000-0000-0000-000000000000'\n"
                     << "receive_shadows = true\n"
                     << "submesh_index = 0\n"
                     << "[entities.transform]\n"
                     << "position = [0.0, -0.05, 0.0]\n"
                     << "rotation = [0.0, 0.0, 0.0, 1.0]\n"
                     << "scale = [30.0, 0.1, 30.0]\n\n"
                     << "[[entities]]\n"
                     << "name = 'PhysicsCube'\n"
                     << "uuid = '" << engine::assets::UUID::generate().to_string() << "'\n"
                     << "[entities.mesh_renderer]\n"
                     << "cast_shadows = true\n"
                     << "is_visible = true\n"
                     << "material_uuid = '00000000-0000-0000-0000-000000000000'\n"
                     << "mesh_uuid = '00000000-0000-0000-0000-000000000000'\n"
                     << "receive_shadows = true\n"
                     << "submesh_index = 0\n"
                     << "[entities.transform]\n"
                     << "position = [-2.0, 1.0, 0.0]\n"
                     << "rotation = [0.0, 0.0, 0.0, 1.0]\n"
                     << "scale = [1.5, 1.5, 1.5]\n\n"
                     << "[[entities]]\n"
                     << "name = 'PhysicsSphere'\n"
                     << "uuid = '" << engine::assets::UUID::generate().to_string() << "'\n"
                     << "[entities.mesh_renderer]\n"
                     << "cast_shadows = true\n"
                     << "is_visible = true\n"
                     << "material_uuid = '00000000-0000-0000-0000-000000000000'\n"
                     << "mesh_uuid = '00000000-0000-0000-0000-000000000000'\n"
                     << "receive_shadows = true\n"
                     << "submesh_index = 0\n"
                     << "[entities.transform]\n"
                     << "position = [2.0, 1.0, 0.0]\n"
                     << "rotation = [0.0, 0.0, 0.0, 1.0]\n"
                     << "scale = [1.5, 1.5, 1.5]\n\n"
                     << "[[entities]]\n"
                     << "name = 'PlayerCharacter'\n"
                     << "uuid = '" << engine::assets::UUID::generate().to_string() << "'\n"
                     << "[entities.mesh_renderer]\n"
                     << "cast_shadows = true\n"
                     << "is_visible = true\n"
                     << "material_uuid = '00000000-0000-0000-0000-000000000000'\n"
                     << "mesh_uuid = '00000000-0000-0000-0000-000000000000'\n"
                     << "receive_shadows = true\n"
                     << "submesh_index = 0\n"
                     << "[entities.transform]\n"
                     << "position = [0.0, 1.5, 3.0]\n"
                     << "rotation = [0.0, 0.0, 0.0, 1.0]\n"
                     << "scale = [0.8, 1.8, 0.8]\n";
        }
        map_file.close();

        // 3. Write sample player controller script
        std::ofstream script_file(root / "scripts" / "player_controller.lua");
        if (script_file.is_open()) {
            script_file << "-- Player Controller Script\n"
                        << "PlayerController = {}\n"
                        << "PlayerController.__index = PlayerController\n\n"
                        << "function PlayerController.new()\n"
                        << "    local self = setmetatable({}, PlayerController)\n"
                        << "    self.move_speed = 8.0\n"
                        << "    self.jump_force = 12.0\n"
                        << "    return self\n"
                        << "end\n\n"
                        << "function PlayerController:on_create()\n"
                        << "    print('[Lua] PlayerController initialized for ' .. tostring(self))\n"
                        << "end\n\n"
                        << "function PlayerController:on_update(dt)\n"
                        << "    -- Script update logic\n"
                        << "end\n\n"
                        << "function PlayerController:on_destroy()\n"
                        << "    print('[Lua] PlayerController destroyed')\n"
                        << "end\n";
            script_file.close();
        }

        LOG_INFO("ProjectHub", "Successfully created new project '{}' at '{}'", name, directory);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("ProjectHub", "Failed to create project: {}", e.what());
        return false;
    }
}

void ProjectHub::render(EditorPreferences& preferences, bool* is_open, 
                       std::function<void(const std::string& project_path)> on_project_selected) {
    if (is_open && !*is_open) return;

    ImGui::SetNextWindowSize(ImVec2(720, 480), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), 
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("Project Hub", is_open, flags)) {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Modern Game Engine Visual Project Hub");
        ImGui::TextDisabled("Create new projects from templates or resume work on recent projects.");
        ImGui::Separator();
        ImGui::Spacing();

        if (!m_error_message.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
            ImGui::Text("Error: %s", m_error_message.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        if (ImGui::BeginTabBar("ProjectHubTabs", ImGuiTabBarFlags_None)) {
            // ================================================================
            // Tab 1: Recent Projects
            // ================================================================
            if (ImGui::BeginTabItem("Recent Projects")) {
                ImGui::Spacing();
                if (preferences.recent_projects.empty()) {
                    ImGui::TextDisabled("No recent projects found.");
                    ImGui::TextDisabled("Use the 'New Project' tab to create your first game project!");
                } else {
                    ImGui::BeginChild("RecentProjectsList", ImVec2(0, -40), true);
                    for (size_t i = 0; i < preferences.recent_projects.size(); ++i) {
                        const auto& proj = preferences.recent_projects[i];
                        ImGui::PushID(static_cast<int>(i));

                        ImGui::BeginGroup();
                        ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.98f, 1.0f), "%s", proj.name.c_str());
                        ImGui::TextDisabled("%s", proj.path.c_str());
                        ImGui::EndGroup();

                        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 140.0f);
                        if (ImGui::Button("Open Project", ImVec2(100, 26))) {
                            m_error_message = "";
                            if (std::filesystem::exists(proj.path)) {
                                if (on_project_selected) on_project_selected(proj.path);
                                if (is_open) *is_open = false;
                            } else {
                                m_error_message = std::format("Project path does not exist: {}", proj.path);
                            }
                        }

                        ImGui::SameLine();
                        if (ImGui::Button("X", ImVec2(26, 26))) {
                            preferences.recent_projects.erase(preferences.recent_projects.begin() + i);
                            preferences.save_to_file(".engine/editor_preferences.toml");
                            ImGui::PopID();
                            break;
                        }

                        ImGui::Separator();
                        ImGui::PopID();
                    }
                    ImGui::EndChild();
                }
                ImGui::EndTabItem();
            }

            // ================================================================
            // Tab 2: New Project Wizard
            // ================================================================
            if (ImGui::BeginTabItem("New Project")) {
                ImGui::Spacing();

                ImGui::Text("1. Select Template:");
                ImGui::Spacing();

                float card_width = 320.0f;
                float card_height = 80.0f;

                // Card 1: Blank 3D
                bool is_blank = (m_selected_template == ProjectTemplate::Blank3D);
                if (is_blank) {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.32f, 0.48f, 0.8f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.6f));
                }

                if (ImGui::BeginChild("TemplateBlank", ImVec2(card_width, card_height), true)) {
                    if (ImGui::Selectable("Blank 3D Project", is_blank, ImGuiSelectableFlags_None, ImVec2(0, 0))) {
                        m_selected_template = ProjectTemplate::Blank3D;
                    }
                    ImGui::TextDisabled("Minimal level with lighting, camera & ground plane.");
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::SameLine();

                // Card 2: Physics Sandbox
                bool is_sandbox = (m_selected_template == ProjectTemplate::PhysicsSandbox);
                if (is_sandbox) {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.32f, 0.48f, 0.8f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.6f));
                }

                if (ImGui::BeginChild("TemplateSandbox", ImVec2(card_width, card_height), true)) {
                    if (ImGui::Selectable("Physics Sandbox", is_sandbox, ImGuiSelectableFlags_None, ImVec2(0, 0))) {
                        m_selected_template = ProjectTemplate::PhysicsSandbox;
                    }
                    ImGui::TextDisabled("3D physics objects, colliders & player character.");
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Text("2. Project Settings:");
                if (ImGui::InputText("Project Name", m_new_project_name, sizeof(m_new_project_name))) {
                    snprintf(m_new_project_dir, sizeof(m_new_project_dir), "projects/%s", m_new_project_name);
                }

                ImGui::InputText("Location", m_new_project_dir, sizeof(m_new_project_dir));

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Create Project", ImVec2(160, 32))) {
                    m_error_message = "";
                    std::string dir_str = m_new_project_dir;
                    std::string name_str = m_new_project_name;

                    if (dir_str.empty() || name_str.empty()) {
                        m_error_message = "Project name and location cannot be empty.";
                    } else if (std::filesystem::exists(dir_str / std::filesystem::path("project.toml"))) {
                        m_error_message = std::format("A project already exists at '{}'.", dir_str);
                    } else {
                        if (create_project_from_template(dir_str, name_str, m_selected_template)) {
                            preferences.add_recent_project(name_str, dir_str);
                            preferences.save_to_file(".engine/editor_preferences.toml");
                            if (on_project_selected) on_project_selected(dir_str);
                            if (is_open) *is_open = false;
                        } else {
                            m_error_message = "Failed to create project files.";
                        }
                    }
                }
                ImGui::EndTabItem();
            }

            // ================================================================
            // Tab 3: Open Existing Project
            // ================================================================
            if (ImGui::BeginTabItem("Open Project")) {
                ImGui::Spacing();
                ImGui::Text("Enter the directory path of an existing project:");
                ImGui::InputTextWithHint("##OpenProjectPath", "e.g. sandbox_project or projects/MyGame", 
                                         m_open_project_path, sizeof(m_open_project_path));

                ImGui::Spacing();
                if (ImGui::Button("Open", ImVec2(120, 30))) {
                    m_error_message = "";
                    std::string path_str = m_open_project_path;
                    if (path_str.empty()) {
                        m_error_message = "Please enter a valid directory path.";
                    } else if (!std::filesystem::exists(path_str)) {
                        m_error_message = std::format("Path does not exist: {}", path_str);
                    } else {
                        if (on_project_selected) on_project_selected(path_str);
                        if (is_open) *is_open = false;
                    }
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

} // namespace editor
