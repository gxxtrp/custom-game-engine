#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include <string>
#include <string_view>
#include <functional>
#include <unordered_map>
#include <memory>
#include <vector>
#include <format>
#include <sstream>

namespace engine::config {

enum class CVarFlags : uint32_t {
    None            = 0,
    SaveToDisk      = 1 << 0,
    ReadOnly        = 1 << 1,
    Cheat           = 1 << 2,
    RequiresRestart = 1 << 3
};

ENGINE_FORCE_INLINE CVarFlags operator|(CVarFlags a, CVarFlags b) {
    return static_cast<CVarFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

ENGINE_FORCE_INLINE bool operator&(CVarFlags a, CVarFlags b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

class ICVar {
public:
    ICVar(std::string_view name, std::string_view description, CVarFlags flags)
        : m_name(name), m_description(description), m_flags(flags) {}
    virtual ~ICVar() = default;

    std::string_view get_name() const { return m_name; }
    std::string_view get_description() const { return m_description; }
    CVarFlags get_flags() const { return m_flags; }

    virtual std::string to_string() const = 0;
    virtual bool from_string(std::string_view str) = 0;
    virtual void reset_to_default() = 0;

protected:
    std::string m_name;
    std::string m_description;
    CVarFlags m_flags{CVarFlags::None};
};

template<typename T>
class CVar : public ICVar {
public:
    using ChangeCallback = std::function<void(const T& old_val, const T& new_val)>;

    CVar(std::string_view name, const T& default_value, std::string_view description, CVarFlags flags = CVarFlags::None)
        : ICVar(name, description, flags), m_value(default_value), m_default_value(default_value) {}

    const T& get() const { return m_value; }
    const T& get_default() const { return m_default_value; }

    bool set(const T& new_value) {
        if (m_flags & CVarFlags::ReadOnly) return false;
        if (m_value != new_value) {
            T old = m_value;
            m_value = new_value;
            for (const auto& cb : m_callbacks) {
                if (cb) cb(old, m_value);
            }
        }
        return true;
    }

    void on_changed(ChangeCallback cb) {
        m_callbacks.push_back(std::move(cb));
    }

    void reset_to_default() override {
        set(m_default_value);
    }

    std::string to_string() const override {
        if constexpr (std::is_same_v<T, bool>) {
            return m_value ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t>) {
            return std::to_string(m_value);
        } else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
            return std::format("{:.4f}", m_value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return m_value;
        } else if constexpr (std::is_same_v<T, engine::core::Vec3>) {
            return std::format("{:.3f} {:.3f} {:.3f}", m_value.x, m_value.y, m_value.z);
        } else {
            return "<unsupported>";
        }
    }

    bool from_string(std::string_view str) override {
        if (m_flags & CVarFlags::ReadOnly) return false;

        if constexpr (std::is_same_v<T, bool>) {
            if (str == "true" || str == "1" || str == "on") return set(true);
            if (str == "false" || str == "0" || str == "off") return set(false);
            return false;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            try {
                return set(std::stoi(std::string(str)));
            } catch (...) { return false; }
        } else if constexpr (std::is_same_v<T, float>) {
            try {
                return set(std::stof(std::string(str)));
            } catch (...) { return false; }
        } else if constexpr (std::is_same_v<T, std::string>) {
            return set(std::string(str));
        } else if constexpr (std::is_same_v<T, engine::core::Vec3>) {
            std::istringstream iss{std::string(str)};
            float x = 0, y = 0, z = 0;
            if (iss >> x >> y >> z) {
                return set(engine::core::Vec3(x, y, z));
            }
            return false;
        }
        return false;
    }

private:
    T m_value;
    T m_default_value;
    std::vector<ChangeCallback> m_callbacks;
};

class CVarRegistry {
public:
    static CVarRegistry& instance();

    void register_cvar(ICVar* cvar);
    void unregister_cvar(ICVar* cvar);

    ICVar* find(std::string_view name);
    const std::unordered_map<std::string, ICVar*>& get_all() const { return m_cvars; }

private:
    CVarRegistry() = default;
    std::unordered_map<std::string, ICVar*> m_cvars;
};

// Global CVar helper for automatic registration
template<typename T>
class AutoCVar : public CVar<T> {
public:
    AutoCVar(std::string_view name, const T& default_value, std::string_view description, CVarFlags flags = CVarFlags::None)
        : CVar<T>(name, default_value, description, flags) {
        CVarRegistry::instance().register_cvar(this);
    }

    ~AutoCVar() override {
        CVarRegistry::instance().unregister_cvar(this);
    }
};

} // namespace engine::config
