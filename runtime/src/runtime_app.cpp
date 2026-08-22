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

        // Open pipeline host: SceneRenderer owns the feature registry, the
        // internal RenderGraph, frame command buffer, fence, and semaphores.
        m_scene_renderer = std::make_unique<engine::renderer::SceneRenderer>(&engine::rhi::RhiContext::instance());
        if (!m_scene_renderer) {
            LOG_FATAL("Runtime", "Failed to construct SceneRenderer!");
            return false;
        }

        // Semaphore slots must cover the swapchain image count: with fewer slots
        // than images, a burst of frames re-signals a render-finished semaphore
        // before the swapchain retires the present waiting on it.
        uint32_t image_count = m_presenter->get_image_count();
        if (image_count == 0) image_count = MAX_FRAMES_IN_FLIGHT_FALLBACK;
        m_image_available_semaphores.resize(image_count);
        for (auto& sem : m_image_available_semaphores) {
            sem.init(false);
        }
        m_scene_renderer->set_frames_in_flight(image_count);
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

    // Reset frame timer after heavy initialization (pipeline compile, Vulkan init, map load)
    // so frame 0 delta_time starts clean at ~0 instead of inheriting startup latency.
    m_timer.reset();

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

    // 3. Vulkan Swapchain Acquisition
    uint32_t image_index = 0;
    VkResult acquire_res = m_presenter->acquire_next_image(
        m_image_available_semaphores[m_current_frame % m_image_available_semaphores.size()].get_handle(),
        VK_NULL_HANDLE,
        image_index
    );

    if (acquire_res == VK_ERROR_OUT_OF_DATE_KHR || acquire_res == VK_SUBOPTIMAL_KHR) {
        if (m_window.get_width() > 0 && m_window.get_height() > 0) {
            m_presenter->resize(m_window.get_width(), m_window.get_height());

            // The aborted acquire may have left this slot's semaphore signaled
            // (SUBOPTIMAL still signals), and the new swapchain may report a
            // different image count. Recreate acquire semaphores and re-sync
            // the renderer's slot count (resize() drains the queue first).
            uint32_t image_count = m_presenter->get_image_count();
            if (image_count == 0) image_count = MAX_FRAMES_IN_FLIGHT_FALLBACK;
            for (auto& sem : m_image_available_semaphores) sem.destroy();
            m_image_available_semaphores.resize(image_count);
            for (auto& sem : m_image_available_semaphores) sem.init(false);
            m_scene_renderer->set_frames_in_flight(image_count);
        }
        return;
    }

    // 4. Build the frame camera from the primary scene camera
    auto& scene = get_scene();
    const float aspect = (m_presenter->get_width() > 0 && m_presenter->get_height() > 0)
        ? static_cast<float>(m_presenter->get_width()) / static_cast<float>(m_presenter->get_height())
        : 16.0f / 9.0f;

    engine::renderer::Camera camera{};
    camera.view_proj = engine::core::Mat4::identity();

    scene.get_world().query_builder<const engine::scene::CameraComponent, const engine::scene::WorldTransformComponent>()
        .build()
        .each([&](flecs::entity, const engine::scene::CameraComponent& cam, const engine::scene::WorldTransformComponent& wt) {
            if (cam.is_primary) {
                camera = engine::renderer::Camera::from_components(cam, wt, aspect);
            }
        });

    // 5. Hand the acquired swapchain image to the open pipeline host
    engine::rhi::RHIImageHandle target{};
    target.image = m_presenter->get_image(image_index);
    target.image_view = m_presenter->get_image_view(image_index);
    target.format = static_cast<engine::rhi::Format>(m_presenter->get_format());
    target.width = m_presenter->get_width();
    target.height = m_presenter->get_height();
    target.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    target.acquire_semaphore = m_image_available_semaphores[m_current_frame % m_image_available_semaphores.size()].get_handle();

    m_kernel->tick(engine::core::ExecutionPhase::Render, dt);

    m_scene_renderer->set_delta_time(dt);
    m_scene_renderer->render(scene, camera, target);

    // 6. Present Frame (waits on the renderer's frame-complete semaphore)
    m_presenter->present(
        engine::rhi::RhiContext::instance().get_graphics_queue(),
        m_scene_renderer->get_render_finished_semaphore(),
        image_index
    );

    // Drain the queue through the present: a present's wait semaphore is not
    // guaranteed released (and the slot safe to re-signal) until the present
    // queue operation completes. Without this, a fast CPU loop re-signals a
    // slot before the swapchain retires the present waiting on it.
    vkQueueWaitIdle(engine::rhi::RhiContext::instance().get_graphics_queue());

    m_kernel->tick(engine::core::ExecutionPhase::Present, dt);

    m_current_frame = (m_current_frame + 1) % m_image_available_semaphores.size();
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

        // Destroy the pipeline host BEFORE the RHI shuts down.
        m_scene_renderer.reset();

        for (auto& sem : m_image_available_semaphores) {
            sem.destroy();
        }

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
