#pragma once

#include "engine/core/config.h"
#include "engine/assets/uuid.h"
#include "engine/core/math.h"
#include <string>
#include <string_view>

namespace engine::project {

struct ProjectSettings {
    std::string name{"Untitled Project"};
    std::string version{"0.1.0"};
    std::string engine_version{"1.0.0"};
    std::string default_map{"maps/default.map"};
    assets::UUID project_id{assets::UUID::generate()};

    // Display
    uint32_t window_width{1280};
    uint32_t window_height{720};
    bool vsync{true};
    bool fullscreen{false};

    // Rendering
    bool ray_tracing{true};
    std::string shadow_quality{"high"};

    // Physics
    float fixed_timestep{1.0f / 60.0f};
    core::Vec3 gravity{0.0f, -9.81f, 0.0f};

    std::string to_toml() const;
    bool from_toml(std::string_view toml_content);
};

class ProjectManager {
public:
    static ProjectManager& instance();

    bool create_project(const std::string& directory_path, const std::string& project_name);
    bool load_project(const std::string& project_toml_path);
    bool save_project();

    const ProjectSettings& get_active_project() const { return m_settings; }
    const std::string& get_project_root() const { return m_project_root; }
    bool is_project_loaded() const { return m_loaded; }

    void close_project();

private:
    ProjectManager() = default;

    ProjectSettings m_settings;
    std::string m_project_root;
    std::string m_manifest_path;
    bool m_loaded{false};
};

} // namespace engine::project
