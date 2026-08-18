#pragma once

#include "engine/core/config.h"
#include "engine/core/platform.h"
#include "engine/core/math.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine::input {

enum class MouseAxis : uint8_t {
    None = 0,
    DeltaX,
    DeltaY,
    WheelX,
    WheelY
};

struct ActionBinding {
    std::string name;
    std::vector<core::KeyCode> keys;
    std::vector<core::MouseButton> mouse_buttons;
    bool is_down{false};
    bool just_pressed{false};
    bool just_released{false};
};

struct AxisBinding {
    std::string name;
    core::KeyCode positive_key{core::KeyCode::Unknown};
    core::KeyCode negative_key{core::KeyCode::Unknown};
    MouseAxis mouse_axis{MouseAxis::None};
    float scale{1.0f};
    float value{0.0f};
};

class InputContext {
public:
    explicit InputContext(std::string name) : m_name(std::move(name)) {}

    void bind_action(std::string_view action_name, core::KeyCode key);
    void bind_action(std::string_view action_name, core::MouseButton button);
    void bind_axis(std::string_view axis_name, core::KeyCode positive, core::KeyCode negative, float scale = 1.0f);
    void bind_mouse_axis(std::string_view axis_name, MouseAxis axis, float sensitivity = 1.0f);

    std::string_view get_name() const { return m_name; }

    std::unordered_map<std::string, ActionBinding> actions;
    std::unordered_map<std::string, AxisBinding> axes;

private:
    std::string m_name;
};

class InputManager {
public:
    static InputManager& instance();

    bool init();
    void shutdown();

    InputContext& get_or_create_context(std::string_view context_name);
    void push_context(std::string_view context_name);
    void pop_context();

    void process_event(const core::PlatformEvent& event);
    void new_frame();

    bool is_action_down(std::string_view action_name) const;
    bool is_action_just_pressed(std::string_view action_name) const;
    bool is_action_just_released(std::string_view action_name) const;
    float get_axis(std::string_view axis_name) const;

    // Direct Key & Mouse Queries
    bool is_key_down(core::KeyCode key) const;
    bool is_key_pressed(core::KeyCode key) const;
    bool is_key_released(core::KeyCode key) const;

    bool is_mouse_button_down(core::MouseButton button) const;
    bool is_mouse_button_pressed(core::MouseButton button) const;
    bool is_mouse_button_released(core::MouseButton button) const;

    core::Vec2 get_mouse_position() const { return m_mouse_pos; }
    core::Vec2 get_mouse_delta() const { return m_mouse_delta; }

private:
    InputManager() = default;

    std::unordered_map<std::string, InputContext> m_contexts;
    std::vector<std::string> m_active_context_stack;

    bool m_keys_down[512]{false};
    bool m_keys_just_pressed[512]{false};
    bool m_keys_just_released[512]{false};

    bool m_mouse_down[16]{false};
    bool m_mouse_just_pressed[16]{false};
    bool m_mouse_just_released[16]{false};

    core::Vec2 m_mouse_pos{0.0f, 0.0f};
    core::Vec2 m_mouse_delta{0.0f, 0.0f};
    core::Vec2 m_mouse_wheel{0.0f, 0.0f};
};

} // namespace engine::input
