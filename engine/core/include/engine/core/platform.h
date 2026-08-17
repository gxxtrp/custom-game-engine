#pragma once

#include "engine/core/config.h"
#include "engine/core/containers.h"
#include <string>
#include <string_view>
#include <functional>

struct SDL_Window;

namespace engine::core {

enum class KeyCode : uint16_t {
    Unknown = 0,
    Space = 32,
    A = 65, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Escape = 256,
    Enter,
    Tab,
    Backspace,
    Insert,
    Delete,
    Right,
    Left,
    Down,
    Up,
    PageUp,
    PageDown,
    Home,
    End,
    CapsLock,
    ScrollLock,
    NumLock,
    PrintScreen,
    Pause,
    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    LeftShift = 340,
    LeftControl,
    LeftAlt,
    LeftSuper,
    RightShift,
    RightControl,
    RightAlt,
    RightSuper
};

enum class MouseButton : uint8_t {
    None = 0,
    Left = 1,
    Middle = 2,
    Right = 3,
    X1 = 4,
    X2 = 5
};

enum class EventType : uint8_t {
    None = 0,
    Quit,
    WindowClose,
    WindowResize,
    WindowMoved,
    WindowFocusGained,
    WindowFocusLost,
    KeyDown,
    KeyUp,
    MouseButtonDown,
    MouseButtonUp,
    MouseMove,
    MouseWheel
};

struct PlatformEvent {
    EventType type{EventType::None};
    uint64_t timestamp_ns{0};

    union {
        struct {
            uint32_t width;
            uint32_t height;
        } window_resize;

        struct {
            int32_t x;
            int32_t y;
        } window_move;

        struct {
            KeyCode key;
            bool repeat;
            bool shift;
            bool ctrl;
            bool alt;
        } key;

        struct {
            MouseButton button;
            float x;
            float y;
        } mouse_button;

        struct {
            float x;
            float y;
            float dx;
            float dy;
        } mouse_move;

        struct {
            float x_offset;
            float y_offset;
        } mouse_wheel;
    };
};

struct WindowDesc {
    std::string title{"Modern Game Engine"};
    uint32_t width{1280};
    uint32_t height{720};
    bool resizable{true};
    bool fullscreen{false};
    bool borderless{false};
    bool vulkan_compatible{true};
};

class Window {
public:
    Window();
    ~Window();

    bool create(const WindowDesc& desc);
    void destroy();

    bool is_open() const { return m_window != nullptr && !m_should_close; }
    bool should_close() const { return m_should_close; }
    void set_should_close(bool close) { m_should_close = close; }

    uint32_t get_width() const { return m_width; }
    uint32_t get_height() const { return m_height; }
    void get_framebuffer_size(uint32_t& out_w, uint32_t& out_h) const;

    void set_title(std::string_view title);
    std::string_view get_title() const { return m_title; }

    SDL_Window* get_sdl_window() const { return m_window; }
    void* get_native_handle() const;

    void on_resized(uint32_t width, uint32_t height);

private:
    SDL_Window* m_window{nullptr};
    std::string m_title;
    uint32_t m_width{0};
    uint32_t m_height{0};
    bool m_should_close{false};
};

class Platform {
public:
    static bool init();
    static void shutdown();
    static bool poll_events(DynamicArray<PlatformEvent>& out_events);
    static void process_window_events(Window& window, const PlatformEvent& event);
};

class Clock {
public:
    static double get_time_seconds();
    static uint64_t get_time_nanoseconds();
    static void sleep_ms(uint32_t ms);
};

class FrameTimer {
public:
    FrameTimer();
    void tick();

    float delta_time() const { return m_delta_time; }
    float total_time() const { return m_total_time; }
    uint32_t fps() const { return m_fps; }
    uint64_t frame_count() const { return m_frame_count; }

private:
    double m_last_time{0.0};
    float m_delta_time{0.0f};
    float m_total_time{0.0f};
    uint64_t m_frame_count{0};
    uint32_t m_fps{0};
    uint32_t m_fps_accumulator{0};
    double m_fps_timer{0.0};
};

class FileSystem {
public:
    static bool read_file_string(const std::string& path, std::string& out_content);
    static bool read_file_binary(const std::string& path, DynamicArray<uint8_t>& out_data);
    static bool write_file_string(const std::string& path, std::string_view content);
    static bool write_file_binary(const std::string& path, const void* data, size_t size);
    static bool file_exists(const std::string& path);
    static size_t get_file_size(const std::string& path);
};

} // namespace engine::core
