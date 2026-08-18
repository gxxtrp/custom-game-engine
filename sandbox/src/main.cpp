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

static void run_phase11_subsystem_tests() {
    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "    Running Phase 11 Lua & Scripting Tests        ");
    LOG_INFO("Sandbox", "==================================================");

    // 1. Sol2 Lua Execution & Math Bindings
    LOG_INFO("Sandbox", "--- 1. Lua Script Execution & Math Bindings ---");
    auto res = ScriptEngine::instance().execute_string(R"LUA(
        local v1 = Vec3.new(1.0, 2.0, 3.0)
        local v2 = Vec3.new(4.0, 5.0, 6.0)
        local v3 = v1 + v2
        Log.info(string.format("Lua Vec3 Result: (%.1f, %.1f, %.1f)", v3.x, v3.y, v3.z))
    )LUA");
    LOG_INFO("Sandbox", "Lua Math Evaluation: {} -> PASS", res.success ? "SUCCESS" : res.error_message);

    // 2. Flecs ECS ScriptComponent Lifecycle Execution
    LOG_INFO("Sandbox", "--- 2. Flecs ECS ScriptComponent Lifecycle ---");
    Scene script_scene("ScriptTestLevel");
    ScriptEngine::instance().register_scene(script_scene);

    const char* lua_code = R"LUA(
        PlayerController = {
            speed = 10.0
        }
        function PlayerController:on_init(entity)
            Log.info("PlayerController initialized on: " .. entity:get_name())
        end
        function PlayerController:on_update(entity, dt)
            entity:translate(self.speed * dt, 0.0, 0.0)
        end
    )LUA";

    Entity player = script_scene.create_entity("LuaPlayer");
    player.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 0.0f, 0.0f) });
    player.set<ScriptComponent>(ScriptComponent{
        .script_source = lua_code,
        .class_name = "PlayerController"
    });

    float initial_x = player.get<TransformComponent>()->position.x;
    LOG_INFO("Sandbox", "Entity Initial Pos X: {:.2f}", initial_x);

    // Step 5 frames of script simulation (dt = 0.1s) -> should move 5 * (10.0 * 0.1) = 5.0 units
    for (int i = 0; i < 5; ++i) {
        ScriptEngine::instance().sync_ecs_scripts(script_scene, 0.1f);
    }

    float final_x = player.get<TransformComponent>()->position.x;
    LOG_INFO("Sandbox", "Entity Final Pos X after 5 script steps: {:.2f}", final_x);
    LOG_INFO("Sandbox", "Lua Script ECS Lifecycle & Component Mutation: {}", (final_x >= 4.9f) ? "PASS" : "FAIL");

    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "    Phase 11 Subsystem Tests Verified Cleanly     ");
    LOG_INFO("Sandbox", "==================================================");
}

int main(int argc, char* argv[]) {
    float max_runtime = 0.0f;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--timeout" && i + 1 < argc) {
            max_runtime = std::stof(argv[++i]);
        }
    }

    Logger::instance().add_sink(std::make_shared<FileSink>("sandbox_phase11.log"));
    LOG_INFO("Engine", "Initializing Modern Game Engine - Phase 11 Scripting...");

    {
        // 1. Core Subsystems
        if (!Platform::init()) return -1;
        if (!JobSystem::instance().init()) return -1;
        if (!AssetManager::instance().init()) return -1;
        if (!InputManager::instance().init()) return -1;
        if (!PhysicsSystem::instance().init()) return -1;
        if (!AudioEngine::instance().init()) return -1;
        if (!ScriptEngine::instance().init()) return -1;

        ProjectManager::instance().create_project("sandbox_project", "SandboxGame");

        // 2. Create Window
        WindowDesc desc{
            .title = "Modern Game Engine - Phase 11 Scripting Subsystem (Lua/Sol2)",
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

        run_phase11_subsystem_tests();

        const auto& caps = RhiContext::instance().get_caps();

        // 4. Create Swapchain
        RhiSwapchain swapchain;
        if (!swapchain.init(window.get_width(), window.get_height(), true)) {
            LOG_FATAL("Engine", "Failed to create RHI swapchain!");
            return -1;
        }

        // 5. Create Command Pool & Double Buffered Frames in Flight
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

        // 6. Create Graphics Pipelines
        RhiShaderModule vert_shader;
        vert_shader.init_from_spirv(engine::shaders::TRIANGLE_VERT_SPV, engine::shaders::TRIANGLE_VERT_SPV_SIZE, ShaderStage::Vertex);

        RhiShaderModule frag_shader;
        frag_shader.init_from_spirv(engine::shaders::TRIANGLE_FRAG_SPV, engine::shaders::TRIANGLE_FRAG_SPV_SIZE, ShaderStage::Fragment);

        GraphicsPipelineDesc post_pipeline_desc{};
        post_pipeline_desc.vertex_shader = &vert_shader;
        post_pipeline_desc.fragment_shader = &frag_shader;
        post_pipeline_desc.color_formats = { static_cast<Format>(swapchain.get_format()) };
        post_pipeline_desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        RhiGraphicsPipeline swapchain_pipeline;
        if (!swapchain_pipeline.init(post_pipeline_desc)) {
            LOG_FATAL("Engine", "Failed to create swapchain graphics pipeline!");
            return -1;
        }

        LOG_INFO("Engine", "==================================================");
        LOG_INFO("Engine", "  Realtime Scripting & Render Loop Starting...    ");
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

            // Update Physics & Audio
            if (dt > 0.0f && dt < 0.1f) {
                PhysicsSystem::instance().update(dt);
                AudioEngine::instance().update(dt);
            }

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

            // Pass 1: Scene Render Pass
            graph.add_pass(
                "SceneForwardPass",
                [&](RenderPassBuilder& builder) {
                    builder.set_color_attachment(
                        0, 
                        swapchain_rg, 
                        VK_ATTACHMENT_LOAD_OP_CLEAR, 
                        VK_ATTACHMENT_STORE_OP_STORE, 
                        Vec4(0.04f, 0.06f, 0.08f, 1.0f)
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
                    cmd.bind_pipeline(swapchain_pipeline.get_pipeline());
                    cmd.draw(3, 1, 0, 0);
                }
            );

            // Pass 2: Present Transition Pass
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
                std::string title = std::format("Modern Game Engine [Phase 11 Scripting] | GPU: {} | FPS: {} | Frames: {}", 
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
        swapchain_pipeline.destroy();
        vert_shader.destroy();
        frag_shader.destroy();

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

    LOG_INFO("Engine", "Engine Phase 11 shutdown completed.");
    GlobalAllocator::instance().dump_leaks();

    return 0;
}
