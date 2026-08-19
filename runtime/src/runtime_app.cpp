#include "runtime/runtime_app.h"
#include "engine/core/log.h"
#include "engine/core/platform.h"
#include "engine/jobs/job_system.h"
#include "engine/assets/asset_manager.h"
#include "engine/input/input_manager.h"
#include "engine/physics/physics_system.h"
#include "engine/physics/physics_components.h"
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

RuntimeApp::RuntimeApp() : m_scene("RuntimeScene") {
}

RuntimeApp::~RuntimeApp() {
    shutdown();
}

RuntimeApp& RuntimeApp::instance() {
    static RuntimeApp s_instance;
    return s_instance;
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

    // 2. Initialize Engine Master Subsystems
    if (!engine::jobs::JobSystem::instance().init()) return false;
    if (!engine::assets::AssetManager::instance().init()) return false;
    if (!engine::input::InputManager::instance().init()) return false;
    if (!engine::physics::PhysicsSystem::instance().init()) return false;
    if (!engine::audio::AudioEngine::instance().init()) return false;
    if (!engine::scripting::ScriptEngine::instance().init()) return false;

    // 3. Resolve and Load Project
    std::string project_dir = m_desc.project_path;
    if (project_dir.empty()) {
        if (std::filesystem::exists("project.toml")) {
            project_dir = ".";
        } else if (std::filesystem::exists("sandbox_project/project.toml")) {
            project_dir = "sandbox_project";
        }
    }

    if (!project_dir.empty()) {
        std::filesystem::path p(project_dir);
        if (std::filesystem::exists(p / "project.toml")) {
            engine::project::ProjectManager::instance().load_project((p / "project.toml").string());
        } else if (std::filesystem::exists(p) && p.extension() == ".toml") {
            engine::project::ProjectManager::instance().load_project(p.string());
        }
    }

    const auto& proj = engine::project::ProjectManager::instance().get_active_project();

    // 4. Create Window from Game Project Config
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

    // 5. Initialize Vulkan 1.3 RHI
    if (!engine::rhi::RhiContext::instance().init(m_window, m_desc.enable_validation)) {
        LOG_FATAL("Runtime", "Failed to initialize Vulkan 1.3 RHI!");
        return false;
    }

    // 6. Create Swapchain
    if (!m_swapchain.init(m_window.get_width(), m_window.get_height(), proj.vsync)) {
        LOG_FATAL("Runtime", "Failed to create swapchain!");
        return false;
    }

    // 7. Initialize Sync Primitives & Command Pools
    m_cmd_pool.init(engine::rhi::RhiContext::instance().get_queue_families().graphics_family);

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

    // 8. Initialize Scene Renderer
    engine::rhi::Format sc_format = static_cast<engine::rhi::Format>(m_swapchain.get_format());
    if (!engine::renderer::SceneRenderer::instance().init(sc_format, engine::rhi::Format::D32_SFLOAT)) {
        LOG_FATAL("Runtime", "Failed to initialize SceneRenderer!");
        return false;
    }

    // 9. Register Scene with Subsystems
    engine::physics::PhysicsSystem::instance().register_scene(m_scene);
    engine::audio::AudioEngine::instance().register_scene(m_scene);
    engine::scripting::ScriptEngine::instance().register_scene(m_scene);

    // 10. Load Startup Map
    if (!proj.default_map.empty() && (engine::vfs::VFS::instance().file_exists(proj.default_map) || std::filesystem::exists(proj.default_map))) {
        engine::scene::MapSerializer::load_map(proj.default_map, m_scene);
        LOG_INFO("Runtime", "Loaded startup map: {}", proj.default_map);
    } else {
        LOG_WARN("Runtime", "No default map found, initializing minimal scene");
        auto ground = m_scene.create_entity("GroundPlane");
        ground.set<engine::scene::TransformComponent>(engine::scene::TransformComponent{ 
            .position = engine::core::Vec3(0.0f, -0.05f, 0.0f),
            .scale = engine::core::Vec3(30.0f, 0.1f, 30.0f) 
        });
        ground.set<engine::scene::MeshRendererComponent>(engine::scene::MeshRendererComponent{ .is_visible = true });

        auto sun = m_scene.create_entity("SunLight");
        sun.set<engine::scene::TransformComponent>(engine::scene::TransformComponent{ .position = engine::core::Vec3(15.0f, 30.0f, -15.0f) });
        sun.set<engine::scene::DirectionalLightComponent>(engine::scene::DirectionalLightComponent{ .color = engine::core::Vec3(1.0f, 0.98f, 0.92f), .intensity = 1.5f });

        auto cam = m_scene.create_entity("MainCamera");
        cam.set<engine::scene::TransformComponent>(engine::scene::TransformComponent{ .position = engine::core::Vec3(0.0f, 2.0f, -6.0f) });
        cam.set<engine::scene::CameraComponent>(engine::scene::CameraComponent{ .fov_deg = 60.0f, .is_primary = true });
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
        engine::core::Platform::process_window_events(m_window, ev);
        engine::input::InputManager::instance().process_event(ev);

        if (ev.type == engine::core::EventType::Quit || ev.type == engine::core::EventType::WindowClose) {
            m_running = false;
            m_window.set_should_close(true);
            return;
        }

        if (ev.type == engine::core::EventType::WindowResize) {
            if (ev.window_resize.width > 0 && ev.window_resize.height > 0) {
                m_swapchain.resize(ev.window_resize.width, ev.window_resize.height);
            }
        }
    }

    if (m_window.should_close() || !m_running || engine::input::InputManager::instance().is_key_pressed(engine::core::KeyCode::Escape)) {
        m_running = false;
        m_window.set_should_close(true);
        return;
    }

    // Skip rendering if window is minimized or has 0 size
    if (m_window.get_width() == 0 || m_window.get_height() == 0 ||
        m_swapchain.get_extent().width == 0 || m_swapchain.get_extent().height == 0) {
        return;
    }

    // Handle automated testing timeout
    if (m_desc.timeout > 0.0f && m_timer.total_time() >= m_desc.timeout) {
        LOG_INFO("Runtime", "Reached automated test timeout ({:.2f}s, rendered {} frames). Requesting exit...",
                 m_desc.timeout, m_rendered_frames);
        m_running = false;
        return;
    }

    // 2. Physics step
    constexpr float FIXED_DT = 1.0f / 60.0f;
    m_physics_accumulator += dt;
    while (m_physics_accumulator >= FIXED_DT) {
        engine::physics::PhysicsSystem::instance().update(FIXED_DT);
        m_physics_accumulator -= FIXED_DT;
    }

    // Script & Audio updates
    engine::scripting::ScriptEngine::instance().sync_ecs_scripts(m_scene, dt);
    engine::audio::AudioEngine::instance().update(dt);
    m_scene.update(dt);

    // 3. Vulkan Swapchain Acquisition & Frame Rendering
    m_in_flight_fences[m_current_frame].wait();

    uint32_t image_index = 0;
    VkResult acquire_res = m_swapchain.acquire_next_image(
        m_image_available_semaphores[m_current_frame].get_handle(),
        VK_NULL_HANDLE,
        image_index
    );

    if (acquire_res == VK_ERROR_OUT_OF_DATE_KHR || acquire_res == VK_SUBOPTIMAL_KHR) {
        if (m_window.get_width() > 0 && m_window.get_height() > 0) {
            m_swapchain.resize(m_window.get_width(), m_window.get_height());
        }
        return;
    }

    m_in_flight_fences[m_current_frame].reset();

    // 4. Render Graph Execution
    m_render_graph.reset();

    engine::renderer::RGTextureHandle swapchain_rg = m_render_graph.import_texture(
        "SwapchainOutput",
        m_swapchain.get_image(image_index),
        m_swapchain.get_image_view(image_index),
        m_swapchain.get_extent().width,
        m_swapchain.get_extent().height,
        static_cast<engine::rhi::Format>(m_swapchain.get_format()),
        VK_IMAGE_LAYOUT_UNDEFINED
    );

    // Camera query
    engine::core::Mat4 view_proj = engine::core::Mat4::identity();
    engine::core::Vec3 camera_pos(0.0f, 2.0f, -6.0f);
    float aspect = static_cast<float>(m_swapchain.get_extent().width) / static_cast<float>(std::max(m_swapchain.get_extent().height, 1u));

    bool found_camera = false;
    m_scene.get_world().query_builder<const engine::scene::CameraComponent, const engine::scene::TransformComponent>()
        .build()
        .each([&](flecs::entity e, const engine::scene::CameraComponent& cam, const engine::scene::TransformComponent& trans) {
            if (cam.is_primary && !found_camera) {
                found_camera = true;
                camera_pos = trans.position;
                engine::core::Mat4 rot_mat = trans.rotation.to_mat4();
                engine::core::Vec3 forward(rot_mat.cols[2].x, rot_mat.cols[2].y, rot_mat.cols[2].z);
                engine::core::Mat4 view = engine::core::Mat4::look_at(
                    trans.position,
                    trans.position + forward,
                    engine::core::Vec3(0.0f, 1.0f, 0.0f)
                );
                engine::core::Mat4 proj = engine::core::Mat4::perspective_vk(
                    engine::core::math::deg_to_rad(cam.fov_deg),
                    aspect,
                    cam.near_z,
                    cam.far_z
                );
                view_proj = proj * view;
            }
        });

    if (!found_camera) {
        engine::core::Mat4 view = engine::core::Mat4::look_at(camera_pos, engine::core::Vec3(0.0f, 0.0f, 0.0f), engine::core::Vec3(0.0f, 1.0f, 0.0f));
        engine::core::Mat4 proj = engine::core::Mat4::perspective_vk(engine::core::math::deg_to_rad(60.0f), aspect, 0.1f, 1000.0f);
        view_proj = proj * view;
    }

    uint32_t width = m_swapchain.get_extent().width;
    uint32_t height = m_swapchain.get_extent().height;

    // Add Game Forward Pass directly into swapchain
    m_render_graph.add_pass(
        "GameForwardPass",
        [&](engine::renderer::RenderPassBuilder& builder) {
            builder.set_color_attachment(
                0,
                swapchain_rg,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                engine::core::Vec4(0.08f, 0.09f, 0.11f, 1.0f)
            );
            
            engine::renderer::RGTextureHandle depth_rg = builder.create_texture(engine::renderer::RGTextureDesc{
                .width = width,
                .height = height,
                .format = engine::rhi::Format::D32_SFLOAT,
                .usage = engine::rhi::TextureUsage::DepthAttachment,
                .debug_name = "SceneDepth"
            });
            builder.set_depth_attachment(depth_rg, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, 1.0f);
        },
        [this, view_proj, camera_pos, width, height](engine::renderer::RenderPassContext& ctx) {
            auto& cmd = ctx.get_command_buffer();

            engine::renderer::SceneRenderer::instance().render_scene(
                cmd,
                m_scene,
                view_proj,
                camera_pos,
                engine::rhi::Viewport{
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = static_cast<float>(width),
                    .height = static_cast<float>(height),
                    .min_depth = 0.0f,
                    .max_depth = 1.0f
                },
                engine::rhi::Rect2D{
                    .offset_x = 0,
                    .offset_y = 0,
                    .width = width,
                    .height = height
                }
            );
        }
    );

    // Present Pass
    m_render_graph.add_pass(
        "PresentPass",
        [&](engine::renderer::RenderPassBuilder& builder) {
            builder.read_texture(swapchain_rg, engine::renderer::RGResourceAccess::Present);
        },
        [](engine::renderer::RenderPassContext&) {}
    );

    auto& cmd = m_cmd_buffers[m_current_frame];
    cmd.reset();
    cmd.begin();

    // 5. Compile and Execute
    m_render_graph.compile();
    m_render_graph.execute(cmd);

    cmd.end();

    // 6. Submit to Graphics Queue
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

    // 7. Present
    VkResult present_res = m_swapchain.present(
        engine::rhi::RhiContext::instance().get_graphics_queue(),
        m_render_finished_semaphores[image_index].get_handle(),
        image_index
    );

    if (present_res == VK_ERROR_OUT_OF_DATE_KHR || present_res == VK_SUBOPTIMAL_KHR) {
        if (m_window.get_width() > 0 && m_window.get_height() > 0) {
            m_swapchain.resize(m_window.get_width(), m_window.get_height());
        }
    }

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    m_rendered_frames++;
}

void RuntimeApp::shutdown() {
    if (!m_initialized) return;

    LOG_INFO("Runtime", "Shutting down Modern Game Engine Runtime...");

    engine::rhi::RhiContext::instance().wait_idle();

    // 1. Destroy Render Graph (frees transient GPU textures/buffers)
    m_render_graph.destroy();

    // 2. Destroy Sync & Command Buffers
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_in_flight_fences[i].destroy();
        m_image_available_semaphores[i].destroy();
        m_cmd_buffers[i].destroy(m_cmd_pool.get_handle());
    }

    for (auto& sem : m_render_finished_semaphores) {
        sem.destroy();
    }
    m_render_finished_semaphores.clear();

    m_cmd_pool.destroy();

    // 3. Shutdown Scene Renderer, Swapchain & Vulkan Context
    engine::renderer::SceneRenderer::instance().shutdown();
    m_swapchain.destroy();
    engine::rhi::RhiContext::instance().shutdown();
    m_window.destroy();

    // 3. Shutdown Engine Master Subsystems
    engine::project::ProjectManager::instance().close_project();
    engine::scripting::ScriptEngine::instance().shutdown();
    engine::audio::AudioEngine::instance().shutdown();
    engine::physics::PhysicsSystem::instance().shutdown();
    engine::input::InputManager::instance().shutdown();
    engine::assets::AssetManager::instance().shutdown();
    engine::jobs::JobSystem::instance().shutdown();
    engine::core::Platform::shutdown();

    m_initialized = false;
    m_running = false;
    LOG_INFO("Runtime", "Runtime shutdown complete.");
}

bool RuntimeApp::is_running() const {
    return m_running && m_window.is_open() && !m_window.should_close();
}

void RuntimeApp::request_exit() {
    m_running = false;
    m_window.set_should_close(true);
}

} // namespace runtime
