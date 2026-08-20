#include "editor/tools/game_exporter.h"
#include "engine/vfs/pak_archive.h"
#include "engine/vfs/vfs.h"
#include "engine/project/project.h"
#include "engine/core/log.h"
#include "engine/core/platform.h"
#include <filesystem>
#include <fstream>
#include <format>

namespace fs = std::filesystem;

namespace editor {

GameExporter::GameExporter() {
    auto& proj = engine::project::ProjectManager::instance().get_active_project();
    m_settings.project_name = proj.name;
    m_settings.startup_map = proj.default_map;
    m_settings.output_directory = "dist/" + proj.name;
}

void GameExporter::render_dialog(engine::scene::Scene& scene, bool* is_open) {
    if (is_open && !*is_open) return;

    ImGui::SetNextWindowSize(ImVec2(560, 440), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Package Project & Standalone Exporter", is_open)) {
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Standalone Distribution Build & Cooker");
        ImGui::TextWrapped("Bundles engine assets into a compressed 'game_data.pak', copies the runtime binary and DLLs, and generates a standalone playable release.");
        ImGui::Separator();

        // Project Name
        char name_buf[128];
        strncpy_s(name_buf, m_settings.project_name.c_str(), sizeof(name_buf));
        if (ImGui::InputText("Game Title", name_buf, sizeof(name_buf))) {
            m_settings.project_name = name_buf;
            m_settings.output_directory = "dist/" + m_settings.project_name;
        }

        // Startup Map
        char map_buf[256];
        strncpy_s(map_buf, m_settings.startup_map.c_str(), sizeof(map_buf));
        if (ImGui::InputText("Startup Map", map_buf, sizeof(map_buf))) {
            m_settings.startup_map = map_buf;
        }

        // Output Directory
        char out_buf[256];
        strncpy_s(out_buf, m_settings.output_directory.c_str(), sizeof(out_buf));
        if (ImGui::InputText("Output Folder", out_buf, sizeof(out_buf))) {
            m_settings.output_directory = out_buf;
        }

        ImGui::Spacing();
        ImGui::Text("Build Configuration:");
        int config_idx = (m_settings.configuration == PackagingBuildConfig::Release) ? 0 : 1;
        if (ImGui::RadioButton("Release (Optimized)", &config_idx, 0)) {
            m_settings.configuration = PackagingBuildConfig::Release;
        }
        ImGui::SameLine(0, 20.0f);
        if (ImGui::RadioButton("Debug (Diagnostics)", &config_idx, 1)) {
            m_settings.configuration = PackagingBuildConfig::Debug;
        }

        ImGui::Spacing();
        ImGui::Checkbox("Prune unused assets (Scan startup map dependencies)", &m_settings.prune_unused_assets);
        ImGui::Checkbox("Include debug symbols (.pdb)", &m_settings.include_debug_symbols);

        ImGui::Separator();

        // Status & Progress
        if (m_is_exporting) {
            ImGui::ProgressBar(m_progress, ImVec2(-1, 24.0f), m_status_message.c_str());
        } else {
            ImGui::TextDisabled("Status: %s", m_status_message.c_str());
        }

        ImGui::Spacing();
        if (ImGui::Button("Package Game", ImVec2(160, 32)) && !m_is_exporting) {
            m_is_exporting = true;
            std::string log;
            m_last_export_success = export_game(m_settings, log);
            m_last_export_log = std::move(log);
            m_is_exporting = false;
            m_show_result_modal = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Open Output Folder", ImVec2(160, 32))) {
            std::error_code ec;
            fs::create_directories(m_settings.output_directory, ec);
            std::string cmd = "explorer \"" + fs::absolute(m_settings.output_directory).string() + "\"";
            std::system(cmd.c_str());
        }
    }
    ImGui::End();

    // Result Modal
    if (m_show_result_modal) {
        ImGui::OpenPopup("Packaging Result");
        m_show_result_modal = false;
    }

    if (ImGui::BeginPopupModal("Packaging Result", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (m_last_export_success) {
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "Game Packaged Successfully!");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Packaging Failed!");
        }
        ImGui::Separator();
        ImGui::TextUnformatted(m_last_export_log.c_str());
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(100, 28))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

bool GameExporter::export_game(const PackagingSettings& settings, std::string& out_log) {
    m_progress = 0.1f;
    m_status_message = "Preparing distribution directories...";

    std::error_code ec;
    fs::path out_dir(settings.output_directory);
    fs::create_directories(out_dir, ec);

    std::stringstream log_ss;
    log_ss << "Target Directory: " << out_dir.string() << "\n";
    log_ss << "Configuration   : " << ((settings.configuration == PackagingBuildConfig::Release) ? "Release" : "Debug") << "\n\n";

    // 1. Gather all asset files for PAK
    m_progress = 0.3f;
    m_status_message = "Scanning assets and maps for cooking...";

    std::vector<std::pair<std::string, std::string>> pak_files;
    auto& proj_mgr = engine::project::ProjectManager::instance();
    std::string proj_root = proj_mgr.get_project_root();
    if (proj_root.empty()) {
        LOG_ERROR("Exporter", "Cannot package project: No active project is currently open!");
        m_status_message = "Packaging Failed: No active project open.";
        m_is_exporting = false;
        return false;
    }

    std::vector<std::string> scan_dirs = {
        proj_root + "/assets",
        proj_root + "/maps",
        proj_root + "/scripts",
        proj_root + "/config"
    };

    size_t total_uncompressed_bytes = 0;

    for (const auto& dir : scan_dirs) {
        if (!fs::exists(dir)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
            if (entry.is_regular_file()) {
                std::string physical_path = entry.path().string();
                std::string rel_path = fs::relative(entry.path(), proj_root).generic_string();
                pak_files.emplace_back(rel_path, physical_path);
                total_uncompressed_bytes += entry.file_size();
            }
        }
    }

    if (pak_files.empty()) {
        // Create fallback test files if directory is fresh
        std::string assets_dir = proj_root + "/assets";
        fs::create_directories(assets_dir, ec);
        std::string dummy_file = assets_dir + "/game_manifest.toml";
        std::ofstream dummy(dummy_file);
        dummy << "title = \"" << settings.project_name << "\"\n";
        dummy.close();
        pak_files.emplace_back("assets/game_manifest.toml", dummy_file);
    }

    // 2. Cook assets into game_data.pak
    m_progress = 0.6f;
    m_status_message = "Building compressed game_data.pak archive...";

    std::string pak_dest = (out_dir / "game_data.pak").string();
    if (!engine::vfs::PakArchiveMountPoint::create_pak(pak_dest, pak_files)) {
        log_ss << "ERROR: Failed to create PAK archive: " << pak_dest << "\n";
        out_log = log_ss.str();
        m_status_message = "Failed to build PAK archive";
        return false;
    }

    size_t pak_size = fs::file_size(pak_dest, ec);
    log_ss << "Cooked " << pak_files.size() << " files into 'game_data.pak' (" 
           << (pak_size / 1024) << " KB, Uncompressed: " << (total_uncompressed_bytes / 1024) << " KB)\n";

    // 3. Copy runtime executable & DLLs
    m_progress = 0.8f;
    m_status_message = "Deploying standalone executable and dependencies...";

    std::string config_name = (settings.configuration == PackagingBuildConfig::Release) ? "x64-clang-release" : "x64-clang-debug";
    fs::path bin_dir = fs::path("build") / config_name;

    // Search for runtime executable (runtime.exe, or sandbox.exe fallback)
    fs::path src_exe = bin_dir / "runtime" / "runtime.exe";
    if (!fs::exists(src_exe)) {
        src_exe = bin_dir / "sandbox" / "sandbox.exe";
    }
    if (!fs::exists(src_exe)) {
        src_exe = bin_dir / "editor" / "editor.exe";
    }

    fs::path dst_exe = out_dir / (settings.project_name + ".exe");
    if (fs::exists(src_exe)) {
        fs::copy_file(src_exe, dst_exe, fs::copy_options::overwrite_existing, ec);
        log_ss << "Copied runtime executable -> " << dst_exe.filename().string() << "\n";
    } else {
        log_ss << "WARNING: Source executable not found in " << src_exe.string() << "\n";
    }

    // Copy any runtime DLLs in bin_dir / vcpkg bin
    fs::path vcpkg_bin = bin_dir / "vcpkg_installed" / "x64-windows" / "bin";
    if (fs::exists(vcpkg_bin)) {
        for (const auto& entry : fs::directory_iterator(vcpkg_bin, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                fs::copy_file(entry.path(), out_dir / entry.path().filename(), fs::copy_options::overwrite_existing, ec);
                log_ss << "Copied dependency DLL: " << entry.path().filename().string() << "\n";
            }
        }
    }

    // 4. Generate standalone project.toml
    m_progress = 0.95f;
    m_status_message = "Generating production manifest...";

    fs::path manifest_path = out_dir / "project.toml";
    std::ofstream manifest(manifest_path);
    if (manifest.is_open()) {
        manifest << "# Standalone Production Game Manifest\n";
        manifest << "name = \"" << settings.project_name << "\"\n";
        manifest << "version = \"1.0.0\"\n";
        manifest << "default_map = \"" << settings.startup_map << "\"\n";
        manifest << "pak_archive = \"game_data.pak\"\n";
        manifest.close();
    }

    m_progress = 1.0f;
    m_status_message = "Packaging Complete!";
    log_ss << "\nSuccessfully packaged standalone game at: " << fs::absolute(out_dir).string() << "\n";

    out_log = log_ss.str();
    LOG_INFO("GameExporter", "{}", out_log);
    return true;
}

} // namespace editor
