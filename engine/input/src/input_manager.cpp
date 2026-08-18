#include "engine/input/input_manager.h"
#include "engine/core/log.h"
#include <algorithm>
#include <cstring>

namespace engine::input {

void InputContext::bind_action(std::string_view action_name, core::KeyCode key) {
    std::string key_str(action_name);
    auto& action = actions[key_str];
    action.name = key_str;
    if (std::find(action.keys.begin(), action.keys.end(), key) == action.keys.end()) {
        action.keys.push_back(key);
    }
}

void InputContext::bind_action(std::string_view action_name, core::MouseButton button) {
    std::string key_str(action_name);
    auto& action = actions[key_str];
    action.name = key_str;
    if (std::find(action.mouse_buttons.begin(), action.mouse_buttons.end(), button) == action.mouse_buttons.end()) {
        action.mouse_buttons.push_back(button);
    }
}

void InputContext::bind_axis(std::string_view axis_name, core::KeyCode positive, core::KeyCode negative, float scale) {
    std::string key_str(axis_name);
    auto& axis = axes[key_str];
    axis.name = key_str;
    axis.positive_key = positive;
    axis.negative_key = negative;
    axis.scale = scale;
}

void InputContext::bind_mouse_axis(std::string_view axis_name, MouseAxis axis, float sensitivity) {
    std::string key_str(axis_name);
    auto& ax = axes[key_str];
    ax.name = key_str;
    ax.mouse_axis = axis;
    ax.scale = sensitivity;
}

InputManager& InputManager::instance() {
    static InputManager s_instance;
    return s_instance;
}

bool InputManager::init() {
    // Create default "Gameplay" context
    get_or_create_context("Gameplay");
    push_context("Gameplay");

    std::memset(m_keys_down, 0, sizeof(m_keys_down));
    std::memset(m_keys_just_pressed, 0, sizeof(m_keys_just_pressed));
    std::memset(m_keys_just_released, 0, sizeof(m_keys_just_released));
    std::memset(m_mouse_down, 0, sizeof(m_mouse_down));
    std::memset(m_mouse_just_pressed, 0, sizeof(m_mouse_just_pressed));
    std::memset(m_mouse_just_released, 0, sizeof(m_mouse_just_released));

    LOG_INFO("Input", "InputManager initialized");
    return true;
}

void InputManager::shutdown() {
    m_contexts.clear();
    m_active_context_stack.clear();
    LOG_INFO("Input", "InputManager shutdown cleanly");
}

InputContext& InputManager::get_or_create_context(std::string_view context_name) {
    std::string name(context_name);
    auto it = m_contexts.find(name);
    if (it == m_contexts.end()) {
        it = m_contexts.emplace(name, InputContext(name)).first;
    }
    return it->second;
}

void InputManager::push_context(std::string_view context_name) {
    std::string name(context_name);
    get_or_create_context(name);
    m_active_context_stack.push_back(name);
}

void InputManager::pop_context() {
    if (!m_active_context_stack.empty()) {
        m_active_context_stack.pop_back();
    }
}

void InputManager::new_frame() {
    m_mouse_delta = {0.0f, 0.0f};
    m_mouse_wheel = {0.0f, 0.0f};

    std::memset(m_keys_just_pressed, 0, sizeof(m_keys_just_pressed));
    std::memset(m_keys_just_released, 0, sizeof(m_keys_just_released));
    std::memset(m_mouse_just_pressed, 0, sizeof(m_mouse_just_pressed));
    std::memset(m_mouse_just_released, 0, sizeof(m_mouse_just_released));

    for (auto& [ctx_name, ctx] : m_contexts) {
        for (auto& [act_name, act] : ctx.actions) {
            act.just_pressed = false;
            act.just_released = false;
        }
        for (auto& [ax_name, ax] : ctx.axes) {
            if (ax.mouse_axis != MouseAxis::None) {
                ax.value = 0.0f;
            }
        }
    }
}

void InputManager::process_event(const core::PlatformEvent& event) {
    // 1. Process Global Key & Mouse States
    if (event.type == core::EventType::KeyDown || event.type == core::EventType::KeyUp) {
        bool is_down = (event.type == core::EventType::KeyDown);
        size_t key_idx = static_cast<size_t>(event.key.key);

        if (key_idx < 512) {
            if (is_down && !m_keys_down[key_idx]) {
                m_keys_just_pressed[key_idx] = true;
            } else if (!is_down && m_keys_down[key_idx]) {
                m_keys_just_released[key_idx] = true;
            }
            m_keys_down[key_idx] = is_down;
        }
    } else if (event.type == core::EventType::MouseButtonDown || event.type == core::EventType::MouseButtonUp) {
        bool is_down = (event.type == core::EventType::MouseButtonDown);
        size_t btn_idx = static_cast<size_t>(event.mouse_button.button);

        if (btn_idx < 16) {
            if (is_down && !m_mouse_down[btn_idx]) {
                m_mouse_just_pressed[btn_idx] = true;
            } else if (!is_down && m_mouse_down[btn_idx]) {
                m_mouse_just_released[btn_idx] = true;
            }
            m_mouse_down[btn_idx] = is_down;
        }
    } else if (event.type == core::EventType::MouseMove) {
        m_mouse_pos = {event.mouse_move.x, event.mouse_move.y};
        m_mouse_delta = {event.mouse_move.dx, event.mouse_move.dy};
    } else if (event.type == core::EventType::MouseWheel) {
        m_mouse_wheel = {event.mouse_wheel.x_offset, event.mouse_wheel.y_offset};
    }

    // 2. Process Context-bound Actions & Axes
    if (m_active_context_stack.empty()) return;
    const std::string& current_ctx_name = m_active_context_stack.back();
    auto ctx_it = m_contexts.find(current_ctx_name);
    if (ctx_it == m_contexts.end()) return;

    auto& ctx = ctx_it->second;

    if (event.type == core::EventType::KeyDown || event.type == core::EventType::KeyUp) {
        bool is_down = (event.type == core::EventType::KeyDown);
        core::KeyCode key = event.key.key;

        for (auto& [name, act] : ctx.actions) {
            for (auto bound_key : act.keys) {
                if (bound_key == key) {
                    if (is_down && !act.is_down) act.just_pressed = true;
                    if (!is_down && act.is_down) act.just_released = true;
                    act.is_down = is_down;
                }
            }
        }

        for (auto& [name, ax] : ctx.axes) {
            float val = 0.0f;
            if (ax.positive_key == key) val += (is_down ? 1.0f : 0.0f);
            if (ax.negative_key == key) val -= (is_down ? 1.0f : 0.0f);
            if (ax.positive_key == key || ax.negative_key == key) {
                ax.value = val * ax.scale;
            }
        }
    } else if (event.type == core::EventType::MouseButtonDown || event.type == core::EventType::MouseButtonUp) {
        bool is_down = (event.type == core::EventType::MouseButtonDown);
        core::MouseButton btn = event.mouse_button.button;

        for (auto& [name, act] : ctx.actions) {
            for (auto bound_btn : act.mouse_buttons) {
                if (bound_btn == btn) {
                    if (is_down && !act.is_down) act.just_pressed = true;
                    if (!is_down && act.is_down) act.just_released = true;
                    act.is_down = is_down;
                }
            }
        }
    } else if (event.type == core::EventType::MouseMove) {
        for (auto& [name, ax] : ctx.axes) {
            if (ax.mouse_axis == MouseAxis::DeltaX) ax.value = m_mouse_delta.x * ax.scale;
            if (ax.mouse_axis == MouseAxis::DeltaY) ax.value = m_mouse_delta.y * ax.scale;
        }
    } else if (event.type == core::EventType::MouseWheel) {
        for (auto& [name, ax] : ctx.axes) {
            if (ax.mouse_axis == MouseAxis::WheelX) ax.value = m_mouse_wheel.x * ax.scale;
            if (ax.mouse_axis == MouseAxis::WheelY) ax.value = m_mouse_wheel.y * ax.scale;
        }
    }
}

bool InputManager::is_action_down(std::string_view action_name) const {
    if (m_active_context_stack.empty()) return false;
    auto it = m_contexts.find(m_active_context_stack.back());
    if (it == m_contexts.end()) return false;

    auto act_it = it->second.actions.find(std::string(action_name));
    if (act_it != it->second.actions.end()) {
        return act_it->second.is_down;
    }
    return false;
}

bool InputManager::is_action_just_pressed(std::string_view action_name) const {
    if (m_active_context_stack.empty()) return false;
    auto it = m_contexts.find(m_active_context_stack.back());
    if (it == m_contexts.end()) return false;

    auto act_it = it->second.actions.find(std::string(action_name));
    if (act_it != it->second.actions.end()) {
        return act_it->second.just_pressed;
    }
    return false;
}

bool InputManager::is_action_just_released(std::string_view action_name) const {
    if (m_active_context_stack.empty()) return false;
    auto it = m_contexts.find(m_active_context_stack.back());
    if (it == m_contexts.end()) return false;

    auto act_it = it->second.actions.find(std::string(action_name));
    if (act_it != it->second.actions.end()) {
        return act_it->second.just_released;
    }
    return false;
}

float InputManager::get_axis(std::string_view axis_name) const {
    if (m_active_context_stack.empty()) return 0.0f;
    auto it = m_contexts.find(m_active_context_stack.back());
    if (it == m_contexts.end()) return 0.0f;

    auto ax_it = it->second.axes.find(std::string(axis_name));
    if (ax_it != it->second.axes.end()) {
        return ax_it->second.value;
    }
    return 0.0f;
}

bool InputManager::is_key_down(core::KeyCode key) const {
    size_t idx = static_cast<size_t>(key);
    return idx < 512 && m_keys_down[idx];
}

bool InputManager::is_key_pressed(core::KeyCode key) const {
    size_t idx = static_cast<size_t>(key);
    return idx < 512 && m_keys_just_pressed[idx];
}

bool InputManager::is_key_released(core::KeyCode key) const {
    size_t idx = static_cast<size_t>(key);
    return idx < 512 && m_keys_just_released[idx];
}

bool InputManager::is_mouse_button_down(core::MouseButton button) const {
    size_t idx = static_cast<size_t>(button);
    return idx < 16 && m_mouse_down[idx];
}

bool InputManager::is_mouse_button_pressed(core::MouseButton button) const {
    size_t idx = static_cast<size_t>(button);
    return idx < 16 && m_mouse_just_pressed[idx];
}

bool InputManager::is_mouse_button_released(core::MouseButton button) const {
    size_t idx = static_cast<size_t>(button);
    return idx < 16 && m_mouse_just_released[idx];
}

} // namespace engine::input
