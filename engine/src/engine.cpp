#include "engine/engine.h"
#include "engine/renderer/embedded_shaders.h"

namespace engine {

Engine::Engine() = default;

Engine::~Engine() {
    shutdown();
}

Engine& Engine::instance() {
    static Engine s_instance;
    return s_instance;
}

scene::Scene& Engine::get_active_scene() {
    return m_kernel->get_context().get<scene::SceneSubsystem>().get_active_scene();
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

    // 2. Project & VFS
    if (!desc.project_manifest_path.empty()) {
        project::ProjectManager::instance().load_project(desc.project_manifest_path);
    }

    // 3. Presenter & Vulkan 1.3 RHI
    if (!desc.headless) {
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

        m_presenter = std::make_unique<rhi::WindowSwapchainPresenter>();
        if (!m_presenter->initialize(m_window.get_width(), m_window.get_height(), desc.enable_vsync)) {
            LOG_FATAL("Engine", "Failed to create Viewport Presenter!");
            return false;
        }

        m_cmd_pool.init(rhi::RhiContext::instance().get_queue_families().graphics_family);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            m_cmd_buffers[i].init(m_cmd_pool.get_handle());
            m_in_flight_fences[i].init(true);
            m_image_available_semaphores[i].init(false);
        }

        uint32_t image_count = m_presenter->get_image_count();
        m_render_finished_semaphores.resize(image_count);
        for (size_t i = 0; i < image_count; ++i) {
            m_render_finished_semaphores[i].init(false);
        }

        // Graphics Pipeline
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
        pipeline_desc.color_formats = { static_cast<rhi::Format>(m_presenter->get_format()) };
        pipeline_desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        if (!m_scene_pipeline.init(pipeline_desc)) {
            LOG_FATAL("Engine", "Failed to create scene graphics pipeline!");
            return false;
        }
    } else {
        LOG_INFO("Engine", "Initializing in headless mode");
        m_presenter = std::make_unique<rhi::HeadlessPresenter>();
        m_presenter->initialize(1280, 720, false);
    }

    // 4. Construct & Initialize EngineKernel DAG
    core::KernelBuilder builder;
    builder.add_subsystem<scene::SceneSubsystem>("DefaultEngineScene");
    builder.add_subsystem<physics::PhysicsSystem>();
    builder.add_subsystem<audio::AudioEngine>();
    builder.add_subsystem<scripting::ScriptEngine>();

    m_kernel = builder.build();
    if (!m_kernel->initialize()) {
        LOG_FATAL("Engine", "Failed to initialize EngineKernel!");
        return false;
    }

    m_initialized = true;
    m_running = true;

    LOG_INFO("Engine", "==================================================");
    LOG_INFO("Engine", "   Modern Game Engine Initialized Successfully    ");
    LOG_INFO("Engine", "==================================================");
    return true;
}

bool Engine::is_running() const {
    return m_running && (m_desc.headless || m_window.is_open());
}

void Engine::request_exit() {
    m_running = false;
    if (!m_desc.headless) {
        m_window.set_should_close(true);
    }
}

void Engine::step() {
    if (!m_initialized || !is_running()) return;

    m_timer.tick();
    input::InputManager::instance().new_frame();

    // 1. Poll Platform Events
    m_events.clear();
    core::Platform::poll_events(m_events);

    for (const auto& event : m_events) {
        if (!m_desc.headless) {
            core::Platform::process_window_events(m_window, event);
        }
        input::InputManager::instance().process_event(event);

        if (event.type == core::EventType::KeyDown && event.key.key == core::KeyCode::Escape) {
            request_exit();
        } else if (!m_desc.headless && event.type == core::EventType::WindowResize) {
            m_presenter->resize(event.window_resize.width, event.window_resize.height);
        }
    }

    float dt = static_cast<float>(m_timer.delta_time());

    // 2. Kernel Frame Step (Executes PreTick, Simulation, PostSimulation)
    m_kernel->tick(core::ExecutionPhase::PreTick, dt);
    m_kernel->tick(core::ExecutionPhase::Simulation, dt);
    m_kernel->tick(core::ExecutionPhase::PostSimulation, dt);

    if (m_desc.headless) {
        m_kernel->tick(core::ExecutionPhase::Render, dt);
        m_kernel->tick(core::ExecutionPhase::Present, dt);
        m_rendered_frames++;
        core::Clock::sleep_ms(1);
        return;
    }

    // 3. Acquire Next Swapchain Image
    m_in_flight_fences[m_current_frame].wait();

    uint32_t image_index = 0;
    VkResult acquire_res = m_presenter->acquire_next_image(
        m_image_available_semaphores[m_current_frame].get_handle(),
        VK_NULL_HANDLE,
        image_index
    );

    if (acquire_res == VK_ERROR_OUT_OF_DATE_KHR) {
        m_presenter->resize(m_window.get_width(), m_window.get_height());
        return;
    }

    m_in_flight_fences[m_current_frame].reset();

    // 4. Build Frame RenderGraph
    m_render_graph.reset();

    renderer::RGTextureHandle swapchain_rg = m_render_graph.import_texture(
        "SwapchainOutput",
        m_presenter->get_image(image_index),
        m_presenter->get_image_view(image_index),
        m_presenter->get_width(),
        m_presenter->get_height(),
        static_cast<rhi::Format>(m_presenter->get_format()),
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
                .width = static_cast<float>(m_presenter->get_width()),
                .height = static_cast<float>(m_presenter->get_height()),
                .min_depth = 0.0f,
                .max_depth = 1.0f
            });
            cmd.set_scissor(rhi::Rect2D{
                .offset_x = 0,
                .offset_y = 0,
                .width = m_presenter->get_width(),
                .height = m_presenter->get_height()
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

    m_kernel->tick(core::ExecutionPhase::Render, dt);

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

    // 6. Present Frame
    m_presenter->present(
        rhi::RhiContext::instance().get_graphics_queue(),
        m_render_finished_semaphores[image_index].get_handle(),
        image_index
    );

    m_kernel->tick(core::ExecutionPhase::Present, dt);

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    m_rendered_frames++;
}

void Engine::run() {
    while (is_running()) {
        step();
    }
}

void Engine::shutdown() {
    if (!m_initialized) return;

    if (!m_desc.headless) {
        rhi::RhiContext::instance().wait_idle();
        m_render_graph.destroy();

        m_scene_pipeline.destroy();
        m_vert_shader.destroy();
        m_frag_shader.destroy();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            m_cmd_buffers[i].destroy(m_cmd_pool.get_handle());
            m_in_flight_fences[i].destroy();
            m_image_available_semaphores[i].destroy();
        }

        for (auto& sem : m_render_finished_semaphores) {
            sem.destroy();
        }
        m_render_finished_semaphores.clear();

        m_cmd_pool.destroy();

        if (m_presenter) {
            m_presenter->shutdown();
            m_presenter.reset();
        }

        rhi::BindlessHeap::instance().shutdown();
        rhi::RhiContext::instance().shutdown();
        m_window.destroy();
    } else {
        if (m_presenter) {
            m_presenter->shutdown();
            m_presenter.reset();
        }
    }

    if (m_kernel) {
        m_kernel->shutdown();
        m_kernel.reset();
    }

    input::InputManager::instance().shutdown();
    assets::AssetManager::instance().shutdown();
    jobs::JobSystem::instance().shutdown();
    core::Platform::shutdown();

    m_initialized = false;
    m_running = false;
    LOG_INFO("Engine", "Modern Game Engine Core Stack Shutdown Complete.");
}

} // namespace engine
