#include "engine/core/platform.h"
#include "engine/core/log.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

namespace engine::core {

static KeyCode translate_sdl_keycode(SDL_Keycode key) {
    if (key >= SDLK_A && key <= SDLK_Z) return static_cast<KeyCode>(key);
    if (key >= SDLK_0 && key <= SDLK_9) return static_cast<KeyCode>(key);

    switch (key) {
        case SDLK_SPACE:        return KeyCode::Space;
        case SDLK_ESCAPE:       return KeyCode::Escape;
        case SDLK_RETURN:       return KeyCode::Enter;
        case SDLK_TAB:          return KeyCode::Tab;
        case SDLK_BACKSPACE:    return KeyCode::Backspace;
        case SDLK_INSERT:       return KeyCode::Insert;
        case SDLK_DELETE:       return KeyCode::Delete;
        case SDLK_RIGHT:        return KeyCode::Right;
        case SDLK_LEFT:         return KeyCode::Left;
        case SDLK_DOWN:         return KeyCode::Down;
        case SDLK_UP:           return KeyCode::Up;
        case SDLK_PAGEUP:       return KeyCode::PageUp;
        case SDLK_PAGEDOWN:     return KeyCode::PageDown;
        case SDLK_HOME:         return KeyCode::Home;
        case SDLK_END:          return KeyCode::End;
        case SDLK_CAPSLOCK:     return KeyCode::CapsLock;
        case SDLK_SCROLLLOCK:   return KeyCode::ScrollLock;
        case SDLK_NUMLOCKCLEAR: return KeyCode::NumLock;
        case SDLK_PRINTSCREEN:  return KeyCode::PrintScreen;
        case SDLK_PAUSE:        return KeyCode::Pause;
        case SDLK_F1:           return KeyCode::F1;
        case SDLK_F2:           return KeyCode::F2;
        case SDLK_F3:           return KeyCode::F3;
        case SDLK_F4:           return KeyCode::F4;
        case SDLK_F5:           return KeyCode::F5;
        case SDLK_F6:           return KeyCode::F6;
        case SDLK_F7:           return KeyCode::F7;
        case SDLK_F8:           return KeyCode::F8;
        case SDLK_F9:           return KeyCode::F9;
        case SDLK_F10:          return KeyCode::F10;
        case SDLK_F11:          return KeyCode::F11;
        case SDLK_F12:          return KeyCode::F12;
        case SDLK_LSHIFT:       return KeyCode::LeftShift;
        case SDLK_LCTRL:        return KeyCode::LeftControl;
        case SDLK_LALT:         return KeyCode::LeftAlt;
        case SDLK_LGUI:         return KeyCode::LeftSuper;
        case SDLK_RSHIFT:       return KeyCode::RightShift;
        case SDLK_RCTRL:        return KeyCode::RightControl;
        case SDLK_RALT:         return KeyCode::RightAlt;
        case SDLK_RGUI:         return KeyCode::RightSuper;
        default:                return KeyCode::Unknown;
    }
}

static MouseButton translate_sdl_mouse_button(Uint8 button) {
    switch (button) {
        case SDL_BUTTON_LEFT:   return MouseButton::Left;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_RIGHT:  return MouseButton::Right;
        case SDL_BUTTON_X1:     return MouseButton::X1;
        case SDL_BUTTON_X2:     return MouseButton::X2;
        default:                return MouseButton::None;
    }
}

// Window
Window::Window() = default;

Window::~Window() {
    destroy();
}

bool Window::create(const WindowDesc& desc) {
    m_title = desc.title;
    m_width = desc.width;
    m_height = desc.height;

    SDL_WindowFlags flags = 0;
    if (desc.vulkan_compatible) flags |= SDL_WINDOW_VULKAN;
    if (desc.resizable) flags |= SDL_WINDOW_RESIZABLE;
    if (desc.fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    if (desc.borderless) flags |= SDL_WINDOW_BORDERLESS;

    m_window = SDL_CreateWindow(m_title.c_str(), static_cast<int>(m_width), static_cast<int>(m_height), flags);
    if (!m_window && (flags & SDL_WINDOW_VULKAN)) {
        LOG_WARN("Platform", "Creating Vulkan-flagged window failed ({}), attempting fallback without Vulkan flag...", SDL_GetError());
        flags &= ~SDL_WINDOW_VULKAN;
        m_window = SDL_CreateWindow(m_title.c_str(), static_cast<int>(m_width), static_cast<int>(m_height), flags);
    }

    if (!m_window) {
        LOG_FATAL("Platform", "Failed to create SDL3 window: {}", SDL_GetError());
        return false;
    }

    LOG_INFO("Platform", "Created window '{}' ({}x{})", m_title, m_width, m_height);
    return true;
}

void Window::destroy() {
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        LOG_INFO("Platform", "Destroyed window '{}'", m_title);
    }
}

void Window::get_framebuffer_size(uint32_t& out_w, uint32_t& out_h) const {
    if (!m_window) {
        out_w = m_width;
        out_h = m_height;
        return;
    }
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(m_window, &w, &h);
    out_w = static_cast<uint32_t>(w);
    out_h = static_cast<uint32_t>(h);
}

void Window::set_title(std::string_view title) {
    m_title = title;
    if (m_window) {
        SDL_SetWindowTitle(m_window, m_title.c_str());
    }
}

void* Window::get_native_handle() const {
    if (!m_window) return nullptr;
#if defined(ENGINE_PLATFORM_WINDOWS)
    return SDL_GetPointerProperty(SDL_GetWindowProperties(m_window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
    return nullptr;
#endif
}

void Window::on_resized(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
}

// Platform Subsystem
bool Platform::init() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) {
        LOG_FATAL("Platform", "Failed to initialize SDL3: {}", SDL_GetError());
        return false;
    }
    LOG_INFO("Platform", "SDL3 initialized successfully");
    return true;
}

void Platform::shutdown() {
    SDL_Quit();
    LOG_INFO("Platform", "SDL3 shutdown cleanly");
}

static Platform::RawEventCallback s_raw_event_cb = nullptr;

void Platform::set_raw_event_callback(RawEventCallback cb) {
    s_raw_event_cb = std::move(cb);
}

bool Platform::poll_events(DynamicArray<PlatformEvent>& out_events) {
    SDL_Event e;
    bool has_events = false;

    while (SDL_PollEvent(&e)) {
        has_events = true;

        if (s_raw_event_cb) {
            s_raw_event_cb(&e);
        }

        PlatformEvent event{};
        event.timestamp_ns = e.common.timestamp;

        switch (e.type) {
            case SDL_EVENT_QUIT:
                event.type = EventType::Quit;
                out_events.push_back(event);
                break;

            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                event.type = EventType::WindowClose;
                out_events.push_back(event);
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                event.type = EventType::WindowResize;
                event.window_resize.width = static_cast<uint32_t>(e.window.data1);
                event.window_resize.height = static_cast<uint32_t>(e.window.data2);
                out_events.push_back(event);
                break;

            case SDL_EVENT_WINDOW_MOVED:
                event.type = EventType::WindowMoved;
                event.window_move.x = e.window.data1;
                event.window_move.y = e.window.data2;
                out_events.push_back(event);
                break;

            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                event.type = EventType::WindowFocusGained;
                out_events.push_back(event);
                break;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                event.type = EventType::WindowFocusLost;
                out_events.push_back(event);
                break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                event.type = (e.type == SDL_EVENT_KEY_DOWN) ? EventType::KeyDown : EventType::KeyUp;
                event.key.key = translate_sdl_keycode(e.key.key);
                event.key.repeat = e.key.repeat;
                event.key.shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
                event.key.ctrl  = (e.key.mod & SDL_KMOD_CTRL) != 0;
                event.key.alt   = (e.key.mod & SDL_KMOD_ALT) != 0;
                out_events.push_back(event);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                event.type = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? EventType::MouseButtonDown : EventType::MouseButtonUp;
                event.mouse_button.button = translate_sdl_mouse_button(e.button.button);
                event.mouse_button.x = e.button.x;
                event.mouse_button.y = e.button.y;
                out_events.push_back(event);
                break;

            case SDL_EVENT_MOUSE_MOTION:
                event.type = EventType::MouseMove;
                event.mouse_move.x = e.motion.x;
                event.mouse_move.y = e.motion.y;
                event.mouse_move.dx = e.motion.xrel;
                event.mouse_move.dy = e.motion.yrel;
                out_events.push_back(event);
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                event.type = EventType::MouseWheel;
                event.mouse_wheel.x_offset = e.wheel.x;
                event.mouse_wheel.y_offset = e.wheel.y;
                out_events.push_back(event);
                break;

            default:
                break;
        }
    }

    return has_events;
}

void Platform::process_window_events(Window& window, const PlatformEvent& event) {
    if (event.type == EventType::Quit || event.type == EventType::WindowClose) {
        window.set_should_close(true);
    } else if (event.type == EventType::WindowResize) {
        window.on_resized(event.window_resize.width, event.window_resize.height);
    }
}

// Clock
double Clock::get_time_seconds() {
    static const uint64_t freq = SDL_GetPerformanceFrequency();
    return static_cast<double>(SDL_GetPerformanceCounter()) / static_cast<double>(freq);
}

uint64_t Clock::get_time_nanoseconds() {
    return SDL_GetTicksNS();
}

void Clock::sleep_ms(uint32_t ms) {
    SDL_Delay(ms);
}

// FrameTimer
FrameTimer::FrameTimer()
    : m_last_time(Clock::get_time_seconds()), m_fps_timer(m_last_time) {}

void FrameTimer::reset() {
    m_last_time = Clock::get_time_seconds();
    m_delta_time = 0.0f;
    m_total_time = 0.0f;
    m_frame_count = 0;
    m_fps = 0;
    m_fps_accumulator = 0;
    m_fps_timer = m_last_time;
}

void FrameTimer::tick() {
    double current_time = Clock::get_time_seconds();
    float raw_dt = static_cast<float>(current_time - m_last_time);
    m_delta_time = std::min(raw_dt, 0.05f); // Max 50ms (20 FPS floor) per step to prevent physics tunneling
    m_last_time = current_time;
    m_total_time += m_delta_time;
    m_frame_count++;

    m_fps_accumulator++;
    if (current_time - m_fps_timer >= 1.0) {
        m_fps = m_fps_accumulator;
        m_fps_accumulator = 0;
        m_fps_timer = current_time;
    }
}

// FileSystem
bool FileSystem::read_file_string(const std::string& path, std::string& out_content) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    out_content.resize(static_cast<size_t>(size));
    if (!file.read(out_content.data(), size)) {
        out_content.clear();
        return false;
    }
    return true;
}

bool FileSystem::read_file_binary(const std::string& path, DynamicArray<uint8_t>& out_data) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    out_data.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(out_data.data()), size)) {
        return false;
    }
    return true;
}

bool FileSystem::write_file_string(const std::string& path, std::string_view content) {
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;
    file << content;
    return file.good();
}

bool FileSystem::write_file_binary(const std::string& path, const void* data, size_t size) {
    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(data), size);
    return file.good();
}

bool FileSystem::file_exists(const std::string& path) {
    return std::filesystem::exists(path);
}

size_t FileSystem::get_file_size(const std::string& path) {
    if (!file_exists(path)) return 0;
    return std::filesystem::file_size(path);
}

} // namespace engine::core
