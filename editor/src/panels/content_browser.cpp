#include "editor/panels/content_browser.h"
#include "editor/assets/prefab_manager.h"
#include "engine/vfs/vfs.h"
#include "engine/assets/asset_manager.h"
#include "engine/project/project.h"
#include "engine/audio/audio_engine.h"
#include "engine/core/log.h"
#include <imgui_internal.h>
#include <format>
#include <fstream>

namespace editor {

ContentBrowserPanel::ContentBrowserPanel() {
    refresh();
}

void ContentBrowserPanel::set_current_directory(std::string_view virtual_dir) {
    m_current_virtual_dir = virtual_dir;
    if (!m_current_virtual_dir.starts_with("/")) {
        m_current_virtual_dir = "/" + m_current_virtual_dir;
    }
    refresh();
}

void ContentBrowserPanel::refresh() {
    scan_current_directory();
}

void ContentBrowserPanel::scan_current_directory() {
    m_cached_items.clear();

    auto& vfs = engine::vfs::VFS::instance();
    std::string physical_dir = vfs.resolve_physical_path(m_current_virtual_dir);

    // If physical directory doesn't exist yet, try to create it
    if (!physical_dir.empty() && !std::filesystem::exists(physical_dir)) {
        std::error_code ec;
        std::filesystem::create_directories(physical_dir, ec);
    }

    if (!physical_dir.empty() && std::filesystem::exists(physical_dir)) {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(physical_dir, ec)) {
            std::string filename = entry.path().filename().string();
            if (filename.ends_with(".meta")) continue; // Hide metadata files from file view

            ContentBrowserItem item;
            item.name = filename;
            item.physical_path = entry.path().string();
            item.is_directory = entry.is_directory();
            
            std::string v_prefix = m_current_virtual_dir;
            if (v_prefix.ends_with("/")) v_prefix.pop_back();
            item.virtual_path = v_prefix + "/" + filename;

            if (item.is_directory) {
                item.category = AssetCategory::Unknown;
                item.file_size = 0;
            } else {
                item.category = EditorAssetImporter::categorize_file(filename);
                item.file_size = entry.is_regular_file() ? entry.file_size() : 0;
            }

            m_cached_items.push_back(std::move(item));
        }
    }

    // Sort directories first, then alphabetically
    std::sort(m_cached_items.begin(), m_cached_items.end(), [](const ContentBrowserItem& a, const ContentBrowserItem& b) {
        if (a.is_directory != b.is_directory) return a.is_directory;
        return a.name < b.name;
    });
}

void ContentBrowserPanel::render(engine::scene::Scene& scene, 
                                 SelectionContext& selection, 
                                 CommandHistory& history, 
                                 bool* is_open) {
    if (is_open && !*is_open) return;

    if (ImGui::Begin("Content Browser", is_open)) {
        draw_breadcrumb_bar();
        ImGui::Separator();

        // 2-Column Split: Folder Tree on left, Items Grid on right
        ImGui::Columns(2, "ContentBrowserLayout", true);
        ImGui::SetColumnWidth(0, 180.0f);

        draw_folder_tree(m_current_virtual_dir);

        ImGui::NextColumn();

        draw_item_grid(scene, selection, history);

        ImGui::Columns(1);

        draw_audio_preview_bar();
        draw_modals();
    }
    ImGui::End();
}

void ContentBrowserPanel::draw_breadcrumb_bar() {
    // Back button
    if (ImGui::Button("< Back") && m_current_virtual_dir != "/assets" && m_current_virtual_dir != "/") {
        std::filesystem::path p(m_current_virtual_dir);
        std::string parent_dir = p.parent_path().string();
        if (parent_dir.empty()) parent_dir = "/assets";
        std::replace(parent_dir.begin(), parent_dir.end(), '\\', '/');
        set_current_directory(parent_dir);
    }
    ImGui::SameLine();

    // Breadcrumbs
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Virtual Path: %s", m_current_virtual_dir.c_str());

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 220.0f);
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputTextWithHint("##ContentSearch", "Search...", m_filter_buffer, sizeof(m_filter_buffer));

    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        refresh();
    }
}

void ContentBrowserPanel::draw_folder_tree(const std::string& current_virtual) {
    ImGui::BeginChild("##FolderTreeRegion", ImVec2(0, 0), false);

    const char* root_folders[] = {
        "/assets",
        "/assets/models",
        "/assets/textures",
        "/assets/materials",
        "/assets/audio",
        "/assets/prefabs",
        "/maps",
        "/scripts",
        "/config"
    };

    ImGui::TextDisabled("PROJECT FOLDERS");
    ImGui::Separator();

    for (const char* folder : root_folders) {
        bool is_active = (current_virtual == folder);
        if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.85f, 1.0f, 1.0f));
        }

        std::string label = std::format(" > {}", folder);
        if (ImGui::Selectable(label.c_str(), is_active)) {
            set_current_directory(folder);
        }

        if (is_active) {
            ImGui::PopStyleColor();
        }
    }

    ImGui::EndChild();
}

void ContentBrowserPanel::draw_item_grid(engine::scene::Scene& scene, SelectionContext& selection, CommandHistory& history) {
    ImGui::BeginChild("##ItemGridRegion", ImVec2(0, -32.0f), false);

    float panel_width = ImGui::GetContentRegionAvail().x;
    float card_width = 96.0f;
    float card_height = 80.0f;
    int columns = std::max(1, static_cast<int>(panel_width / (card_width + 12.0f)));

    int col_idx = 0;

    for (size_t i = 0; i < m_cached_items.size(); ++i) {
        const auto& item = m_cached_items[i];

        // Search filtering
        if (m_filter_buffer[0] != '\0') {
            if (item.name.find(m_filter_buffer) == std::string::npos) continue;
        }

        ImGui::PushID(static_cast<int>(i));

        ImVec4 badge_color = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
        const char* type_str = "[FILE]";

        if (item.is_directory) {
            badge_color = ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
            type_str = "[DIR]";
        } else {
            switch (item.category) {
                case AssetCategory::Model:
                    badge_color = ImVec4(0.30f, 0.75f, 1.0f, 1.0f);
                    type_str = "[MESH]";
                    break;
                case AssetCategory::Texture:
                    badge_color = ImVec4(0.85f, 0.40f, 0.85f, 1.0f);
                    type_str = "[TEX]";
                    break;
                case AssetCategory::Audio:
                    badge_color = ImVec4(0.30f, 0.90f, 0.45f, 1.0f);
                    type_str = "[AUDIO]";
                    break;
                case AssetCategory::Material:
                    badge_color = ImVec4(0.95f, 0.50f, 0.20f, 1.0f);
                    type_str = "[MAT]";
                    break;
                case AssetCategory::Script:
                    badge_color = ImVec4(0.20f, 0.65f, 0.95f, 1.0f);
                    type_str = "[LUA]";
                    break;
                case AssetCategory::Map:
                    badge_color = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
                    type_str = "[MAP]";
                    break;
                case AssetCategory::Prefab:
                    badge_color = ImVec4(0.35f, 0.95f, 0.75f, 1.0f);
                    type_str = "[PREFAB]";
                    break;
                default:
                    break;
            }
        }

        // Draw Card Box
        ImGui::BeginGroup();
        
        ImVec2 card_size(card_width, card_height);
        if (ImGui::Button(std::format("{}##btn", type_str).c_str(), card_size)) {
            // Single click action
            if (item.category == AssetCategory::Audio) {
                engine::audio::AudioEngine::instance().play_sound_2d(item.name, engine::audio::AudioBus::SFX, m_preview_volume);
                m_currently_playing_sound = item.name;
                m_is_audio_playing = true;
            }
        }

        // Double Click Action
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (item.is_directory) {
                set_current_directory(item.virtual_path);
            } else if (item.category == AssetCategory::Prefab) {
                flecs::entity spawned = PrefabManager::instantiate_prefab(item.virtual_path, scene);
                if (spawned.is_valid()) {
                    selection.select(spawned, false);
                    history.push_executed_command(std::make_unique<EntityCreateCommand>(
                        scene, EntitySnapshot::capture(spawned, scene)
                    ));
                }
            } else if (item.category == AssetCategory::Audio) {
                engine::audio::AudioEngine::instance().play_sound_2d(item.name, engine::audio::AudioBus::SFX, m_preview_volume);
                m_currently_playing_sound = item.name;
                m_is_audio_playing = true;
            }
        }

        // Drag and Drop Source for Scene / Viewport Instantiation
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", item.virtual_path.c_str(), item.virtual_path.size() + 1);
            ImGui::Text("Asset: %s", item.name.c_str());
            ImGui::EndDragDropSource();
        }

        // Item Context Menu
        if (ImGui::BeginPopupContextItem()) {
            if (item.category == AssetCategory::Audio && ImGui::MenuItem("Play Preview")) {
                engine::audio::AudioEngine::instance().play_sound_2d(item.name, engine::audio::AudioBus::SFX, m_preview_volume);
                m_currently_playing_sound = item.name;
                m_is_audio_playing = true;
            }
            if (ImGui::MenuItem("Rename...")) {
                m_show_rename_modal = true;
                m_item_to_rename_path = item.virtual_path;
                strncpy_s(m_rename_buffer, item.name.c_str(), sizeof(m_rename_buffer));
            }
            if (ImGui::MenuItem("Delete")) {
                EditorAssetImporter::delete_asset(item.virtual_path);
                refresh();
            }
            ImGui::EndPopup();
        }

        // Label truncated
        std::string display_name = item.name;
        if (display_name.size() > 12) {
            display_name = display_name.substr(0, 10) + "..";
        }
        ImGui::TextUnformatted(display_name.c_str());
        ImGui::EndGroup();

        ImGui::PopID();

        col_idx++;
        if (col_idx < columns) {
            ImGui::SameLine(0.0f, 10.0f);
        } else {
            col_idx = 0;
        }
    }

    // Drop Target for Prefab Creation from Outliner
    if (ImGui::BeginDragDropTargetCustom(ImGui::GetCurrentWindow()->Rect(), ImGui::GetID("##ContentBrowserBgArea"))) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OUTLINER_ENTITY")) {
            uint64_t dragged_id = *static_cast<const uint64_t*>(payload->Data);
            flecs::entity dragged = scene.get_world().entity(dragged_id);
            if (dragged.is_valid() && dragged.is_alive()) {
                std::string tag = dragged.has<engine::scene::TagComponent>() 
                    ? dragged.get<engine::scene::TagComponent>().name 
                    : dragged.name().c_str();
                std::string prefab_vpath = m_current_virtual_dir + "/" + tag + ".prefab";
                if (PrefabManager::save_prefab(dragged, scene, prefab_vpath)) {
                    LOG_INFO("ContentBrowser", "Created prefab '{}' from Outliner entity", prefab_vpath);
                    refresh();
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Right-Click Context Menu on Empty Background
    if (ImGui::BeginPopupContextWindow("ContentBrowserBgContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        draw_context_menu(scene, selection, history);
        ImGui::EndPopup();
    }

    ImGui::EndChild();
}

void ContentBrowserPanel::draw_audio_preview_bar() {
    if (m_currently_playing_sound.empty()) return;

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "Audio Preview: %s", m_currently_playing_sound.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        m_currently_playing_sound.clear();
        m_is_audio_playing = false;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::SliderFloat("Preview Vol", &m_preview_volume, 0.0f, 1.0f, "%.2f");
}

void ContentBrowserPanel::draw_context_menu(engine::scene::Scene& scene, SelectionContext& selection, CommandHistory& history) {
    if (ImGui::MenuItem("New Folder...")) {
        m_show_new_folder_modal = true;
    }
    if (ImGui::MenuItem("New Lua Script...")) {
        m_show_new_script_modal = true;
    }
    if (ImGui::MenuItem("New Material (.mat)")) {
        std::string mat_vpath = m_current_virtual_dir + "/NewMaterial.mat";
        std::string mat_content = "name = \"NewMaterial\"\nalbedo = [1.0, 1.0, 1.0, 1.0]\nroughness = 0.5\nmetallic = 0.0\n";
        engine::vfs::VFS::instance().write_string(mat_vpath, mat_content);
        refresh();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Refresh")) {
        refresh();
    }
}

void ContentBrowserPanel::draw_modals() {
    // 1. Rename Modal
    if (m_show_rename_modal) {
        ImGui::OpenPopup("Rename Asset");
        m_show_rename_modal = false;
    }
    if (ImGui::BeginPopupModal("Rename Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter new name for asset:");
        ImGui::InputText("##NewAssetName", m_rename_buffer, sizeof(m_rename_buffer));
        if (ImGui::Button("Rename", ImVec2(100, 0))) {
            std::filesystem::path old_p(m_item_to_rename_path);
            std::string new_vpath = old_p.parent_path().string() + "/" + m_rename_buffer;
            std::replace(new_vpath.begin(), new_vpath.end(), '\\', '/');
            EditorAssetImporter::rename_asset(m_item_to_rename_path, new_vpath);
            refresh();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 2. New Folder Modal
    if (m_show_new_folder_modal) {
        ImGui::OpenPopup("Create New Folder");
        m_show_new_folder_modal = false;
    }
    if (ImGui::BeginPopupModal("Create New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter folder name:");
        ImGui::InputText("##FolderNameInput", m_new_folder_buffer, sizeof(m_new_folder_buffer));
        if (ImGui::Button("Create", ImVec2(100, 0))) {
            std::string new_folder_vpath = m_current_virtual_dir + "/" + m_new_folder_buffer;
            std::string physical = engine::vfs::VFS::instance().resolve_physical_path(new_folder_vpath);
            std::error_code ec;
            std::filesystem::create_directories(physical, ec);
            refresh();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 3. New Script Modal
    if (m_show_new_script_modal) {
        ImGui::OpenPopup("Create Lua Script");
        m_show_new_script_modal = false;
    }
    if (ImGui::BeginPopupModal("Create Lua Script", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Enter script component class name:");
        ImGui::InputText("##ScriptClassName", m_new_script_buffer, sizeof(m_new_script_buffer));
        if (ImGui::Button("Create Script", ImVec2(120, 0))) {
            std::string script_vpath = m_current_virtual_dir + "/" + m_new_script_buffer + ".lua";
            std::string script_content = std::format(
                "-- {}\n"
                "local {} = {{\n"
                "    speed = 5.0,\n"
                "}}\n\n"
                "function {}:on_init(entity)\n"
                "    log_info(\"{} initialized on entity: \" .. entity:get_id())\n"
                "end\n\n"
                "function {}:on_update(entity, dt)\n"
                "end\n\n"
                "return {}\n",
                script_vpath, m_new_script_buffer, m_new_script_buffer, 
                m_new_script_buffer, m_new_script_buffer, m_new_script_buffer
            );
            engine::vfs::VFS::instance().write_string(script_vpath, script_content);
            refresh();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace editor
