#include "editor/panels/project_hub.h"
#include "engine/core/log.h"
#include "engine/core/platform.h"
#include "engine/assets/uuid.h"
#include <toml++/toml.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <ctime>
#include <format>
#include <vector>

#if defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>

static std::string platform_browse_folder(const std::string& title = "Select Project Folder") {
    std::string result = "";
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool co_initialized = SUCCEEDED(hr);

    IFileOpenDialog* pFileOpen = nullptr;
    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions))) {
            pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        }

        std::wstring wtitle(title.begin(), title.end());
        pFileOpen->SetTitle(wtitle.c_str());

        if (SUCCEEDED(pFileOpen->Show(NULL))) {
            IShellItem* pItem = nullptr;
            if (SUCCEEDED(pFileOpen->GetResult(&pItem))) {
                PWSTR pszFilePath = nullptr;
                if (SUCCEEDED(pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                    int size_needed = WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, NULL, 0, NULL, NULL);
                    if (size_needed > 0) {
                        result.resize(size_needed - 1);
                        WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, &result[0], size_needed, NULL, NULL);
                    }
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }

    if (co_initialized) {
        CoUninitialize();
    }
    return result;
}
#else
static std::string platform_browse_folder(const std::string& = "Select Project Folder") {
    return "";
}
#endif

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
                     << "name = '" << name << " Blank Level'\n"
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
            // Physics Sandbox
            map_file << "[map]\n"
                     << "name = '" << name << " Physics Level'\n"
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

        // 3. Write default Lua script
        std::ofstream script_file(root / "scripts" / "player_controller.lua");
        if (script_file.is_open()) {
            script_file << "-- " << name << " Starter Player Controller\n"
                        << "PlayerController = {}\n"
                        << "PlayerController.__index = PlayerController\n\n"
                        << "function PlayerController:on_create(entity)\n"
                        << "    Log.info(\"Started PlayerController on \" .. entity:get_name())\n"
                        << "end\n\n"
                        << "function PlayerController:on_update(entity, dt)\n"
                        << "    if Input.is_key_down(\"W\") then\n"
                        << "        entity:translate(0.0, 0.0, 5.0 * dt)\n"
                        << "    end\n"
                        << "end\n";
            script_file.close();
        }

        LOG_INFO("ProjectHub", "Successfully created new project '{}' at '{}'", name, directory);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("ProjectHub", "Exception creating project: {}", e.what());
        return false;
    }
}

void ProjectHub::render(EditorPreferences& preferences, bool* is_open, 
                       std::function<void(const std::string& project_path)> on_project_selected) {
    if (is_open && !*is_open) return;

    ImGui::SetNextWindowSize(ImVec2(800, 540), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Project Hub & Launcher", is_open, flags)) {
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Modern Game Engine: Projects & Templates");
        ImGui::TextDisabled("Create, manage, and open your engine game projects.");
        ImGui::Separator();
        ImGui::Spacing();

        if (!m_error_message.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::Text("Error: %s", m_error_message.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        if (ImGui::BeginTabBar("ProjectHubTabs")) {
            // ================================================================
            // Tab 1: Recent Projects
            // ================================================================
            if (ImGui::BeginTabItem("Recent Projects")) {
                ImGui::Spacing();
                const auto& recents = preferences.recent_projects;

                if (recents.empty()) {
                    ImGui::TextDisabled("No recent projects found.");
                    ImGui::Spacing();
                    ImGui::Text("Switch to the 'New Project' tab to create your first game project!");
                } else {
                    if (ImGui::BeginTable("RecentProjectsTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY, ImVec2(0, 360))) {
                        ImGui::TableSetupColumn("Project Name", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Last Opened", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                        ImGui::TableHeadersRow();

                        for (size_t i = 0; i < recents.size(); ++i) {
                            const auto& proj = recents[i];
                            ImGui::TableNextRow();

                            bool exists = std::filesystem::exists(std::filesystem::path(proj.path) / "project.toml") || 
                                          (std::filesystem::exists(proj.path) && std::filesystem::path(proj.path).extension() == ".toml");
                            
                            std::string display_name = proj.name;
                            if (exists) {
                                std::filesystem::path manifest = std::filesystem::exists(std::filesystem::path(proj.path) / "project.toml")
                                    ? std::filesystem::path(proj.path) / "project.toml"
                                    : std::filesystem::path(proj.path);
                                
                                std::string file_content;
                                if (engine::core::FileSystem::read_file_string(manifest.string(), file_content)) {
                                    try {
                                        auto tbl = toml::parse(file_content);
                                        if (auto* p = tbl["project"].as_table()) {
                                            if (auto* n = (*p)["name"].as_string()) {
                                                display_name = n->get();
                                            }
                                        }
                                    } catch (...) {}
                                }
                            }

                            // Col 1: Name
                            ImGui::TableSetColumnIndex(0);
                            if (exists) {
                                ImGui::TextColored(ImVec4(0.9f, 0.95f, 1.0f, 1.0f), "%s", display_name.c_str());
                            } else {
                                ImGui::TextColored(ImVec4(0.85f, 0.4f, 0.4f, 1.0f), "%s (Missing)", display_name.c_str());
                            }

                            // Col 2: Path
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextDisabled("%s", proj.path.c_str());

                            // Col 3: Date
                            ImGui::TableSetColumnIndex(2);
                            std::time_t t = static_cast<std::time_t>(proj.last_opened_timestamp);
                            char time_buf[64];
                            std::tm tm_buf{};
#if defined(_WIN32)
                            localtime_s(&tm_buf, &t);
#else
                            localtime_r(&t, &tm_buf);
#endif
                            std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M", &tm_buf);
                            ImGui::TextDisabled("%s", time_buf);

                            // Col 4: Open / Remove
                            ImGui::TableSetColumnIndex(3);
                            ImGui::PushID(static_cast<int>(i));
                            if (ImGui::Button("Open", ImVec2(50, 0))) {
                                if (exists) {
                                    if (on_project_selected) on_project_selected(proj.path);
                                    if (is_open) *is_open = false;
                                } else {
                                    m_error_message = std::format("Project path does not exist on disk: '{}'", proj.path);
                                }
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Remove", ImVec2(55, 0))) {
                                preferences.remove_recent_project(proj.path);
                                preferences.save_to_file(".engine/editor_preferences.toml");
                                ImGui::PopID();
                                break;
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                    }
                }
                ImGui::EndTabItem();
            }

            // ================================================================
            // Tab 2: New Project Wizard
            // ================================================================
            if (ImGui::BeginTabItem("New Project")) {
                ImGui::Spacing();
                ImGui::Text("1. Choose a Starter Template:");
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

                ImGui::Text("Location:");
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110.0f);
                ImGui::InputText("##NewProjectLocation", m_new_project_dir, sizeof(m_new_project_dir));
                ImGui::SameLine();
                if (ImGui::Button("Browse...##NewProj", ImVec2(100, 0))) {
                    std::string chosen = platform_browse_folder("Select Parent Directory for New Project");
                    if (!chosen.empty()) {
                        std::filesystem::path p(chosen);
                        if (p.filename().string() != m_new_project_name) {
                            p /= m_new_project_name;
                        }
                        snprintf(m_new_project_dir, sizeof(m_new_project_dir), "%s", p.generic_string().c_str());
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Create Project", ImVec2(160, 32))) {
                    m_error_message = "";
                    std::string dir_str = m_new_project_dir;
                    std::string name_str = m_new_project_name;

                    if (dir_str.empty() || name_str.empty()) {
                        m_error_message = "Project name and location cannot be empty.";
                    } else if (std::filesystem::exists(std::filesystem::path(dir_str) / "project.toml")) {
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
                ImGui::Text("Select or browse to the folder of an existing project:");
                ImGui::Spacing();

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 140.0f);
                ImGui::InputTextWithHint("##OpenProjectPath", "Select or type project folder path...", 
                                         m_open_project_path, sizeof(m_open_project_path));
                ImGui::SameLine();
                if (ImGui::Button("Browse Folder...##OpenProj", ImVec2(130, 0))) {
                    std::string chosen = platform_browse_folder("Select Project Folder to Open");
                    if (!chosen.empty()) {
                        snprintf(m_open_project_path, sizeof(m_open_project_path), "%s", chosen.c_str());
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button("Open Selected Project", ImVec2(170, 32))) {
                    m_error_message = "";
                    std::string path_str = m_open_project_path;
                    if (path_str.empty()) {
                        m_error_message = "Please select or enter a valid directory path.";
                    } else if (!std::filesystem::exists(path_str)) {
                        m_error_message = std::format("Path does not exist: {}", path_str);
                    } else if (!std::filesystem::exists(std::filesystem::path(path_str) / "project.toml")) {
                        m_error_message = std::format("Selected folder does not contain 'project.toml': {}", path_str);
                    } else {
                        if (on_project_selected) on_project_selected(path_str);
                        if (is_open) *is_open = false;
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("Discovered Local Projects:");
                if (ImGui::BeginChild("DiscoveredProjects", ImVec2(0, 160), true)) {
                    std::vector<std::string> search_dirs = { "projects", "." };
                    int proj_count = 0;
                    for (const auto& sdir : search_dirs) {
                        if (!std::filesystem::exists(sdir)) continue;
                        for (const auto& entry : std::filesystem::directory_iterator(sdir)) {
                            if (entry.is_directory()) {
                                std::filesystem::path manifest = entry.path() / "project.toml";
                                if (std::filesystem::exists(manifest)) {
                                    proj_count++;
                                    std::string p_name = entry.path().filename().generic_string();
                                    std::string p_path = entry.path().generic_string();

                                    std::string file_content;
                                    if (engine::core::FileSystem::read_file_string(manifest.string(), file_content)) {
                                        try {
                                            auto tbl = toml::parse(file_content);
                                            if (auto* p = tbl["project"].as_table()) {
                                                if (auto* n = (*p)["name"].as_string()) {
                                                    p_name = n->get();
                                                }
                                            }
                                        } catch (...) {}
                                    }

                                    ImGui::PushID(proj_count);
                                    if (ImGui::Button("Open", ImVec2(60, 24))) {
                                        if (on_project_selected) on_project_selected(p_path);
                                        if (is_open) *is_open = false;
                                    }
                                    ImGui::SameLine();
                                    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "%s", p_name.c_str());
                                    ImGui::SameLine();
                                    ImGui::TextDisabled("(%s)", p_path.c_str());
                                    ImGui::PopID();
                                }
                            }
                        }
                    }
                    if (proj_count == 0) {
                        ImGui::TextDisabled("No local projects detected in ./projects");
                    }
                }
                ImGui::EndChild();

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

} // namespace editor
