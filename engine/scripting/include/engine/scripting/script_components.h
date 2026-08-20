#pragma once

#include "engine/core/config.h"
#include "engine/scripting/script_types.h"
#include <string>

namespace engine::scripting {

struct ScriptComponent {
    std::string script_path;
    std::string script_source;
    std::string class_name{"ScriptClass"};

    sol::table self_table;
    bool is_initialized{false};
    bool is_valid{false};
};

} // namespace engine::scripting
