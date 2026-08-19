#pragma once

#include "engine/scene/scene.h"
#include "editor/selection_context.h"
#include "editor/command_history.h"
#include "editor/asset_importer.h"
#include <imgui.h>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

namespace editor {

struct ContentBrowserItem {
    std::string name;
    std::string virtual_path;
    std::string physical_path;
    bool is_directory{false};
    AssetCategory category{AssetCategory::Unknown};
    size_t file_size{0};
};

class ContentBrowserPanel {
public:
    ContentBrowserPanel();
    ~ContentBrowserPanel() = default;

    void render(engine::scene::Scene& scene, 
                SelectionContext& selection, 
                CommandHistory& history, 
                bool* is_open = nullptr);

    void set_current_directory(std::string_view virtual_dir);
    std::string_view get_current_directory() const { return m_current_virtual_dir; }

    void refresh();

private:
    void draw_breadcrumb_bar();
    void draw_folder_tree(const std::string& current_virtual);
    void draw_item_grid(engine::scene::Scene& scene, SelectionContext& selection, CommandHistory& history);
    void draw_audio_preview_bar();
    void draw_context_menu(engine::scene::Scene& scene, SelectionContext& selection, CommandHistory& history);
    void draw_modals();

    void scan_current_directory();

    std::string m_current_virtual_dir{"/assets"};
    std::vector<ContentBrowserItem> m_cached_items;
    char m_filter_buffer[128]{""};

    // Audio Preview State
    std::string m_currently_playing_sound;
    bool m_is_audio_playing{false};
    float m_preview_volume{1.0f};

    // Modal Action State
    bool m_show_rename_modal{false};
    std::string m_item_to_rename_path;
    char m_rename_buffer[128]{""};

    bool m_show_new_folder_modal{false};
    char m_new_folder_buffer[64]{"NewFolder"};

    bool m_show_new_script_modal{false};
    char m_new_script_buffer[64]{"CustomComponent"};
};

} // namespace editor
