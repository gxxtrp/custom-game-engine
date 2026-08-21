#include "engine/engine.h"
#include "engine/renderer/embedded_shaders.h"

namespace engine {

Engine::Engine() : m_active_scene("DefaultEngineScene") {}

Engine::~Engine() {
    shutdown();
}

Engine& Engine::instance() {
    static Engine s_instance;
    return s_instance;
}

bool Engine::init(const EngineDesc& desc) {
    if (m_initialized) return true;
    m_desc = desc;

    LOG_INFO("Engine", "==================================================");
    LOG_INFO("Engine", "    Initializing Modern Game Engine Core Stack    ");
    LOG_INFO("Engine", "==================================================");

    // 1. Platform & Multithreading
    if (!core::Platform::init()) return false;
    if (!jobs::JobSystem::instance().init()) return false;
    if (!assets::AssetManager::instance().init()) return false;
    if (!input::InputManager::instance().init()) return false;

    // 2. Physics, Audio & Scripting
    if (!physics::PhysicsSystem::instance().init()) return false;
    if (!audio::AudioEngine::instance().init()) return false;
    if (!scripting::ScriptEngine::instance().init()) return false;

    // 3. Project & VFS
    if (!desc.project_manifest_path.empty()) {
        project::ProjectManager::instance().load_project(desc.project_manifest_path);
    }

    // 4. Window & Vulkan 1.3 RHI
    core::WindowDesc win_desc{
        .title = desc.title,
        .width = desc.width,
        .height = desc.height,
        .resizable = true,
        .vulkan_compatible = true
    };

    if (!m_window.create(win_desc)) {
        LOG_FATAL("Engine", "Failed to create main window!");
        return false;
    }

    if (!rhi::RhiContext::instance().init(m_window, desc.enable_validation)) {
        LOG_FATAL("Engine", "Failed to initialize Vulkan 1.3 RHI!");
        return false;
    }

    if (!rhi::BindlessHeap::instance().init()) {
        LOG_FATAL("Engine", "Failed to initialize Bindless Heap!");
        return false;
    }

    // 5. Swapchain & Command Synchronization
    if (!m_swapchain.init(m_window.get_width(), m_window.get_height(), desc.enable_vsync)) {
        LOG_FATAL("Engine", "Failed to create RHI swapchain!");
        return false;
    }

    m_cmd_pool.init(rhi::RhiContext::instance().get_queue_families().graphics_family);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_cmd_buffers[i].init(m_cmd_pool.get_handle());
        m_in_flight_fences[i].init(true);
        m_image_available_semaphores[i].init(false);
    }

    uint32_t image_count = m_swapchain.get_image_count();
    m_render_finished_semaphores.resize(image_count);
    for (size_t i = 0; i < image_count; ++i) {
        m_render_finished_semaphores[i].init(false);
    }

    // 6. Graphics Pipeline
    if (!m_vert_shader.init_from_spirv(shaders::TRIANGLE_VERT_SPV, shaders::TRIANGLE_VERT_SPV_SIZE, rhi::ShaderStage::Vertex)) {
        LOG_FATAL("Engine", "Failed to create triangle vertex shader module!");
        return false;
    }

    if (!m_frag_shader.init_from_spirv(shaders::TRIANGLE_FRAG_SPV, shaders::TRIANGLE_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment)) {
        LOG_FATAL("Engine", "Failed to create triangle fragment shader module!");
        return false;
    }

    rhi::GraphicsPipelineDesc pipeline_desc{};
    pipeline_desc.vertex_shader = &m_vert_shader;
    pipeline_desc.fragment_shader = &m_frag_shader;
    pipeline_desc.color_formats = { static_cast<rhi::Format>(m_swapchain.get_format()) };
    pipeline_desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (!m_scene_pipeline.init(pipeline_desc)) {
        LOG_FATAL("Engine", "Failed to create scene graphics pipeline!");
        return false;
    }

    // 7. Register Scene Subsystems
    physics::PhysicsSystem::instance().register_scene(m_active_scene);
    audio::AudioEngine::instance().register_scene(m_active_scene);
    scripting::ScriptEngine::instance().register_scene(m_active_scene);

    m_initialized = true;
    m_running = true;

    LOG_INFO("Engine", "==================================================");
    LOG_INFO("Engine", "   Modern Game Engine Initialized Successfully    ");
    LOG_INFO("Engine", "==================================================");
    return true;
}

bool Engine::is_running() const {
    return m_running && m_window.is_open();
}

void Engine::request_exit() {
    m_running = false;
    m_window.set_should_close(true);
}

void Engine::step() {
    if (!m_initialized || !is_running()) return;

    m_timer.tick();
    input::InputManager::instance().new_frame();

    // 1. Poll Platform Events
    m_events.clear();
    core::Platform::poll_events(m_events);

    for (const auto& event : m_events) {
        core::Platform::process_window_events(m_window, event);
        input::InputManager::instance().process_event(event);

        if (event.type == core::EventType::KeyDown && event.key.key == core::KeyCode::Escape) {
            request_exit();
        } else if (event.type == core::EventType::WindowResize) {
            m_swapchain.resize(event.window_resize.width, event.window_resize.height);
        }
    }

    float dt = static_cast<float>(m_timer.delta_time());

    // 2. Physics & Audio Ticks
    if (dt > 0.0f && dt < 0.1f) {
        physics::PhysicsSystem::instance().update(dt);
        audio::AudioEngine::instance().update(dt);
        scripting::ScriptEngine::instance().sync_ecs_scripts(m_active_scene, dt);
    }

    // 3. Acquire Next Swapchain Image
    m_in_flight_fences[m_current_frame].wait();

    uint32_t image_index = 0;
    VkResult acquire_res = m_swapchain.acquire_next_image(
        m_image_available_semaphores[m_current_frame].get_handle(),
        VK_NULL_HANDLE,
        image_index
    );

    if (acquire_res == VK_ERROR_OUT_OF_DATE_KHR) {
        m_swapchain.resize(m_window.get_width(), m_window.get_height());
        return;
    }

    m_in_flight_fences[m_current_frame].reset();

    // 4. Build Frame RenderGraph
    m_render_graph.reset();

    renderer::RGTextureHandle swapchain_rg = m_render_graph.import_texture(
        "SwapchainOutput",
        m_swapchain.get_image(image_index),
        m_swapchain.get_image_view(image_index),
        m_swapchain.get_extent().width,
        m_swapchain.get_extent().height,
        static_cast<rhi::Format>(m_swapchain.get_format()),
        VK_IMAGE_LAYOUT_UNDEFINED
    );

    // Pass 1: Scene Graphics Pass
    m_render_graph.add_pass(
        "ScenePass",
        [&](renderer::RenderPassBuilder& builder) {
            builder.set_color_attachment(
                0,
                swapchain_rg,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                core::Vec4(0.08f, 0.09f, 0.11f, 1.0f)
            );
        },
        [this](renderer::RenderPassContext& ctx) {
            auto& cmd = ctx.get_command_buffer();
            cmd.set_viewport(rhi::Viewport{
                .x = 0.0f,
                .y = 0.0f,
                .width = static_cast<float>(m_swapchain.get_extent().width),
                .height = static_cast<float>(m_swapchain.get_extent().height),
                .min_depth = 0.0f,
                .max_depth = 1.0f
            });
            cmd.set_scissor(rhi::Rect2D{
                .offset_x = 0,
                .offset_y = 0,
                .width = m_swapchain.get_extent().width,
                .height = m_swapchain.get_extent().height
            });
            cmd.bind_pipeline(m_scene_pipeline.get_pipeline());
            cmd.draw(3, 1, 0, 0);
        }
    );

    // Pass 2: Present Transition Pass
    m_render_graph.add_pass(
        "PresentTransitionPass",
        [&](renderer::RenderPassBuilder& builder) {
            builder.read_texture(swapchain_rg, renderer::RGResourceAccess::Present);
        },
        [](renderer::RenderPassContext&) {}
    );

    // Execute RenderGraph
    auto& cmd = m_cmd_buffers[m_current_frame];
    cmd.reset();
    cmd.begin();

    m_render_graph.compile();
    m_render_graph.execute(cmd);

    cmd.end();

    // 5. Submit Command Buffer
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = { m_image_available_semaphores[m_current_frame].get_handle() };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    VkCommandBuffer raw_cmd = cmd.get_handle();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &raw_cmd;

    VkSemaphore signal_semaphores[] = { m_render_finished_semaphores[image_index].get_handle() };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkQueueSubmit(rhi::RhiContext::instance().get_graphics_queue(), 1, &submit_info, m_in_flight_fences[m_current_frame].get_handle());

    // 6. Present Swapchain Image
    VkResult present_res = m_swapchain.present(
        rhi::RhiContext::instance().get_graphics_queue(),
        m_render_finished_semaphores[image_index].get_handle(),
        image_index
    );

    if (present_res == VK_ERROR_OUT_OF_DATE_KHR || present_res == VK_SUBOPTIMAL_KHR) {
        m_swapchain.resize(m_window.get_width(), m_window.get_height());
    }

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    m_rendered_frames++;

    if (m_timer.fps() > 0) {
        std::string title = std::format("{} | GPU: {} | FPS: {} | Frames: {}", 
                                        m_desc.title, 
                                        rhi::RhiContext::instance().get_caps().device_name, 
                                        m_timer.fps(), 
                                        m_rendered_frames);
        m_window.set_title(title);
    }
}

void Engine::run() {
    while (is_running()) {
        step();
    }
}

void Engine::shutdown() {
    if (!m_initialized) return;

    rhi::RhiContext::instance().wait_idle();

    m_render_graph.destroy();
    m_scene_pipeline.destroy();
    m_vert_shader.destroy();
    m_frag_shader.destroy();

    for (auto& sem : m_render_finished_semaphores) {
        sem.destroy();
    }
    m_render_finished_semaphores.clear();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_in_flight_fences[i].destroy();
        m_image_available_semaphores[i].destroy();
        m_cmd_buffers[i].destroy(m_cmd_pool.get_handle());
    }
    m_cmd_pool.destroy();
    m_swapchain.destroy();

    rhi::BindlessHeap::instance().shutdown();
    rhi::RhiContext::instance().shutdown();

    m_window.destroy();
    project::ProjectManager::instance().close_project();
    scripting::ScriptEngine::instance().shutdown();
    audio::AudioEngine::instance().shutdown();
    physics::PhysicsSystem::instance().shutdown();
    input::InputManager::instance().shutdown();
    assets::AssetManager::instance().shutdown();
    jobs::JobSystem::instance().shutdown();
    core::Platform::shutdown();

    m_initialized = false;
    LOG_INFO("Engine", "Modern Game Engine shutdown completed cleanly.");
}

} // namespace engine
