#include "engine/ui/panels/content_browser_panel.h"
#include "engine/vfs/vfs.h"
#include <imgui.h>
#include <filesystem>

namespace engine::ui {

ContentBrowserPanel::ContentBrowserPanel() = default;

void ContentBrowserPanel::render() {
    ImGui::Begin("Content Browser");

    // Directory navigation bar
    if (ImGui::Button("Assets")) m_current_virtual_dir = "/assets";
    ImGui::SameLine();
    if (ImGui::Button("Config")) m_current_virtual_dir = "/config";
    ImGui::SameLine();
    if (ImGui::Button("Maps")) m_current_virtual_dir = "/maps";

    ImGui::Separator();
    ImGui::Text("Current Path: %s", m_current_virtual_dir.c_str());
    ImGui::Separator();

    std::string physical_path = vfs::VFS::instance().resolve_physical_path(m_current_virtual_dir);

    if (!physical_path.empty() && std::filesystem::exists(physical_path)) {
        ImGui::Columns(4, nullptr, false);
        for (const auto& entry : std::filesystem::directory_iterator(physical_path)) {
            std::string filename = entry.path().filename().string();
            if (entry.is_directory()) {
                if (ImGui::Button(("[D] " + filename).c_str(), ImVec2(100, 40))) {
                    // Navigate
                }
            } else {
                if (ImGui::Button(("[F] " + filename).c_str(), ImVec2(100, 40))) {
                    // Select
                }
            }
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
    } else {
        ImGui::TextDisabled("Empty or non-physical virtual mount.");
    }

    ImGui::End();
}

} // namespace engine::ui
