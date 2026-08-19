#pragma once

#include "engine/scene/scene.h"
#include <imgui.h>
#include <string>
#include <vector>

namespace editor {

enum class PackagingBuildConfig {
    Debug,
    Release
};

struct PackagingSettings {
    std::string project_name{"SandboxGame"};
    std::string startup_map{"maps/sandbox.map"};
    std::string output_directory{"dist/SandboxGame"};
    PackagingBuildConfig configuration{PackagingBuildConfig::Release};
    bool prune_unused_assets{false};
    bool include_debug_symbols{false};
};

class GameExporter {
public:
    GameExporter();
    ~GameExporter() = default;

    void render_dialog(engine::scene::Scene& scene, bool* is_open = nullptr);

    bool export_game(const PackagingSettings& settings, std::string& out_log);

    bool is_exporting() const { return m_is_exporting; }
    float get_progress() const { return m_progress; }
    const std::string& get_status_message() const { return m_status_message; }

private:
    PackagingSettings m_settings;
    bool m_is_exporting{false};
    float m_progress{0.0f};
    std::string m_status_message{"Ready"};
    std::string m_last_export_log;
    bool m_last_export_success{false};
    bool m_show_result_modal{false};
};

} // namespace editor
