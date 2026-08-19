#pragma once

#include "engine/core/log.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <memory>

namespace editor {

struct EditorConsoleEntry {
    engine::core::LogLevel level{engine::core::LogLevel::Info};
    std::string category;
    std::string message;
    std::string file;
    uint32_t line{0};
    std::string timestamp;
};

class EditorConsoleSink : public engine::core::ILogSink {
public:
    explicit EditorConsoleSink(size_t max_entries = 2000);
    void log(const engine::core::LogMessage& message) override;
    void flush() override {}

    std::vector<EditorConsoleEntry> get_entries() const;
    void clear();

private:
    size_t m_max_entries;
    mutable std::mutex m_mutex;
    std::deque<EditorConsoleEntry> m_entries;
};

class ConsolePanel {
public:
    explicit ConsolePanel(std::shared_ptr<EditorConsoleSink> sink = nullptr);
    ~ConsolePanel() = default;

    void set_sink(std::shared_ptr<EditorConsoleSink> sink) { m_sink = std::move(sink); }
    void render(bool* is_open = nullptr);

    void clear();

private:
    std::shared_ptr<EditorConsoleSink> m_sink;

    bool m_show_info{true};
    bool m_show_warn{true};
    bool m_show_error{true};
    bool m_auto_scroll{true};
    char m_filter_buffer[128]{""};
};

} // namespace editor
