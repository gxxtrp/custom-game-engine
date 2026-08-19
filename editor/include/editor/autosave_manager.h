#pragma once

#include "engine/scene/scene.h"
#include <string>
#include <vector>

namespace editor {

class AutosaveManager {
public:
    explicit AutosaveManager(float interval_seconds = 60.0f, size_t max_backups = 5);
    ~AutosaveManager() = default;

    void update(engine::scene::Scene& scene, float dt);
    bool trigger_autosave(engine::scene::Scene& scene);

    void set_interval(float seconds) { m_interval_seconds = seconds; }
    float get_interval() const { return m_interval_seconds; }

    void set_enabled(bool enabled) { m_enabled = enabled; }
    bool is_enabled() const { return m_enabled; }

    float get_time_until_next_autosave() const { 
        return std::max(0.0f, m_interval_seconds - m_timer); 
    }

private:
    void prune_old_autosaves();

    float m_interval_seconds{60.0f};
    size_t m_max_backups{5};
    float m_timer{0.0f};
    bool m_enabled{true};
    std::string m_autosave_dir{".intermediate/autosave"};
};

} // namespace editor
