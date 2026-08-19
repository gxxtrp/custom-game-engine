#include "editor/core/editor_preferences.h"
#include "engine/core/log.h"
#include "engine/core/platform.h"
#include <toml++/toml.hpp>
#include <filesystem>
#include <sstream>
#include <chrono>
#include <algorithm>

namespace editor {

bool EditorPreferences::load_from_file(const std::string& path) {
    std::string content;
    if (!engine::core::FileSystem::read_file_string(path, content)) {
        LOG_INFO("Editor", "No preferences file found at '{}', using defaults", path);
        return false;
    }

    try {
        auto tbl = toml::parse(content);

        if (auto* general = tbl["general"].as_table()) {
            if (auto* lp = (*general)["last_project_path"].as_string()) {
                last_project_path = lp->get();
            }
            if (auto* th = (*general)["theme_index"].as_integer()) {
                theme_index = static_cast<int>(th->get());
            }
        }

        if (auto* vp = tbl["viewport"].as_table()) {
            if (auto* cs = (*vp)["camera_speed"].as_floating_point()) {
                camera_speed = static_cast<float>(cs->get());
            }
            if (auto* fov = (*vp)["camera_fov"].as_floating_point()) {
                camera_fov = static_cast<float>(fov->get());
            }
            if (auto* sens = (*vp)["flycam_sensitivity"].as_floating_point()) {
                flycam_sensitivity = static_cast<float>(sens->get());
            }
        }

        if (auto* snap = tbl["snapping"].as_table()) {
            if (auto* se = (*snap)["snap_enabled"].as_boolean()) {
                snap_enabled = se->get();
            }
            if (auto* st = (*snap)["snap_translation"].as_floating_point()) {
                snap_translation = static_cast<float>(st->get());
            }
            if (auto* sr = (*snap)["snap_rotation"].as_floating_point()) {
                snap_rotation = static_cast<float>(sr->get());
            }
            if (auto* ss = (*snap)["snap_scale"].as_floating_point()) {
                snap_scale = static_cast<float>(ss->get());
            }
        }

        if (auto* auto_tbl = tbl["autosave"].as_table()) {
            if (auto* ae = (*auto_tbl)["autosave_enabled"].as_boolean()) {
                autosave_enabled = ae->get();
            }
            if (auto* ai = (*auto_tbl)["autosave_interval_seconds"].as_floating_point()) {
                autosave_interval_seconds = static_cast<float>(ai->get());
            }
            if (auto* ma = (*auto_tbl)["max_autosaves"].as_integer()) {
                max_autosaves = static_cast<uint32_t>(ma->get());
            }
        }

        if (auto* recents = tbl["recent_projects"].as_array()) {
            recent_projects.clear();
            for (auto&& item : *recents) {
                if (auto* r_tbl = item.as_table()) {
                    RecentProjectEntry entry{};
                    if (auto* n = (*r_tbl)["name"].as_string()) entry.name = n->get();
                    if (auto* p = (*r_tbl)["path"].as_string()) entry.path = p->get();
                    if (auto* ts = (*r_tbl)["last_opened"].as_integer()) entry.last_opened_timestamp = ts->get();
                    if (!entry.path.empty()) {
                        recent_projects.push_back(std::move(entry));
                    }
                }
            }
        }

        LOG_INFO("Editor", "Loaded editor preferences from '{}'", path);
        return true;
    } catch (const toml::parse_error& err) {
        LOG_ERROR("Editor", "Failed to parse editor preferences TOML: {}", err.description());
        return false;
    }
}

bool EditorPreferences::save_to_file(const std::string& path) const {
    toml::table root;

    toml::table gen_tbl;
    gen_tbl.insert("last_project_path", last_project_path);
    gen_tbl.insert("theme_index", theme_index);
    root.insert("general", gen_tbl);

    toml::table vp_tbl;
    vp_tbl.insert("camera_speed", camera_speed);
    vp_tbl.insert("camera_fov", camera_fov);
    vp_tbl.insert("flycam_sensitivity", flycam_sensitivity);
    root.insert("viewport", vp_tbl);

    toml::table snap_tbl;
    snap_tbl.insert("snap_enabled", snap_enabled);
    snap_tbl.insert("snap_translation", snap_translation);
    snap_tbl.insert("snap_rotation", snap_rotation);
    snap_tbl.insert("snap_scale", snap_scale);
    root.insert("snapping", snap_tbl);

    toml::table auto_tbl;
    auto_tbl.insert("autosave_enabled", autosave_enabled);
    auto_tbl.insert("autosave_interval_seconds", autosave_interval_seconds);
    auto_tbl.insert("max_autosaves", static_cast<int64_t>(max_autosaves));
    root.insert("autosave", auto_tbl);

    toml::array recents_arr;
    for (const auto& entry : recent_projects) {
        toml::table r_tbl;
        r_tbl.insert("name", entry.name);
        r_tbl.insert("path", entry.path);
        r_tbl.insert("last_opened", entry.last_opened_timestamp);
        recents_arr.push_back(r_tbl);
    }
    root.insert("recent_projects", recents_arr);

    std::stringstream ss;
    ss << root;

    std::filesystem::path p(path);
    std::error_code ec;
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    if (!engine::core::FileSystem::write_file_string(path, ss.str())) {
        LOG_ERROR("Editor", "Failed to save editor preferences to '{}'", path);
        return false;
    }

    LOG_INFO("Editor", "Saved editor preferences to '{}'", path);
    return true;
}

void EditorPreferences::add_recent_project(const std::string& name, const std::string& path) {
    // Remove if already exists
    recent_projects.erase(
        std::remove_if(recent_projects.begin(), recent_projects.end(),
            [&](const RecentProjectEntry& e) { return e.path == path; }),
        recent_projects.end()
    );

    auto now = std::chrono::system_clock::now();
    int64_t ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    RecentProjectEntry new_entry{
        .name = name,
        .path = path,
        .last_opened_timestamp = ts
    };

    recent_projects.insert(recent_projects.begin(), new_entry);
    last_project_path = path;

    // Cap to 10 recent projects
    if (recent_projects.size() > 10) {
        recent_projects.resize(10);
    }
}

} // namespace editor
