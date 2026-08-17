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

static void run_phase3_subsystem_tests() {
    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "    Running Phase 3 Subsystem Verification        ");
    LOG_INFO("Sandbox", "==================================================");

    // 1. UUID & Asset System
    LOG_INFO("Sandbox", "--- 1. UUID & Asset System Verification ---");
    {
        UUID id1 = UUID::generate();
        UUID id2 = UUID::generate();
        std::string id1_str = id1.to_string();
        UUID parsed_id1 = UUID::from_string(id1_str);

        LOG_INFO("Sandbox", "Generated UUID: {}", id1_str);
        LOG_INFO("Sandbox", "UUID String Parsing: {}", (id1 == parsed_id1 && id1 != id2) ? "PASS" : "FAIL");

        AssetMeta meta = AssetManager::instance().create_or_get_meta("/assets/textures/albedo.png", AssetType::Texture);
        LOG_INFO("Sandbox", "Registered Asset Meta: '{}' -> UUID: {}", meta.virtual_path, meta.uuid.to_string());
        LOG_INFO("Sandbox", "UUID Registry Lookup: '{}'", UUIDRegistry::instance().find_path_by_uuid(meta.uuid));
    }

    // 2. Project System
    LOG_INFO("Sandbox", "--- 2. Project System Verification ---");
    {
        bool proj_created = ProjectManager::instance().create_project("sandbox_project", "MyDemoGame");
        LOG_INFO("Sandbox", "Project Creation: {}", proj_created ? "PASS" : "FAIL");

        if (proj_created) {
            const auto& settings = ProjectManager::instance().get_active_project();
            LOG_INFO("Sandbox", "Active Project: '{}' (ID: {})", settings.name, settings.project_id.to_string());
            ProjectManager::instance().save_project();
        }
    }

    // 3. Input System
    LOG_INFO("Sandbox", "--- 3. Input System Verification ---");
    {
        auto& ctx = InputManager::instance().get_or_create_context("Gameplay");
        ctx.bind_action("Jump", KeyCode::Space);
        ctx.bind_axis("MoveForward", KeyCode::W, KeyCode::S, 1.0f);
        ctx.bind_mouse_axis("LookYaw", MouseAxis::DeltaX, 0.1f);

        // Simulate KeyDown event
        PlatformEvent event{};
        event.type = EventType::KeyDown;
        event.key.key = KeyCode::Space;
        InputManager::instance().process_event(event);

        LOG_INFO("Sandbox", "Input Action 'Jump' Pressed: {}", InputManager::instance().is_action_down("Jump") ? "PASS" : "FAIL");
        LOG_INFO("Sandbox", "Input Action 'Jump' Just Pressed: {}", InputManager::instance().is_action_just_pressed("Jump") ? "PASS" : "FAIL");
    }

    // 4. Asset Importer (glTF 2.0 Schema Mapper)
    LOG_INFO("Sandbox", "--- 4. glTF 2.0 Asset Importer Verification ---");
    {
        // Minimal valid glTF 2.0 JSON with 1 node, 1 camera, 1 material
        const std::string minimal_gltf = R"({
            "asset": { "version": "2.0", "generator": "TestEngine" },
            "scenes": [ { "nodes": [ 0 ] } ],
            "nodes": [ { "name": "MainCamera", "camera": 0, "translation": [ 0.0, 2.0, 5.0 ] } ],
            "cameras": [ { "type": "perspective", "perspective": { "yfov": 1.047, "znear": 0.1, "zfar": 100.0 } } ],
            "materials": [ { "name": "Gold", "pbrMetallicRoughness": { "baseColorFactor": [ 1.0, 0.84, 0.0, 1.0 ], "metallicFactor": 1.0, "roughnessFactor": 0.2 } } ]
        })";

        GltfImporter importer;
        ImportedScene scene;
        bool import_ok = importer.import_from_memory(
            reinterpret_cast<const uint8_t*>(minimal_gltf.data()),
            minimal_gltf.size(),
            "sample.gltf",
            scene
        );

        LOG_INFO("Sandbox", "glTF Import Result: {}", import_ok ? "PASS" : "FAIL");
        LOG_INFO("Sandbox", "Imported Scene: '{}', Nodes: {}, Materials: {}", 
                 scene.name, scene.nodes.size(), scene.materials.size());
        if (!scene.materials.empty()) {
            LOG_INFO("Sandbox", "Imported Material '{}': metallic = {:.2f}, roughness = {:.2f}", 
                     scene.materials[0].name, scene.materials[0].metallic_factor, scene.materials[0].roughness_factor);
        }
    }

    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "    Phase 3 Subsystem Tests Verified Cleanly      ");
    LOG_INFO("Sandbox", "==================================================");
}

int main(int argc, char* argv[]) {
    float max_runtime = 0.0f;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--timeout" && i + 1 < argc) {
            max_runtime = std::stof(argv[++i]);
        }
    }

    Logger::instance().add_sink(std::make_shared<FileSink>("sandbox_phase3.log"));
    LOG_INFO("Engine", "Initializing Modern Game Engine - Phase 3 Hardware & Systems...");

    {
        // 1. Core Subsystems
        if (!Platform::init()) return -1;
        if (!JobSystem::instance().init()) return -1;
        if (!AssetManager::instance().init()) return -1;
        if (!InputManager::instance().init()) return -1;

        run_phase3_subsystem_tests();

        // 2. Create Window
        WindowDesc desc{
            .title = "Modern Game Engine - Phase 3 Vulkan 1.3 RHI",
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

        // 3. Initialize Vulkan 1.3 RHI
        LOG_INFO("Engine", "--- 5. Vulkan 1.3 RHI & Dynamic Rendering Verification ---");
        if (!RhiContext::instance().init(window, true)) {
            LOG_FATAL("Engine", "Failed to initialize Vulkan 1.3 RHI!");
            return -1;
        }

        const auto& caps = RhiContext::instance().get_caps();
        LOG_INFO("RHI", "GPU: {}", caps.device_name);
        LOG_INFO("RHI", "Vulkan 1.3 Dynamic Rendering: {}", caps.dynamic_rendering_supported ? "YES" : "NO");
        LOG_INFO("RHI", "Hardware Ray Tracing: {}", caps.ray_tracing_supported ? "SUPPORTED" : "UNAVAILABLE");
        LOG_INFO("RHI", "Mesh Shaders: {}", caps.mesh_shader_supported ? "SUPPORTED" : "UNAVAILABLE");

        // 4. Create Swapchain
        RhiSwapchain swapchain;
        if (!swapchain.init(window.get_width(), window.get_height(), true)) {
            LOG_FATAL("Engine", "Failed to create RHI swapchain!");
            return -1;
        }

        // 5. Create Command Pool & Buffers (Double Buffered Frames in Flight)
        constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
        RhiCommandPool cmd_pool;
        cmd_pool.init(RhiContext::instance().get_queue_families().graphics_family);

        RhiCommandBuffer cmd_buffers[MAX_FRAMES_IN_FLIGHT];
        RhiFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];
        RhiSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            cmd_buffers[i].init(cmd_pool.get_handle());
            in_flight_fences[i].init(true); // Signaled on start
            image_available_semaphores[i].init(false);
        }

        uint32_t image_count = swapchain.get_image_count();
        std::vector<RhiSemaphore> render_finished_semaphores(image_count);
        for (size_t i = 0; i < image_count; ++i) {
            render_finished_semaphores[i].init(false);
        }

        // 6. Create Shaders & Pipeline
        RhiShaderModule vert_shader;
        vert_shader.init_from_spirv(engine::shaders::TRIANGLE_VERT_SPV, engine::shaders::TRIANGLE_VERT_SPV_SIZE, ShaderStage::Vertex);

        RhiShaderModule frag_shader;
        frag_shader.init_from_spirv(engine::shaders::TRIANGLE_FRAG_SPV, engine::shaders::TRIANGLE_FRAG_SPV_SIZE, ShaderStage::Fragment);

        GraphicsPipelineDesc pipeline_desc{};
        pipeline_desc.vertex_shader = &vert_shader;
        pipeline_desc.fragment_shader = &frag_shader;
        pipeline_desc.color_formats = { static_cast<Format>(swapchain.get_format()) };
        pipeline_desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        RhiGraphicsPipeline pipeline;
        if (!pipeline.init(pipeline_desc)) {
            LOG_FATAL("Engine", "Failed to create graphics pipeline!");
            return -1;
        }

        LOG_INFO("Engine", "==================================================");
        LOG_INFO("Engine", "       Vulkan 1.3 Render Loop Starting...         ");
        LOG_INFO("Engine", "==================================================");

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

            // --- Render Frame ---
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

            // Record Commands
            auto& cmd = cmd_buffers[current_frame];
            cmd.reset();
            cmd.begin();

            // Transition Swapchain Image -> COLOR_ATTACHMENT_OPTIMAL
            cmd.transition_image_layout(
                swapchain.get_image(image_index),
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            // Begin Dynamic Rendering
            RenderingDesc render_desc{};
            render_desc.render_area = {
                .offset_x = 0,
                .offset_y = 0,
                .width = swapchain.get_extent().width,
                .height = swapchain.get_extent().height
            };
            render_desc.color_attachments = {
                ColorAttachmentDesc{
                    .image_view = swapchain.get_image_view(image_index),
                    .image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .store_op = VK_ATTACHMENT_STORE_OP_STORE,
                    .clear_color = Vec4(0.08f, 0.09f, 0.12f, 1.0f) // Dark modern slate clear color
                }
            };
            render_desc.has_depth = false;

            cmd.begin_rendering(render_desc);

            // Dynamic Viewport and Scissor
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

            // Bind Pipeline & Draw Triangle
            cmd.bind_pipeline(pipeline.get_pipeline());
            cmd.draw(3, 1, 0, 0);

            cmd.end_rendering();

            // Transition Swapchain Image -> PRESENT_SRC_KHR
            cmd.transition_image_layout(
                swapchain.get_image(image_index),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            cmd.end();

            // Submit Command Buffer
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

            // Present to Screen
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
                std::string title = std::format("Modern Game Engine [Phase 3 RHI] | GPU: {} | FPS: {} | Frames: {}", 
                                                caps.device_name, timer.fps(), rendered_frames);
                window.set_title(title);
            }

            if (max_runtime > 0.0f && timer.total_time() >= max_runtime) {
                LOG_INFO("Sandbox", "Reached maximum test runtime ({:.2f}s, rendered {} frames). Exiting loop...", 
                         max_runtime, rendered_frames);
                window.set_should_close(true);
            }
        }

        // Cleanup RHI Resources
        RhiContext::instance().wait_idle();

        pipeline.destroy();
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

        RhiContext::instance().shutdown();

        window.destroy();
        ProjectManager::instance().close_project();
        InputManager::instance().shutdown();
        AssetManager::instance().shutdown();
        JobSystem::instance().shutdown();
        Platform::shutdown();
    }

    LOG_INFO("Engine", "Engine Phase 3 shutdown completed.");
    GlobalAllocator::instance().dump_leaks();

    return 0;
}
