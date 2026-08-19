#pragma once

#include "engine/core/config.h"
#include "engine/core/log.h"
#include "engine/core/memory.h"
#include "engine/core/math.h"
#include "engine/core/containers.h"
#include "engine/core/platform.h"
#include "engine/jobs/job_system.h"
#include "engine/events/event_bus.h"
#include "engine/config/cvar.h"
#include "engine/config/config_system.h"
#include "engine/vfs/vfs.h"
#include "engine/assets/uuid.h"
#include "engine/assets/asset_manager.h"
#include "engine/project/project.h"
#include "engine/input/input_manager.h"
#include "engine/importer/importer.h"
#include "engine/rhi/rhi_context.h"
#include "engine/rhi/rhi_swapchain.h"
#include "engine/rhi/rhi_command_buffer.h"
#include "engine/rhi/rhi_sync.h"
#include "engine/rhi/rhi_pipeline.h"
#include "engine/rhi/rhi_buffer.h"
#include "engine/rhi/rhi_texture.h"
#include "engine/rhi/rhi_bindless.h"
#include "engine/scene/components.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "engine/scene/map_serializer.h"
#include "engine/scene/scene_importer.h"
#include "engine/renderer/render_graph.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/material.h"
#include "engine/renderer/gpu_scene.h"
#include "engine/renderer/lighting.h"
#include "engine/renderer/shadow_map.h"
#include "engine/renderer/volumetrics.h"
#include "engine/renderer/oit.h"
#include "engine/renderer/restir.h"
#include "engine/renderer/post_process.h"
#include "engine/renderer/taa.h"
#include "engine/renderer/bloom.h"
#include "engine/renderer/auto_exposure.h"
#include "engine/physics/physics_types.h"
#include "engine/physics/physics_components.h"
#include "engine/physics/physics_system.h"
#include "engine/audio/audio_types.h"
#include "engine/audio/audio_components.h"
#include "engine/audio/audio_engine.h"
#include "engine/scripting/script_types.h"
#include "engine/scripting/script_components.h"
#include "engine/scripting/script_engine.h"
#include "engine/ui/editor_ui.h"
#include "engine/ui/editor_camera.h"
#include "engine/ui/panels/viewport_panel.h"

namespace engine {

struct EngineDesc {
    std::string title{"Modern Game Engine"};
    uint32_t width{1280};
    uint32_t height{720};
    bool enable_vsync{true};
    bool enable_validation{true};
    bool enable_editor_ui{true};
    std::string project_manifest_path{""};
};

class Engine {
public:
    static Engine& instance();

    bool init(const EngineDesc& desc = {});
    void run();
    void step();
    void shutdown();

    bool is_running() const;
    void request_exit();

    core::Window& get_window() { return m_window; }
    scene::Scene& get_active_scene() { return m_active_scene; }
    renderer::RenderGraph& get_render_graph() { return m_render_graph; }

private:
    Engine();
    ~Engine();

    EngineDesc m_desc{};
    core::Window m_window;
    rhi::RhiSwapchain m_swapchain;

    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
    rhi::RhiCommandPool m_cmd_pool;
    rhi::RhiCommandBuffer m_cmd_buffers[MAX_FRAMES_IN_FLIGHT];
    rhi::RhiFence m_in_flight_fences[MAX_FRAMES_IN_FLIGHT];
    rhi::RhiSemaphore m_image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    std::vector<rhi::RhiSemaphore> m_render_finished_semaphores;

    rhi::RhiShaderModule m_vert_shader;
    rhi::RhiShaderModule m_frag_shader;
    rhi::RhiGraphicsPipeline m_scene_pipeline;

    renderer::RenderGraph m_render_graph;
    scene::Scene m_active_scene;
    core::FrameTimer m_timer;
    core::DynamicArray<core::PlatformEvent> m_events;

    uint32_t m_current_frame{0};
    uint32_t m_rendered_frames{0};
    bool m_initialized{false};
    bool m_running{false};
};

} // namespace engine
