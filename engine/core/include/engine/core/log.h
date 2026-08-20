#pragma once

#include "engine/core/config.h"
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <mutex>
#include <format>
#include <chrono>

namespace engine::core {

enum class LogLevel : uint8_t {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
    Off
};

const char* log_level_to_string(LogLevel level);

struct LogMessage {
    LogLevel level{LogLevel::Info};
    std::string_view category;
    std::string message;
    std::string_view file;
    uint32_t line{0};
    std::chrono::system_clock::time_point timestamp;
};

class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void log(const LogMessage& message) = 0;
    virtual void flush() = 0;
};

class ConsoleSink : public ILogSink {
public:
    void log(const LogMessage& message) override;
    void flush() override;
};

class FileSink : public ILogSink {
public:
    explicit FileSink(const std::string& filepath);
    ~FileSink() override;
    void log(const LogMessage& message) override;
    void flush() override;
private:
    void* m_file_handle{nullptr};
};

class RingBufferSink : public ILogSink {
public:
    explicit RingBufferSink(size_t max_entries = 1024);
    void log(const LogMessage& message) override;
    void flush() override {}

    std::vector<std::string> get_recent_logs() const;
    void clear();
private:
    size_t m_max_entries;
    mutable std::mutex m_mutex;
    std::vector<std::string> m_buffer;
    size_t m_head{0};
    bool m_full{false};
};

class Logger {
public:
    static Logger& instance();

    void add_sink(std::shared_ptr<ILogSink> sink);
    void remove_all_sinks();
    void set_min_level(LogLevel level) { m_min_level = level; }
    LogLevel get_min_level() const { return m_min_level; }

    void log(LogLevel level, std::string_view category, std::string_view file, uint32_t line, std::string message);

    template<typename... Args>
    void log_fmt(LogLevel level, std::string_view category, std::string_view file, uint32_t line,
                std::format_string<Args...> fmt, Args&&... args) {
        if (level < m_min_level) return;
        std::string formatted = std::format(fmt, std::forward<Args>(args)...);
        log(level, category, file, line, std::move(formatted));
    }

private:
    Logger();
    ~Logger();

    LogLevel m_min_level{LogLevel::Trace};
    std::vector<std::shared_ptr<ILogSink>> m_sinks;
    std::mutex m_mutex;
};

} // namespace engine::core

#define LOG_TRACE(category, fmt, ...) \
    ::engine::core::Logger::instance().log_fmt(::engine::core::LogLevel::Trace, category, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_DEBUG(category, fmt, ...) \
    ::engine::core::Logger::instance().log_fmt(::engine::core::LogLevel::Debug, category, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_INFO(category, fmt, ...) \
    ::engine::core::Logger::instance().log_fmt(::engine::core::LogLevel::Info, category, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_WARN(category, fmt, ...) \
    ::engine::core::Logger::instance().log_fmt(::engine::core::LogLevel::Warn, category, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_ERROR(category, fmt, ...) \
    ::engine::core::Logger::instance().log_fmt(::engine::core::LogLevel::Error, category, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_FATAL(category, fmt, ...) \
    ::engine::core::Logger::instance().log_fmt(::engine::core::LogLevel::Fatal, category, __FILE__, __LINE__, fmt __VA_OPT__(,) __VA_ARGS__)
