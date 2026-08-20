#pragma once

#include "engine/scene/scene.h"
#include "engine/core/platform.h"
#include <imgui.h>
#include <cstdint>

namespace editor {

class ProfilerPanel {
public:
    ProfilerPanel() = default;
    ~ProfilerPanel() = default;

    void render(engine::scene::Scene& scene, float dt, bool* is_open = nullptr);

private:
    static constexpr size_t SAMPLE_COUNT = 120;
    float m_frame_time_history[SAMPLE_COUNT]{0.0f};
    float m_fps_history[SAMPLE_COUNT]{0.0f};
    size_t m_sample_offset{0};

    float m_min_frame_time{1000.0f};
    float m_max_frame_time{0.0f};
    float m_avg_frame_time{0.0f};
};

} // namespace editor
