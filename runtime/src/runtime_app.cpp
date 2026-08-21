#include "runtime/runtime_app.h"
#include "engine/core/log.h"
#include "engine/core/platform.h"
#include "engine/jobs/job_system.h"
#include "engine/assets/asset_manager.h"
#include "engine/input/input_manager.h"
#include "engine/physics/physics_system.h"
#include "engine/audio/audio_engine.h"
#include "engine/scripting/script_engine.h"
#include "engine/project/project.h"
#include "engine/scene/map_serializer.h"
#include "engine/scene/components.h"
#include "engine/vfs/vfs.h"
#include <filesystem>
#include <format>
#include <algorithm>

namespace runtime {

RuntimeApp::RuntimeApp() = default;

RuntimeApp::~RuntimeApp() {
    shutdown();
}

RuntimeApp& RuntimeApp::instance() {
    static RuntimeApp s_instance;
    return s_instance;
}

engine::scene::Scene& RuntimeApp::get_scene() {
    return m_kernel->get_context().get<engine::scene::SceneSubsystem>().get_active_scene();
}

bool RuntimeApp::init(const RuntimeAppDesc& desc) {
    m_desc = desc;

    LOG_INFO("Runtime", "==================================================");
    LOG_INFO("Runtime", "   Initializing Modern Game Engine Game Runtime   ");
    LOG_INFO("Runtime", "==================================================");

    // 1. Initialize Platform Subsystem
    if (!engine::core::Platform::init()) {
        LOG_FATAL("Runtime", "Failed to initialize platform subsystem!");
        return false;
    }

    // 2. Initialize Engine Foundation Services
    if (!engine::jobs::JobSystem::instance().init()) return false;
    if (!engine::assets::AssetManager::instance().init()) return false;
    if (!engine::input::InputManager::instance().init()) return false;

    // 3. Resolve and Load Project
    std::string project_dir = m_desc.project_path;
    if (project_dir.empty()) {
        if (std::filesystem::exists("project.toml")) {
            project_dir = ".";
        }
    }

    if (project_dir.empty() || (!std::filesystem::exists(project_dir) && !std::filesystem::exists(std::filesystem::path(project_dir) / "project.toml"))) {
        LOG_ERROR("Runtime", "===============================================================================");
        LOG_ERROR("Runtime", "No game project found to run!");
        LOG_ERROR("Runtime", "Usage: runtime.exe --project <project_directory>");
        LOG_ERROR("Runtime", "Or execute runtime.exe from within a packaged game folder containing project.toml.");
        LOG_ERROR("Runtime", "===============================================================================");
        return false;
    }

    std::filesystem::path p(project_dir);
    if (std::filesystem::exists(p / "project.toml")) {
        if (!engine::project::ProjectManager::instance().load_project((p / "project.toml").string())) {
            LOG_ERROR("Runtime", "Failed to load project manifest: {}/project.toml", project_dir);
            return false;
        }
    } else if (std::filesystem::exists(p) && p.extension() == ".toml") {
        if (!engine::project::ProjectManager::instance().load_project(p.string())) {
            LOG_ERROR("Runtime", "Failed to load project manifest: {}", p.string());
            return false;
        }
    } else {
        LOG_ERROR("Runtime", "Specified path is not a valid project directory or project.toml: {}", project_dir);
        return false;
    }

    const auto& proj = engine::project::ProjectManager::instance().get_active_project();

    // 4. Viewport Presenter Setup
    if (!m_desc.headless) {
        engine::core::WindowDesc win_desc{
            .title = proj.name.empty() ? "Modern Game Engine Runtime" : proj.name,
            .width = proj.window_width > 0 ? proj.window_width : 1280,
            .height = proj.window_height > 0 ? proj.window_height : 720,
            .resizable = true,
            .vulkan_compatible = true
        };

        if (!m_window.create(win_desc)) {
            LOG_FATAL("Runtime", "Failed to create runtime window!");
            return false;
        }

        if (!engine::rhi::RhiContext::instance().init(m_window, m_desc.enable_validation)) {
            LOG_FATAL("Runtime", "Failed to initialize Vulkan 1.3 RHI!");
            return false;
        }

        m_presenter = std::make_unique<engine::rhi::WindowSwapchainPresenter>();
        if (!m_presenter->initialize(m_window.get_width(), m_window.get_height(), proj.vsync)) {
            LOG_FATAL("Runtime", "Failed to initialize WindowSwapchainPresenter!");
            return false;
        }

        m_cmd_pool.init(engine::rhi::RhiContext::instance().get_queue_families().graphics_family);

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

        engine::rhi::Format sc_format = static_cast<engine::rhi::Format>(m_presenter->get_format());
        if (!engine::renderer::SceneRenderer::instance().init(sc_format, engine::rhi::Format::R16G16B16A16_SFLOAT, engine::rhi::Format::D32_SFLOAT)) {
            LOG_FATAL("Runtime", "Failed to initialize SceneRenderer!");
            return false;
        }
    } else {
        LOG_INFO("Runtime", "Running in Headless Dedicated Simulation / Server mode (No Window / No GPU)");
        m_presenter = std::make_unique<engine::rhi::HeadlessPresenter>();
        m_presenter->initialize(1280, 720, false);
    }

    // 5. Construct & Initialize EngineKernel DAG
    engine::core::KernelBuilder builder;
    builder.add_subsystem<engine::scene::SceneSubsystem>("RuntimeScene");
    builder.add_subsystem<engine::physics::PhysicsSystem>();
    builder.add_subsystem<engine::audio::AudioEngine>();
    builder.add_subsystem<engine::scripting::ScriptEngine>();

    m_kernel = builder.build();
    if (!m_kernel->initialize()) {
        LOG_FATAL("Runtime", "Failed to initialize EngineKernel!");
        return false;
    }

    // 6. Load Startup Map into SceneSubsystem
    auto& scene = get_scene();
    if (!proj.default_map.empty() && (engine::vfs::VFS::instance().file_exists(proj.default_map) || std::filesystem::exists(proj.default_map))) {
        engine::scene::MapSerializer::load_map(proj.default_map, scene);
        LOG_INFO("Runtime", "Loaded startup map: {}", proj.default_map);
    } else {
        LOG_WARN("Runtime", "Startup map '{}' was not found in project. Initializing clean empty scene.", proj.default_map);
    }

    m_initialized = true;
    m_running = true;

    LOG_INFO("Runtime", "Modern Game Engine Runtime Initialized Successfully! Running game '{}'", proj.name);
    return true;
}

void RuntimeApp::run() {
    while (is_running()) {
        step();
    }
}

void RuntimeApp::step() {
    m_timer.tick();
    float dt = m_timer.delta_time();

    // 1. Process Platform Events & Input
    engine::input::InputManager::instance().new_frame();

    engine::core::DynamicArray<engine::core::PlatformEvent> events;
    engine::core::Platform::poll_events(events);
    for (const auto& ev : events) {
        if (!m_desc.headless) {
            engine::core::Platform::process_window_events(m_window, ev);
        }
        engine::input::InputManager::instance().process_event(ev);

        if (ev.type == engine::core::EventType::Quit || ev.type == engine::core::EventType::WindowClose) {
            m_running = false;
            if (!m_desc.headless) m_window.set_should_close(true);
            return;
        }

        if (!m_desc.headless && ev.type == engine::core::EventType::WindowResize) {
            if (ev.window_resize.width > 0 && ev.window_resize.height > 0) {
                m_presenter->resize(ev.window_resize.width, ev.window_resize.height);
            }
        }
    }

    if (!m_desc.headless) {
        if (m_window.should_close() || !m_running || engine::input::InputManager::instance().is_key_pressed(engine::core::KeyCode::Escape)) {
            m_running = false;
            m_window.set_should_close(true);
            return;
        }

        // Skip rendering if window is minimized or has 0 size
        if (m_window.get_width() == 0 || m_window.get_height() == 0 ||
            m_presenter->get_width() == 0 || m_presenter->get_height() == 0) {
            return;
        }
    }

    // Handle automated testing timeout
    if (m_desc.timeout > 0.0f && m_timer.total_time() >= m_desc.timeout) {
        LOG_INFO("Runtime", "Reached automated test timeout ({:.2f}s, rendered/simulated {} frames). Requesting exit...",
                 m_desc.timeout, m_rendered_frames);
        m_running = false;
        return;
    }

    // 2. Kernel Frame Step (Executes PreTick, Simulation, PostSimulation)
    m_kernel->tick(engine::core::ExecutionPhase::PreTick, dt);
    m_kernel->tick(engine::core::ExecutionPhase::Simulation, dt);
    m_kernel->tick(engine::core::ExecutionPhase::PostSimulation, dt);

    if (m_desc.headless) {
        m_kernel->tick(engine::core::ExecutionPhase::Render, dt);
        m_kernel->tick(engine::core::ExecutionPhase::Present, dt);
        m_rendered_frames++;
        engine::core::Clock::sleep_ms(1);
        return;
    }

    // 3. Vulkan Swapchain Acquisition & Frame Rendering
    m_in_flight_fences[m_current_frame].wait();

    uint32_t image_index = 0;
    VkResult acquire_res = m_presenter->acquire_next_image(
        m_image_available_semaphores[m_current_frame].get_handle(),
        VK_NULL_HANDLE,
        image_index
    );

    if (acquire_res == VK_ERROR_OUT_OF_DATE_KHR || acquire_res == VK_SUBOPTIMAL_KHR) {
        if (m_window.get_width() > 0 && m_window.get_height() > 0) {
            m_presenter->resize(m_window.get_width(), m_window.get_height());
        }
        return;
    }

    m_in_flight_fences[m_current_frame].reset();

    // 4. Render Scene with RenderGraph
    m_render_graph.reset();

    engine::renderer::RGTextureHandle swapchain_rg = m_render_graph.import_texture(
        "SwapchainOutput",
        m_presenter->get_image(image_index),
        m_presenter->get_image_view(image_index),
        m_presenter->get_width(),
        m_presenter->get_height(),
        static_cast<engine::rhi::Format>(m_presenter->get_format()),
        VK_IMAGE_LAYOUT_UNDEFINED
    );

    auto& scene = get_scene();
    engine::renderer::RenderCamera camera{};
    camera.view_proj = engine::core::Mat4::identity();
    camera.position = engine::core::Vec3(0.0f, 0.0f, 0.0f);

    // Find primary camera in scene
    scene.get_world().query_builder<const engine::scene::CameraComponent, const engine::scene::TransformComponent>()
        .build()
        .each([&](flecs::entity e, const engine::scene::CameraComponent& cam, const engine::scene::TransformComponent& t) {
            if (cam.is_primary) {
                float aspect = m_presenter->get_width() > 0 && m_presenter->get_height() > 0
                    ? static_cast<float>(m_presenter->get_width()) / static_cast<float>(m_presenter->get_height())
                    : 16.0f / 9.0f;
                camera.proj = cam.get_projection_matrix(aspect);
                engine::core::Vec3 fwd = t.rotation.rotate(engine::core::Vec3(0.0f, 0.0f, 1.0f));
                engine::core::Vec3 up = t.rotation.rotate(engine::core::Vec3(0.0f, 1.0f, 0.0f));
                camera.view = engine::core::Mat4::look_at(t.position, t.position + fwd, up);
                camera.view_proj = camera.proj * camera.view;
                camera.position = t.position;
                camera.frustum = engine::core::Frustum::from_view_projection(camera.view_proj);
            }
        });

    engine::renderer::GraphicsSettings settings{};
    engine::renderer::SceneRenderer::instance().setup_render_pipeline(
        m_render_graph,
        scene,
        camera,
        settings,
        swapchain_rg,
        m_presenter->get_width(),
        m_presenter->get_height()
    );

    // Pass 2: Present Transition Pass
    m_render_graph.add_pass(
        "PresentTransitionPass",
        [&](engine::renderer::RenderPassBuilder& builder) {
            builder.read_texture(swapchain_rg, engine::renderer::RGResourceAccess::Present);
        },
        [](engine::renderer::RenderPassContext&) {}
    );

    // Execute RenderGraph
    auto& cmd = m_cmd_buffers[m_current_frame];
    cmd.reset();
    cmd.begin();

    m_kernel->tick(engine::core::ExecutionPhase::Render, dt);

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

    vkQueueSubmit(engine::rhi::RhiContext::instance().get_graphics_queue(), 1, &submit_info, m_in_flight_fences[m_current_frame].get_handle());

    // 6. Present Frame
    m_presenter->present(
        engine::rhi::RhiContext::instance().get_graphics_queue(),
        m_render_finished_semaphores[image_index].get_handle(),
        image_index
    );

    m_kernel->tick(engine::core::ExecutionPhase::Present, dt);

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    m_rendered_frames++;
}

bool RuntimeApp::is_running() const {
    return m_running && (m_desc.headless || m_window.is_open());
}

void RuntimeApp::request_exit() {
    m_running = false;
    if (!m_desc.headless) {
        m_window.set_should_close(true);
    }
}

void RuntimeApp::shutdown() {
    if (!m_initialized) return;

    if (!m_desc.headless) {
        engine::rhi::RhiContext::instance().wait_idle();
        m_render_graph.destroy();
        engine::renderer::SceneRenderer::instance().shutdown();

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

        engine::rhi::RhiContext::instance().shutdown();
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

    engine::input::InputManager::instance().shutdown();
    engine::assets::AssetManager::instance().shutdown();
    engine::jobs::JobSystem::instance().shutdown();
    engine::core::Platform::shutdown();

    m_initialized = false;
    m_running = false;
    LOG_INFO("Runtime", "Modern Game Engine Runtime Shutdown Complete.");
}

} // namespace runtime
