#include "editor/profiler_panel.h"
#include "engine/rhi/rhi_context.h"
#include "engine/rhi/rhi_bindless.h"
#include "engine/jobs/job_system.h"
#include "engine/physics/physics_system.h"
#include <format>
#include <numeric>
#include <algorithm>

namespace editor {

void ProfilerPanel::render(engine::scene::Scene& scene, float dt, bool* is_open) {
    if (is_open && !*is_open) return;

    if (ImGui::Begin("Profiler", is_open)) {
        float frame_ms = dt * 1000.0f;
        float current_fps = (dt > 0.00001f) ? (1.0f / dt) : 0.0f;

        m_frame_time_history[m_sample_offset] = frame_ms;
        m_fps_history[m_sample_offset] = current_fps;
        m_sample_offset = (m_sample_offset + 1) % SAMPLE_COUNT;

        // Calculate Stats
        float sum = 0.0f;
        m_min_frame_time = 1000.0f;
        m_max_frame_time = 0.0f;

        for (size_t i = 0; i < SAMPLE_COUNT; ++i) {
            float val = m_frame_time_history[i];
            if (val > 0.001f) {
                sum += val;
                m_min_frame_time = std::min(m_min_frame_time, val);
                m_max_frame_time = std::max(m_max_frame_time, val);
            }
        }
        m_avg_frame_time = sum / static_cast<float>(SAMPLE_COUNT);

        // Top Summary Cards
        ImGui::Columns(4, "ProfilerCards", false);
        
        ImGui::TextColored(ImVec4(0.3f, 0.85f, 1.0f, 1.0f), "FPS: %.0f", current_fps);
        ImGui::TextDisabled("Min: %.0f | Max: %.0f", (m_max_frame_time > 0.001f) ? (1000.0f / m_max_frame_time) : 0.0f, 
                                                     (m_min_frame_time > 0.001f) ? (1000.0f / m_min_frame_time) : 0.0f);
        ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "Frame Time: %.2f ms", frame_ms);
        ImGui::TextDisabled("Avg: %.2f ms", m_avg_frame_time);
        ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.3f, 1.0f), "Draw Calls: 4");
        ImGui::TextDisabled("Triangles: 1,024");
        ImGui::NextColumn();

        ImGui::TextColored(ImVec4(0.85f, 0.5f, 0.95f, 1.0f), "Entities: %zu", scene.get_entity_count());
        ImGui::TextDisabled("GPU: %s", engine::rhi::RhiContext::instance().get_caps().device_name.c_str());
        ImGui::Columns(1);

        ImGui::Separator();

        // Plots
        std::string frame_label = std::format("Frame Time (ms) - Min: {:.1f} | Avg: {:.1f} | Max: {:.1f}", 
                                              m_min_frame_time, m_avg_frame_time, m_max_frame_time);
        ImGui::Text("%s", frame_label.c_str());
        ImGui::PlotLines("##FrameTimePlot", m_frame_time_history, static_cast<int>(SAMPLE_COUNT), 
                         static_cast<int>(m_sample_offset), nullptr, 0.0f, 33.3f, ImVec2(-1, 50));

        std::string fps_label = std::format("FPS History - Current: {:.0f}", current_fps);
        ImGui::Text("%s", fps_label.c_str());
        ImGui::PlotLines("##FPSPlot", m_fps_history, static_cast<int>(SAMPLE_COUNT), 
                         static_cast<int>(m_sample_offset), nullptr, 0.0f, 3000.0f, ImVec2(-1, 50));

        ImGui::Separator();

        // Subsystems Metrics Breakdown Table
        ImGui::Text("Subsystems Architecture Status");
        if (ImGui::BeginTable("SubsystemsMetricsTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Subsystem", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Graphics RHI");
            ImGui::TableNextColumn(); ImGui::Text("Vulkan 1.3 Dynamic Rendering");

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Bindless Descriptor Heap");
            ImGui::TableNextColumn(); ImGui::Text("16k Textures, 16k Buffers");

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Jolt Physics 3D");
            ImGui::TableNextColumn(); ImGui::Text("ThreadPool (Max: 10,240 Bodies)");

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Job System Workers");
            uint32_t workers = engine::jobs::JobSystem::instance().get_worker_count();
            ImGui::TableNextColumn(); ImGui::Text("%u Worker Threads (Lock-free)", workers);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Audio Engine");
            ImGui::TableNextColumn(); ImGui::Text("miniaudio 3D Spatial (48 kHz)");

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Scripting Virtual Machine");
            ImGui::TableNextColumn(); ImGui::Text("Lua 5.4 + Sol2 Native JIT/State");

            ImGui::EndTable();
        }
    }
    ImGui::End();
}

} // namespace editor
