#pragma once

#include "engine/core/config.h"
#include <vector>

namespace engine::ui {

class ProfilerPanel {
public:
    ProfilerPanel() = default;
    void render(float dt, uint32_t fps);

private:
    std::vector<float> m_fps_history;
};

} // namespace engine::ui
