#pragma once

#include "engine/core/platform.h"
#include "engine/core/engine_kernel.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_subsystem.h"
#include "engine/rhi/rhi_context.h"
#include "engine/rhi/viewport_presenter.h"
#include "engine/rhi/window_swapchain_presenter.h"
#include "engine/rhi/headless_presenter.h"
#include "engine/rhi/rhi_command_buffer.h"
#include "engine/rhi/rhi_sync.h"
#include "engine/renderer/render_graph.h"
#include "engine/renderer/scene_renderer.h"
#include <string>
#include <vector>
#include <memory>

namespace runtime {

struct RuntimeAppDesc {
    std::string project_path{""};
    float timeout{0.0f};
    bool enable_validation{true};
    bool headless{false};
};

class RuntimeApp {
public:
    static RuntimeApp& instance();

    bool init(const RuntimeAppDesc& desc = {});
    void run();
    void step();
    void shutdown();

    bool is_running() const;
    void request_exit();

    engine::scene::Scene& get_scene();
    engine::core::EngineKernel& get_kernel() { return *m_kernel; }

private:
    RuntimeApp();
    ~RuntimeApp();

    RuntimeAppDesc m_desc{};
    engine::core::Window m_window;
    std::unique_ptr<engine::rhi::IViewportPresenter> m_presenter;
    std::unique_ptr<engine::core::EngineKernel> m_kernel;

    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
    engine::rhi::RhiCommandPool m_cmd_pool;
    engine::rhi::RhiCommandBuffer m_cmd_buffers[MAX_FRAMES_IN_FLIGHT];
    engine::rhi::RhiFence m_in_flight_fences[MAX_FRAMES_IN_FLIGHT];
    engine::rhi::RhiSemaphore m_image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    std::vector<engine::rhi::RhiSemaphore> m_render_finished_semaphores;

    engine::renderer::RenderGraph m_render_graph;
    engine::core::FrameTimer m_timer;

    uint32_t m_current_frame{0};
    uint32_t m_rendered_frames{0};
    bool m_initialized{false};
    bool m_running{false};
};

} // namespace runtime
