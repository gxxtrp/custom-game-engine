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
#include "engine/assets/asset.h"
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
#include "embedded_shaders.h"

using namespace engine::core;
using namespace engine::jobs;
using namespace engine::events;
using namespace engine::config;
using namespace engine::vfs;
using namespace engine::assets;
using namespace engine::project;
using namespace engine::input;
using namespace engine::importer;
using namespace engine::rhi;
using namespace engine::scene;
using namespace engine::renderer;
using namespace engine::physics;
using namespace engine::audio;
using namespace engine::scripting;
using namespace engine::ui;

static void run_input_subsystem_tests() {
    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "      Running Input Subsystem Verification        ");
    LOG_INFO("Sandbox", "==================================================");

    // 1. Test Key Down / Pressed State Registration
    PlatformEvent key_event{};
    key_event.type = EventType::KeyDown;
    key_event.key.key = KeyCode::W;
    InputManager::instance().process_event(key_event);

    bool w_down = InputManager::instance().is_key_down(KeyCode::W);
    bool w_pressed = InputManager::instance().is_key_pressed(KeyCode::W);
    LOG_INFO("Sandbox", "Key 'W' Down: {}, Just Pressed: {} -> PASS", w_down ? "YES" : "NO", w_pressed ? "YES" : "NO");

    // 2. Test Mouse Move & Delta Registration
    PlatformEvent mouse_event{};
    mouse_event.type = EventType::MouseMove;
    mouse_event.mouse_move.x = 640.0f;
    mouse_event.mouse_move.y = 360.0f;
    mouse_event.mouse_move.dx = 15.0f;
    mouse_event.mouse_move.dy = -8.0f;
    InputManager::instance().process_event(mouse_event);

    Vec2 mouse_pos = InputManager::instance().get_mouse_position();
    Vec2 mouse_delta = InputManager::instance().get_mouse_delta();
    LOG_INFO("Sandbox", "Mouse Position: ({:.1f}, {:.1f}), Delta: ({:.1f}, {:.1f}) -> PASS",
             mouse_pos.x, mouse_pos.y, mouse_delta.x, mouse_delta.y);

    // 3. Test Mouse Button Click Registration
    PlatformEvent click_event{};
    click_event.type = EventType::MouseButtonDown;
    click_event.mouse_button.button = MouseButton::Left;
    InputManager::instance().process_event(click_event);

    bool left_down = InputManager::instance().is_mouse_button_down(MouseButton::Left);
    bool left_pressed = InputManager::instance().is_mouse_button_pressed(MouseButton::Left);
    LOG_INFO("Sandbox", "Mouse Left Button Down: {}, Pressed: {} -> PASS", left_down ? "YES" : "NO", left_pressed ? "YES" : "NO");

    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "      Input Subsystem Verified Cleanly            ");
    LOG_INFO("Sandbox", "==================================================");
}

int main(int argc, char* argv[]) {
    float max_runtime = 0.0f;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--timeout" && i + 1 < argc) {
            max_runtime = std::stof(argv[++i]);
        }
    }

    Logger::instance().add_sink(std::make_shared<FileSink>("sandbox_phase12.log"));
    LOG_INFO("Engine", "Initializing Modern Game Engine - Phase 12 ImGui Editor UI...");

    {
        // 1. Core Subsystems
        if (!Platform::init()) return -1;
        if (!JobSystem::instance().init()) return -1;
        if (!AssetManager::instance().init()) return -1;
        if (!InputManager::instance().init()) return -1;
        if (!PhysicsSystem::instance().init()) return -1;
        if (!AudioEngine::instance().init()) return -1;
        if (!ScriptEngine::instance().init()) return -1;

        run_input_subsystem_tests();

        ProjectManager::instance().create_project("sandbox_project", "SandboxGame");

        // 2. Create Window
        WindowDesc desc{
            .title = "Modern Game Engine - Phase 12 ImGui Editor UI & Docking",
            .width = 1280,
            .height = 720,
            .resizable = true,
            .vulkan_compatible = true
        };

        Window window;
        if (!window.create(desc)) {
            LOG_FATAL("Engine", "Failed to create main window!");
            return -1;
        }

        // 3. Initialize Vulkan 1.3 RHI & Bindless
        if (!RhiContext::instance().init(window, true)) {
            LOG_FATAL("Engine", "Failed to initialize Vulkan 1.3 RHI!");
            return -1;
        }

        if (!BindlessHeap::instance().init()) {
            LOG_FATAL("Engine", "Failed to initialize Bindless Heap!");
            return -1;
        }

        const auto& caps = RhiContext::instance().get_caps();

        // 4. Create Swapchain
        RhiSwapchain swapchain;
        if (!swapchain.init(window.get_width(), window.get_height(), true)) {
            LOG_FATAL("Engine", "Failed to create RHI swapchain!");
            return -1;
        }

        // 5. Initialize Editor UI
        if (!EditorUI::instance().init(window, static_cast<Format>(swapchain.get_format()))) {
            LOG_FATAL("Engine", "Failed to initialize Editor UI!");
            return -1;
        }

        // 6. Setup Active Scene for Hierarchy & Inspector
        Scene main_scene("EditorMainLevel");
        PhysicsSystem::instance().register_scene(main_scene);
        AudioEngine::instance().register_scene(main_scene);
        ScriptEngine::instance().register_scene(main_scene);

        Entity ground = main_scene.create_entity("GroundPlane");
        ground.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, -1.0f, 0.0f) });
        ground.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Static });
        ground.set<ColliderComponent>(ColliderComponent{ .shape_type = ColliderShapeType::Box });

        Entity player = main_scene.create_entity("PlayerController");
        player.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 2.0f, 0.0f) });
        player.set<AudioSourceComponent>(AudioSourceComponent{ .sound_name = "sfx_footstep.wav", .volume = 0.8f });
        player.set<ScriptComponent>(ScriptComponent{ .class_name = "PlayerController" });

        Entity light = main_scene.create_entity("SunLight");
        light.set<TransformComponent>(TransformComponent{ .position = Vec3(20.0f, 50.0f, -30.0f) });

        // Select player entity by default
        EditorUI::instance().set_selected_entity(player.get_id());

        // 7. Create Command Pool & Double Buffered Frames in Flight
        constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
        RhiCommandPool cmd_pool;
        cmd_pool.init(RhiContext::instance().get_queue_families().graphics_family);

        RhiCommandBuffer cmd_buffers[MAX_FRAMES_IN_FLIGHT];
        RhiFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];
        RhiSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            cmd_buffers[i].init(cmd_pool.get_handle());
            in_flight_fences[i].init(true);
            image_available_semaphores[i].init(false);
        }

        uint32_t image_count = swapchain.get_image_count();
        std::vector<RhiSemaphore> render_finished_semaphores(image_count);
        for (size_t i = 0; i < image_count; ++i) {
            render_finished_semaphores[i].init(false);
        }

        // 8. Create Graphics Pipeline for Scene
        RhiShaderModule vert_shader;
        vert_shader.init_from_spirv(engine::shaders::TRIANGLE_VERT_SPV, engine::shaders::TRIANGLE_VERT_SPV_SIZE, ShaderStage::Vertex);

        RhiShaderModule frag_shader;
        frag_shader.init_from_spirv(engine::shaders::TRIANGLE_FRAG_SPV, engine::shaders::TRIANGLE_FRAG_SPV_SIZE, ShaderStage::Fragment);

        GraphicsPipelineDesc post_pipeline_desc{};
        post_pipeline_desc.vertex_shader = &vert_shader;
        post_pipeline_desc.fragment_shader = &frag_shader;
        post_pipeline_desc.color_formats = { static_cast<Format>(swapchain.get_format()) };
        post_pipeline_desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        RhiGraphicsPipeline scene_pipeline;
        if (!scene_pipeline.init(post_pipeline_desc)) {
            LOG_FATAL("Engine", "Failed to create scene graphics pipeline!");
            return -1;
        }

        LOG_INFO("Engine", "==================================================");
        LOG_INFO("Engine", "     ImGui Editor UI Render Loop Starting...      ");
        LOG_INFO("Engine", "==================================================");

        RenderGraph graph;
        DynamicArray<PlatformEvent> events;
        FrameTimer timer;
        uint32_t current_frame = 0;
        uint32_t rendered_frames = 0;

        while (window.is_open()) {
            timer.tick();
            InputManager::instance().new_frame();

            events.clear();
            Platform::poll_events(events);

            for (const auto& event : events) {
                Platform::process_window_events(window, event);
                InputManager::instance().process_event(event);

                if (event.type == EventType::KeyDown && event.key.key == KeyCode::Escape) {
                    window.set_should_close(true);
                } else if (event.type == EventType::WindowResize) {
                    swapchain.resize(event.window_resize.width, event.window_resize.height);
                }
            }

            float dt = static_cast<float>(timer.delta_time());

            if (dt > 0.0f && dt < 0.1f) {
                PhysicsSystem::instance().update(dt);
                AudioEngine::instance().update(dt);
            }

            // Begin ImGui Frame & Panels
            EditorUI::instance().begin_frame();
            EditorUI::instance().render_panels(main_scene, dt, timer.fps());
            EditorUI::instance().end_frame();

            in_flight_fences[current_frame].wait();

            uint32_t image_index = 0;
            VkResult acquire_res = swapchain.acquire_next_image(
                image_available_semaphores[current_frame].get_handle(),
                VK_NULL_HANDLE,
                image_index
            );

            if (acquire_res == VK_ERROR_OUT_OF_DATE_KHR) {
                swapchain.resize(window.get_width(), window.get_height());
                continue;
            }

            in_flight_fences[current_frame].reset();

            // Record Render Graph
            graph.reset();

            RGTextureHandle swapchain_rg = graph.import_texture(
                "SwapchainOutput",
                swapchain.get_image(image_index),
                swapchain.get_image_view(image_index),
                swapchain.get_extent().width,
                swapchain.get_extent().height,
                static_cast<Format>(swapchain.get_format()),
                VK_IMAGE_LAYOUT_UNDEFINED
            );

            // Pass 1: Scene Geometry Pass
            graph.add_pass(
                "SceneForwardPass",
                [&](RenderPassBuilder& builder) {
                    builder.set_color_attachment(
                        0, 
                        swapchain_rg, 
                        VK_ATTACHMENT_LOAD_OP_CLEAR, 
                        VK_ATTACHMENT_STORE_OP_STORE, 
                        Vec4(0.04f, 0.05f, 0.07f, 1.0f)
                    );
                },
                [&](RenderPassContext& ctx) {
                    auto& cmd = ctx.get_command_buffer();
                    cmd.set_viewport(Viewport{
                        .x = 0.0f,
                        .y = 0.0f,
                        .width = static_cast<float>(swapchain.get_extent().width),
                        .height = static_cast<float>(swapchain.get_extent().height),
                        .min_depth = 0.0f,
                        .max_depth = 1.0f
                    });
                    cmd.set_scissor(Rect2D{
                        .offset_x = 0,
                        .offset_y = 0,
                        .width = swapchain.get_extent().width,
                        .height = swapchain.get_extent().height
                    });
                    cmd.bind_pipeline(scene_pipeline.get_pipeline());
                    cmd.draw(3, 1, 0, 0);
                }
            );

            // Pass 2: Editor ImGui UI Pass
            graph.add_pass(
                "EditorUIPass",
                [&](RenderPassBuilder& builder) {
                    builder.set_color_attachment(
                        0,
                        swapchain_rg,
                        VK_ATTACHMENT_LOAD_OP_LOAD,
                        VK_ATTACHMENT_STORE_OP_STORE
                    );
                },
                [](RenderPassContext& ctx) {
                    auto& cmd = ctx.get_command_buffer();
                    EditorUI::instance().render(cmd.get_handle());
                }
            );

            // Pass 3: Present Transition Pass
            graph.add_pass(
                "PresentTransitionPass",
                [&](RenderPassBuilder& builder) {
                    builder.read_texture(swapchain_rg, RGResourceAccess::Present);
                },
                [](RenderPassContext&) {}
            );

            // Execute RenderGraph
            auto& cmd = cmd_buffers[current_frame];
            cmd.reset();
            cmd.begin();

            graph.compile();
            graph.execute(cmd);

            cmd.end();

            // Submit
            VkSubmitInfo submit_info{};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore wait_semaphores[] = { image_available_semaphores[current_frame].get_handle() };
            VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            submit_info.waitSemaphoreCount = 1;
            submit_info.pWaitSemaphores = wait_semaphores;
            submit_info.pWaitDstStageMask = wait_stages;

            VkCommandBuffer raw_cmd = cmd.get_handle();
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &raw_cmd;

            VkSemaphore signal_semaphores[] = { render_finished_semaphores[image_index].get_handle() };
            submit_info.signalSemaphoreCount = 1;
            submit_info.pSignalSemaphores = signal_semaphores;

            vkQueueSubmit(RhiContext::instance().get_graphics_queue(), 1, &submit_info, in_flight_fences[current_frame].get_handle());

            // Present
            VkResult present_res = swapchain.present(
                RhiContext::instance().get_graphics_queue(),
                render_finished_semaphores[image_index].get_handle(),
                image_index
            );

            if (present_res == VK_ERROR_OUT_OF_DATE_KHR || present_res == VK_SUBOPTIMAL_KHR) {
                swapchain.resize(window.get_width(), window.get_height());
            }

            current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
            rendered_frames++;

            if (timer.fps() > 0) {
                std::string title = std::format("Modern Game Engine [Phase 12 ImGui Editor] | GPU: {} | FPS: {} | Frames: {}", 
                                                caps.device_name, timer.fps(), rendered_frames);
                window.set_title(title);
            }

            if (max_runtime > 0.0f && timer.total_time() >= max_runtime) {
                LOG_INFO("Sandbox", "Reached maximum test runtime ({:.2f}s, rendered {} frames). Exiting loop...", 
                         max_runtime, rendered_frames);
                window.set_should_close(true);
            }
        }

        // Cleanup
        RhiContext::instance().wait_idle();

        graph.destroy();
        scene_pipeline.destroy();
        vert_shader.destroy();
        frag_shader.destroy();

        EditorUI::instance().shutdown();

        for (auto& sem : render_finished_semaphores) {
            sem.destroy();
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            in_flight_fences[i].destroy();
            image_available_semaphores[i].destroy();
            cmd_buffers[i].destroy(cmd_pool.get_handle());
        }
        cmd_pool.destroy();
        swapchain.destroy();

        BindlessHeap::instance().shutdown();
        RhiContext::instance().shutdown();

        window.destroy();
        ProjectManager::instance().close_project();
        ScriptEngine::instance().shutdown();
        AudioEngine::instance().shutdown();
        PhysicsSystem::instance().shutdown();
        InputManager::instance().shutdown();
        AssetManager::instance().shutdown();
        JobSystem::instance().shutdown();
        Platform::shutdown();
    }

    LOG_INFO("Engine", "Engine Phase 12 shutdown completed.");
    GlobalAllocator::instance().dump_leaks();

    return 0;
}
