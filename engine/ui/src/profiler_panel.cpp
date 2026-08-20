#include "engine/ui/panels/profiler_panel.h"
#include "engine/core/memory.h"
#include "engine/rhi/rhi_context.h"
#include <imgui.h>

namespace engine::ui {

void ProfilerPanel::render(float dt, uint32_t fps) {
    ImGui::Begin("Profiler & Console");

    if (m_fps_history.size() > 120) {
        m_fps_history.erase(m_fps_history.begin());
    }
    m_fps_history.push_back(static_cast<float>(fps));

    ImGui::Text("GPU: %s", rhi::RhiContext::instance().get_caps().device_name.c_str());
    ImGui::Text("FPS: %u (%.2f ms/frame)", fps, dt * 1000.0f);

    if (!m_fps_history.empty()) {
        ImGui::PlotLines("FPS History", m_fps_history.data(), static_cast<int>(m_fps_history.size()), 0, nullptr, 0.0f, 5000.0f, ImVec2(0, 80));
    }

    ImGui::Separator();
    const auto& mem = core::GlobalAllocator::instance().get_stats();
    ImGui::Text("Allocations: %zu | Deallocations: %zu", mem.allocation_count, mem.deallocation_count);
    ImGui::Text("Active Heap Allocations: %zu", (mem.allocation_count >= mem.deallocation_count) ? (mem.allocation_count - mem.deallocation_count) : 0);
    ImGui::Text("Active Heap Bytes: %zu B", mem.total_allocated);
    ImGui::Text("Peak Heap Bytes: %zu B", mem.peak_allocated);

    ImGui::End();
}

} // namespace engine::ui
