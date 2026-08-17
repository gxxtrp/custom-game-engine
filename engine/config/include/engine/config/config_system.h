#pragma once

#include "engine/core/config.h"
#include "engine/config/cvar.h"
#include <string>
#include <string_view>

namespace engine::config {

class ConfigSystem {
public:
    static ConfigSystem& instance();

    bool load_from_file(const std::string& filepath);
    bool save_to_file(const std::string& filepath);

    bool load_string(std::string_view toml_content);
    std::string to_string() const;

private:
    ConfigSystem() = default;
};

} // namespace engine::config
