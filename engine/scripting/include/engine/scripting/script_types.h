#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include <string>
#include <string_view>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace engine::scripting {

struct ScriptResult {
    bool success{false};
    std::string error_message;
};

} // namespace engine::scripting
