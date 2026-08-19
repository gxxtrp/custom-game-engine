#include "editor/autosave_manager.h"
#include "engine/scene/map_serializer.h"
#include "engine/core/log.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <format>
#include <algorithm>

namespace editor {

AutosaveManager::AutosaveManager(float interval_seconds, size_t max_backups)
    : m_interval_seconds(interval_seconds), m_max_backups(max_backups) {
    std::error_code ec;
    std::filesystem::create_directories(m_autosave_dir, ec);
}

void AutosaveManager::update(engine::scene::Scene& scene, float dt) {
    if (!m_enabled || m_interval_seconds <= 0.0f) return;

    m_timer += dt;
    if (m_timer >= m_interval_seconds) {
        m_timer = 0.0f;
        trigger_autosave(scene);
    }
}

bool AutosaveManager::trigger_autosave(engine::scene::Scene& scene) {
    std::error_code ec;
    std::filesystem::create_directories(m_autosave_dir, ec);

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
#if defined(_WIN32)
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    std::string timestamp = std::format("{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}",
                                        tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                                        tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);

    std::string filename = std::format("autosave_{}.map", timestamp);
    std::filesystem::path full_path = std::filesystem::path(m_autosave_dir) / filename;

    std::string toml_str;
    if (!engine::scene::MapSerializer::serialize_to_toml(scene, toml_str)) {
        LOG_ERROR("Autosave", "Failed to serialize active scene for autosave");
        return false;
    }

    std::ofstream out(full_path, std::ios::binary);
    if (!out.is_open()) {
        LOG_ERROR("Autosave", "Failed to open autosave destination: {}", full_path.string());
        return false;
    }

    out.write(toml_str.data(), static_cast<std::streamsize>(toml_str.size()));
    LOG_INFO("Autosave", "Periodic scene snapshot saved to '{}' ({} bytes)", full_path.string(), toml_str.size());

    prune_old_autosaves();
    return true;
}

void AutosaveManager::prune_old_autosaves() {
    std::error_code ec;
    if (!std::filesystem::exists(m_autosave_dir, ec)) return;

    struct AutosaveFile {
        std::filesystem::path path;
        std::filesystem::file_time_type time;
    };

    std::vector<AutosaveFile> files;
    for (const auto& entry : std::filesystem::directory_iterator(m_autosave_dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".map") {
            files.push_back({ entry.path(), entry.last_write_time(ec) });
        }
    }

    if (files.size() <= m_max_backups) return;

    // Sort descending by write time
    std::sort(files.begin(), files.end(), [](const AutosaveFile& a, const AutosaveFile& b) {
        return a.time > b.time;
    });

    // Delete older files beyond max_backups
    for (size_t i = m_max_backups; i < files.size(); ++i) {
        std::filesystem::remove(files[i].path, ec);
        LOG_INFO("Autosave", "Pruned old recovery snapshot: {}", files[i].path.filename().string());
    }
}

} // namespace editor
