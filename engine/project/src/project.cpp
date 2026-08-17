#include "engine/project/project.h"
#include "engine/core/log.h"
#include "engine/core/platform.h"
#include "engine/vfs/vfs.h"
#include "engine/assets/asset_manager.h"
#include <toml++/toml.hpp>
#include <filesystem>
#include <sstream>

namespace engine::project {

std::string ProjectSettings::to_toml() const {
    toml::table root;

    toml::table proj_tbl;
    proj_tbl.insert("name", name);
    proj_tbl.insert("version", version);
    proj_tbl.insert("engine_version", engine_version);
    proj_tbl.insert("default_map", default_map);
    proj_tbl.insert("id", project_id.to_string());
    root.insert("project", proj_tbl);

    toml::table disp_tbl;
    disp_tbl.insert("width", static_cast<int64_t>(window_width));
    disp_tbl.insert("height", static_cast<int64_t>(window_height));
    disp_tbl.insert("vsync", vsync);
    disp_tbl.insert("fullscreen", fullscreen);
    root.insert("display", disp_tbl);

    toml::table rnd_tbl;
    rnd_tbl.insert("ray_tracing", ray_tracing);
    rnd_tbl.insert("shadow_quality", shadow_quality);
    root.insert("rendering", rnd_tbl);

    toml::table phys_tbl;
    phys_tbl.insert("fixed_timestep", fixed_timestep);
    toml::array grav_arr;
    grav_arr.push_back(gravity.x);
    grav_arr.push_back(gravity.y);
    grav_arr.push_back(gravity.z);
    phys_tbl.insert("gravity", grav_arr);
    root.insert("physics", phys_tbl);

    std::stringstream ss;
    ss << root;
    return ss.str();
}

bool ProjectSettings::from_toml(std::string_view toml_content) {
    try {
        auto tbl = toml::parse(toml_content);

        if (auto* p = tbl["project"].as_table()) {
            if (auto* n = (*p)["name"].as_string()) name = n->get();
            if (auto* v = (*p)["version"].as_string()) version = v->get();
            if (auto* ev = (*p)["engine_version"].as_string()) engine_version = ev->get();
            if (auto* dm = (*p)["default_map"].as_string()) default_map = dm->get();
            if (auto* pid = (*p)["id"].as_string()) project_id = assets::UUID::from_string(pid->get());
        }

        if (auto* d = tbl["display"].as_table()) {
            if (auto* w = (*d)["width"].as_integer()) window_width = static_cast<uint32_t>(w->get());
            if (auto* h = (*d)["height"].as_integer()) window_height = static_cast<uint32_t>(h->get());
            if (auto* vs = (*d)["vsync"].as_boolean()) vsync = vs->get();
            if (auto* fs = (*d)["fullscreen"].as_boolean()) fullscreen = fs->get();
        }

        if (auto* r = tbl["rendering"].as_table()) {
            if (auto* rt = (*r)["ray_tracing"].as_boolean()) ray_tracing = rt->get();
            if (auto* sq = (*r)["shadow_quality"].as_string()) shadow_quality = sq->get();
        }

        if (auto* ph = tbl["physics"].as_table()) {
            if (auto* ts = (*ph)["fixed_timestep"].as_floating_point()) fixed_timestep = static_cast<float>(ts->get());
            if (auto* g = (*ph)["gravity"].as_array()) {
                if (g->size() >= 3) {
                    gravity.x = static_cast<float>(g->get(0)->as_floating_point() ? g->get(0)->as_floating_point()->get() : 0.0);
                    gravity.y = static_cast<float>(g->get(1)->as_floating_point() ? g->get(1)->as_floating_point()->get() : 0.0);
                    gravity.z = static_cast<float>(g->get(2)->as_floating_point() ? g->get(2)->as_floating_point()->get() : 0.0);
                }
            }
        }

        return true;
    } catch (const toml::parse_error& err) {
        LOG_ERROR("Project", "Failed to parse project TOML: {}", err.description());
        return false;
    }
}

ProjectManager& ProjectManager::instance() {
    static ProjectManager s_instance;
    return s_instance;
}

bool ProjectManager::create_project(const std::string& directory_path, const std::string& project_name) {
    std::filesystem::path dir(directory_path);
    std::error_code ec;
    std::filesystem::create_directories(dir / "assets" / "meshes", ec);
    std::filesystem::create_directories(dir / "assets" / "textures", ec);
    std::filesystem::create_directories(dir / "assets" / "materials", ec);
    std::filesystem::create_directories(dir / "assets" / "shaders", ec);
    std::filesystem::create_directories(dir / "config", ec);
    std::filesystem::create_directories(dir / "maps", ec);
    std::filesystem::create_directories(dir / ".engine", ec);

    m_settings = ProjectSettings{};
    m_settings.name = project_name;
    m_settings.project_id = assets::UUID::generate();

    std::filesystem::path manifest = dir / "project.toml";
    std::string toml_str = m_settings.to_toml();

    if (!core::FileSystem::write_file_string(manifest.string(), toml_str)) {
        LOG_ERROR("Project", "Failed to write project manifest: {}", manifest.string());
        return false;
    }

    LOG_INFO("Project", "Created project '{}' at: {}", project_name, dir.string());
    return load_project(manifest.string());
}

bool ProjectManager::load_project(const std::string& project_toml_path) {
    std::string content;
    if (!core::FileSystem::read_file_string(project_toml_path, content)) {
        LOG_ERROR("Project", "Failed to read project file: {}", project_toml_path);
        return false;
    }

    if (!m_settings.from_toml(content)) {
        return false;
    }

    std::filesystem::path manifest_path(project_toml_path);
    m_project_root = manifest_path.parent_path().string();
    m_manifest_path = project_toml_path;
    m_loaded = true;

    // Auto-mount project folders into VFS
    std::filesystem::path root(m_project_root);
    vfs::VFS::instance().mount_physical("/assets", (root / "assets").string(), 10);
    vfs::VFS::instance().mount_physical("/config", (root / "config").string(), 5);
    vfs::VFS::instance().mount_physical("/maps", (root / "maps").string(), 5);

    // Load UUID registry if present
    std::filesystem::path uuid_reg_path = root / ".engine" / "uuid_registry.toml";
    std::string uuid_reg_content;
    if (core::FileSystem::read_file_string(uuid_reg_path.string(), uuid_reg_content)) {
        assets::UUIDRegistry::instance().load_from_toml(uuid_reg_content);
    }

    LOG_INFO("Project", "Loaded project '{}' v{} (ID: {})", 
             m_settings.name, m_settings.version, m_settings.project_id.to_string());
    return true;
}

bool ProjectManager::save_project() {
    if (!m_loaded || m_manifest_path.empty()) return false;

    std::string toml_str = m_settings.to_toml();
    if (!core::FileSystem::write_file_string(m_manifest_path, toml_str)) {
        return false;
    }

    // Save UUID registry
    std::filesystem::path root(m_project_root);
    std::filesystem::path uuid_reg_path = root / ".engine" / "uuid_registry.toml";
    std::string uuid_toml = assets::UUIDRegistry::instance().save_to_toml();
    core::FileSystem::write_file_string(uuid_reg_path.string(), uuid_toml);

    LOG_INFO("Project", "Saved project manifest to: {}", m_manifest_path);
    return true;
}

void ProjectManager::close_project() {
    if (m_loaded) {
        save_project();
        vfs::VFS::instance().unmount("/assets");
        vfs::VFS::instance().unmount("/config");
        vfs::VFS::instance().unmount("/maps");
        m_loaded = false;
        m_project_root.clear();
        m_manifest_path.clear();
        LOG_INFO("Project", "Closed active project");
    }
}

} // namespace engine::project
