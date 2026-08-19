#include "editor/console_panel.h"
#include <chrono>
#include <format>
#include <sstream>

namespace editor {

// --- EditorConsoleSink Implementation ---
EditorConsoleSink::EditorConsoleSink(size_t max_entries)
    : m_max_entries(max_entries) {}

void EditorConsoleSink::log(const engine::core::LogMessage& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
#if defined(_WIN32)
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    std::string ts = std::format("{:02d}:{:02d}:{:02d}", tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);

    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.push_back(EditorConsoleEntry{
        .level = message.level,
        .category = std::string(message.category),
        .message = std::string(message.message),
        .file = std::string(message.file),
        .line = message.line,
        .timestamp = std::move(ts)
    });

    if (m_entries.size() > m_max_entries) {
        m_entries.pop_front();
    }
}

std::vector<EditorConsoleEntry> EditorConsoleSink::get_entries() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return { m_entries.begin(), m_entries.end() };
}

void EditorConsoleSink::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

// --- ConsolePanel Implementation ---
ConsolePanel::ConsolePanel(std::shared_ptr<EditorConsoleSink> sink)
    : m_sink(std::move(sink)) {}

void ConsolePanel::clear() {
    if (m_sink) {
        m_sink->clear();
    }
}

void ConsolePanel::render(bool* is_open) {
    if (is_open && !*is_open) return;

    if (ImGui::Begin("Console", is_open)) {
        // --- Toolbar ---
        if (ImGui::Button("Clear")) {
            clear();
        }
        ImGui::SameLine();

        if (ImGui::Button("Copy All")) {
            if (m_sink) {
                auto entries = m_sink->get_entries();
                std::stringstream ss;
                for (const auto& entry : entries) {
                    ss << "[" << entry.timestamp << "] [" << entry.category << "] " << entry.message << "\n";
                }
                ImGui::SetClipboardText(ss.str().c_str());
            }
        }
        ImGui::SameLine();

        ImGui::Checkbox("Auto-Scroll", &m_auto_scroll);
        ImGui::SameLine(0, 16.0f);
        ImGui::TextDisabled("|");
        ImGui::SameLine(0, 16.0f);

        ImGui::Checkbox("Info", &m_show_info);
        ImGui::SameLine();
        ImGui::Checkbox("Warnings", &m_show_warn);
        ImGui::SameLine();
        ImGui::Checkbox("Errors", &m_show_error);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 180.0f);
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputTextWithHint("##ConsoleFilter", "Search log...", m_filter_buffer, sizeof(m_filter_buffer));

        ImGui::Separator();

        // --- Log Entries Table ---
        if (ImGui::BeginTable("ConsoleLogTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 68.0f);
            ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 54.0f);
            ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            if (m_sink) {
                auto entries = m_sink->get_entries();
                for (const auto& entry : entries) {
                    // Severity filtering
                    if (entry.level == engine::core::LogLevel::Info && !m_show_info) continue;
                    if (entry.level == engine::core::LogLevel::Warn && !m_show_warn) continue;
                    if ((entry.level == engine::core::LogLevel::Error || entry.level == engine::core::LogLevel::Fatal) && !m_show_error) continue;

                    // Query filtering
                    if (m_filter_buffer[0] != '\0') {
                        if (entry.message.find(m_filter_buffer) == std::string::npos &&
                            entry.category.find(m_filter_buffer) == std::string::npos) {
                            continue;
                        }
                    }

                    ImGui::TableNextRow();

                    // Timestamp
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", entry.timestamp.c_str());

                    // Level Badge
                    ImGui::TableNextColumn();
                    if (entry.level == engine::core::LogLevel::Info) {
                        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "INFO");
                    } else if (entry.level == engine::core::LogLevel::Warn) {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "WARN");
                    } else if (entry.level == engine::core::LogLevel::Error || entry.level == engine::core::LogLevel::Fatal) {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "ERROR");
                    } else {
                        ImGui::TextDisabled("DEBUG");
                    }

                    // Category
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", entry.category.c_str());

                    // Message
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.message.c_str());
                }

                if (m_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();
}

} // namespace editor
