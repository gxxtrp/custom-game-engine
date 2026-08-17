#include "engine/core/log.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

#if defined(ENGINE_PLATFORM_WINDOWS)
#include <windows.h>
#endif

namespace engine::core {

void internal_assert_failed(const char* expr, const char* msg, const char* file, uint32_t line) {
    LOG_FATAL("Assert", "Assertion failed: ({}) - {} at {}:{}", expr, msg ? msg : "No message", file, line);
}

const char* log_level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        default: return "UNKNOWN";
    }
}

static std::string format_time(const std::chrono::system_clock::time_point& tp) {
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
    auto timer = std::chrono::system_clock::to_time_t(tp);
    std::tm bt{};
#if defined(ENGINE_PLATFORM_WINDOWS)
    localtime_s(&bt, &timer);
#else
    localtime_r(&timer, &bt);
#endif
    std::ostringstream oss;
    oss << std::put_time(&bt, "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << millis.count();
    return oss.str();
}

void ConsoleSink::log(const LogMessage& message) {
    std::string time_str = format_time(message.timestamp);

    // ANSI escape color codes
    const char* color_code = "";
    const char* reset_code = "\033[0m";

    switch (message.level) {
        case LogLevel::Trace: color_code = "\033[90m"; break; // Dark Gray
        case LogLevel::Debug: color_code = "\033[36m"; break; // Cyan
        case LogLevel::Info:  color_code = "\033[32m"; break; // Green
        case LogLevel::Warn:  color_code = "\033[33m"; break; // Yellow
        case LogLevel::Error: color_code = "\033[31m"; break; // Red
        case LogLevel::Fatal: color_code = "\033[35;1m"; break; // Bold Magenta
        default: break;
    }

    std::cout << color_code << "[" << time_str << "] [" 
              << log_level_to_string(message.level) << "] [" 
              << message.category << "] " << message.message 
              << reset_code << "\n";
}

void ConsoleSink::flush() {
    std::cout.flush();
}

FileSink::FileSink(const std::string& filepath) {
    auto* stream = new std::ofstream(filepath, std::ios::out | std::ios::app);
    if (stream->is_open()) {
        m_file_handle = stream;
    } else {
        delete stream;
        m_file_handle = nullptr;
    }
}

FileSink::~FileSink() {
    if (m_file_handle) {
        auto* stream = static_cast<std::ofstream*>(m_file_handle);
        stream->flush();
        stream->close();
        delete stream;
        m_file_handle = nullptr;
    }
}

void FileSink::log(const LogMessage& message) {
    if (!m_file_handle) return;
    auto* stream = static_cast<std::ofstream*>(m_file_handle);
    std::string time_str = format_time(message.timestamp);
    (*stream) << "[" << time_str << "] [" 
              << log_level_to_string(message.level) << "] [" 
              << message.category << "] " << message.message 
              << " (" << message.file << ":" << message.line << ")\n";
}

void FileSink::flush() {
    if (m_file_handle) {
        static_cast<std::ofstream*>(m_file_handle)->flush();
    }
}

RingBufferSink::RingBufferSink(size_t max_entries)
    : m_max_entries(max_entries), m_buffer(max_entries) {}

void RingBufferSink::log(const LogMessage& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string time_str = format_time(message.timestamp);
    std::string line = std::format("[{}] [{}] [{}] {}", time_str, log_level_to_string(message.level), message.category, message.message);
    m_buffer[m_head] = std::move(line);
    m_head = (m_head + 1) % m_max_entries;
    if (m_head == 0) m_full = true;
}

std::vector<std::string> RingBufferSink::get_recent_logs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> result;
    result.reserve(m_full ? m_max_entries : m_head);
    if (m_full) {
        for (size_t i = m_head; i < m_max_entries; ++i) {
            result.push_back(m_buffer[i]);
        }
    }
    for (size_t i = 0; i < m_head; ++i) {
        result.push_back(m_buffer[i]);
    }
    return result;
}

void RingBufferSink::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_head = 0;
    m_full = false;
}

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

Logger::Logger() {
#if defined(ENGINE_PLATFORM_WINDOWS)
    // Enable ANSI escape sequences for Windows console
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
    m_sinks.push_back(std::make_shared<ConsoleSink>());
}

Logger::~Logger() {
    for (auto& sink : m_sinks) {
        sink->flush();
    }
}

void Logger::add_sink(std::shared_ptr<ILogSink> sink) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (sink) {
        m_sinks.push_back(sink);
    }
}

void Logger::remove_all_sinks() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sinks.clear();
}

void Logger::log(LogLevel level, std::string_view category, std::string_view file, uint32_t line, std::string message) {
    if (level < m_min_level) return;

    LogMessage msg{
        .level = level,
        .category = category,
        .message = std::move(message),
        .file = file,
        .line = line,
        .timestamp = std::chrono::system_clock::now()
    };

    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& sink : m_sinks) {
        sink->log(msg);
    }
}

} // namespace engine::core
