#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace editor {

struct RecentProjectEntry {
    std::string name;
    std::string path;
    int64_t last_opened_timestamp{0};
};

struct EditorPreferences {
    std::string last_project_path{""};
    std::vector<RecentProjectEntry> recent_projects;

    // Viewport & Camera settings
    float camera_speed{5.0f};
    float camera_fov{60.0f};
    float flycam_sensitivity{0.3f};

    // Snapping settings
    bool snap_enabled{false};
    float snap_translation{0.5f};
    float snap_rotation{15.0f};
    float snap_scale{0.1f};

    // Autosave settings
    bool autosave_enabled{true};
    float autosave_interval_seconds{300.0f};
    uint32_t max_autosaves{5};

    // UI Theme settings
    int theme_index{0}; // 0 = Modern Obsidian Dark, 1 = Classic Dark

    bool load_from_file(const std::string& path = ".engine/editor_preferences.toml");
    bool save_to_file(const std::string& path = ".engine/editor_preferences.toml") const;
    void add_recent_project(const std::string& name, const std::string& path);
    void remove_recent_project(const std::string& path);
};

} // namespace editor
