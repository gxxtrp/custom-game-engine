#include "engine/core/log.h"
#include "engine/core/platform.h"
#include "engine/vfs/pak_archive.h"
#include <filesystem>
#include <iostream>
#include <vector>

using namespace engine::core;
using namespace engine::vfs;
namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    Logger::instance().add_sink(std::make_shared<ConsoleSink>());

    std::string input_dir = "sandbox_project/assets";
    std::string output_pak = "game_data.pak";

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--input" && i + 1 < argc) {
            input_dir = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_pak = argv[++i];
        }
    }

    LOG_INFO("PakBuilder", "==================================================");
    LOG_INFO("PakBuilder", "    Automated PAK Archive & Asset Cooker Tool     ");
    LOG_INFO("PakBuilder", "==================================================");
    LOG_INFO("PakBuilder", "Input Directory: {}", input_dir);
    LOG_INFO("PakBuilder", "Output Archive : {}", output_pak);

    if (!fs::exists(input_dir)) {
        LOG_WARN("PakBuilder", "Input directory does not exist: {}. Creating dummy test files...", input_dir);
        fs::create_directories(input_dir);
        std::ofstream dummy1(input_dir + "/test_shader.spv", std::ios::binary);
        dummy1 << "SPIRV_DUMMY_BYTECODE_TEST_DATA";
        dummy1.close();

        std::ofstream dummy2(input_dir + "/level_manifest.toml");
        dummy2 << "title = \"PakCookedMap\"\nversion = 1\n";
        dummy2.close();
    }

    std::vector<std::pair<std::string, std::string>> files;
    for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
        if (entry.is_regular_file()) {
            std::string physical_path = entry.path().string();
            std::string rel_path = fs::relative(entry.path(), input_dir).generic_string();
            files.emplace_back(rel_path, physical_path);
            LOG_INFO("PakBuilder", " -> Adding file: '{}' (Size: {} bytes)", rel_path, entry.file_size());
        }
    }

    if (files.empty()) {
        LOG_WARN("PakBuilder", "No files found to pack in: {}", input_dir);
        return 0;
    }

    if (PakArchiveMountPoint::create_pak(output_pak, files)) {
        LOG_INFO("PakBuilder", "Successfully cooked and packed {} assets into '{}'!", files.size(), output_pak);
    } else {
        LOG_FATAL("PakBuilder", "Failed to create PAK archive: {}", output_pak);
        return 1;
    }

    // Verify loading archive
    LOG_INFO("PakBuilder", "Verifying PAK integrity with PakArchiveMountPoint...");
    PakArchiveMountPoint pak_mount(output_pak);
    if (!pak_mount.is_valid()) {
        LOG_FATAL("PakBuilder", "Integrity check failed: PAK is invalid!");
        return 1;
    }

    for (const auto& [rel, _] : files) {
        if (!pak_mount.file_exists(rel)) {
            LOG_FATAL("PakBuilder", "Missing file inside PAK: {}", rel);
            return 1;
        }
        std::string content;
        pak_mount.read_string(rel, content);
        LOG_INFO("PakBuilder", " [VERIFIED] '{}' ({} bytes)", rel, content.size());
    }

    LOG_INFO("PakBuilder", "PAK Archive verification PASSED cleanly.");
    return 0;
}
