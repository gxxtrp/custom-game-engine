#include "engine/config/cvar.h"
#include "engine/core/log.h"

namespace engine::config {

CVarRegistry& CVarRegistry::instance() {
    static CVarRegistry s_instance;
    return s_instance;
}

void CVarRegistry::register_cvar(ICVar* cvar) {
    if (!cvar) return;
    std::string key(cvar->get_name());
    m_cvars[key] = cvar;
}

void CVarRegistry::unregister_cvar(ICVar* cvar) {
    if (!cvar) return;
    std::string key(cvar->get_name());
    m_cvars.erase(key);
}

ICVar* CVarRegistry::find(std::string_view name) {
    auto it = m_cvars.find(std::string(name));
    if (it != m_cvars.end()) {
        return it->second;
    }
    return nullptr;
}

} // namespace engine::config
