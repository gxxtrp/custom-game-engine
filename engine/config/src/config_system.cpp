#include "engine/config/config_system.h"
#include "engine/config/cvar.h"
#include "engine/core/log.h"
#include "engine/core/platform.h"
#include <toml++/toml.hpp>
#include <sstream>

namespace engine::config {

ConfigSystem& ConfigSystem::instance() {
    static ConfigSystem s_instance;
    return s_instance;
}

static void apply_toml_table(const toml::table& tbl, const std::string& prefix) {
    for (auto&& [key, node] : tbl) {
        std::string full_key = prefix.empty() ? std::string(key.str()) : (prefix + "." + std::string(key.str()));

        if (node.is_table()) {
            apply_toml_table(*node.as_table(), full_key);
        } else if (node.is_string()) {
            if (auto* cvar = CVarRegistry::instance().find(full_key)) {
                cvar->from_string(node.as_string()->get());
            }
        } else if (node.is_integer()) {
            if (auto* cvar = CVarRegistry::instance().find(full_key)) {
                cvar->from_string(std::to_string(node.as_integer()->get()));
            }
        } else if (node.is_floating_point()) {
            if (auto* cvar = CVarRegistry::instance().find(full_key)) {
                cvar->from_string(std::to_string(node.as_floating_point()->get()));
            }
        } else if (node.is_boolean()) {
            if (auto* cvar = CVarRegistry::instance().find(full_key)) {
                cvar->from_string(node.as_boolean()->get() ? "true" : "false");
            }
        } else if (node.is_array()) {
            if (auto* cvar = CVarRegistry::instance().find(full_key)) {
                const auto* arr = node.as_array();
                std::ostringstream oss;
                for (size_t i = 0; i < arr->size(); ++i) {
                    if (arr->get(i)->is_floating_point()) {
                        oss << arr->get(i)->as_floating_point()->get() << " ";
                    } else if (arr->get(i)->is_integer()) {
                        oss << arr->get(i)->as_integer()->get() << " ";
                    }
                }
                cvar->from_string(oss.str());
            }
        }
    }
}

bool ConfigSystem::load_from_file(const std::string& filepath) {
    std::string content;
    if (!engine::core::FileSystem::read_file_string(filepath, content)) {
        LOG_WARN("Config", "Config file not found or failed to read: {}", filepath);
        return false;
    }
    return load_string(content);
}

bool ConfigSystem::load_string(std::string_view toml_content) {
    try {
        auto tbl = toml::parse(toml_content);
        apply_toml_table(tbl, "");
        LOG_INFO("Config", "Configuration parsed successfully");
        return true;
    } catch (const toml::parse_error& err) {
        LOG_ERROR("Config", "Failed to parse TOML configuration: {}", err.description());
        return false;
    }
}

bool ConfigSystem::save_to_file(const std::string& filepath) {
    std::string toml_str = to_string();
    if (engine::core::FileSystem::write_file_string(filepath, toml_str)) {
        LOG_INFO("Config", "Configuration saved to: {}", filepath);
        return true;
    }
    LOG_ERROR("Config", "Failed to write configuration file: {}", filepath);
    return false;
}

std::string ConfigSystem::to_string() const {
    toml::table root;

    const auto& cvars = CVarRegistry::instance().get_all();
    for (const auto& [name, cvar] : cvars) {
        if (!(cvar->get_flags() & CVarFlags::SaveToDisk)) continue;

        // Break name by dot into sections if present
        size_t dot_pos = name.find('.');
        if (dot_pos != std::string::npos) {
            std::string section = name.substr(0, dot_pos);
            std::string key = name.substr(dot_pos + 1);

            if (!root.contains(section)) {
                root.insert(section, toml::table{});
            }
            auto* sec_tbl = root[section].as_table();
            sec_tbl->insert_or_assign(key, cvar->to_string());
        } else {
            root.insert_or_assign(name, cvar->to_string());
        }
    }

    std::stringstream ss;
    ss << root;
    return ss.str();
}

} // namespace engine::config
