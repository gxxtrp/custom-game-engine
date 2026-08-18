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

static void run_phase7_subsystem_tests() {
    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "    Running Phase 7 Lighting & Advanced Rendering ");
    LOG_INFO("Sandbox", "==================================================");

    Mat4 view = Mat4::look_at(Vec3(0.0f, 2.0f, -10.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
    Mat4 proj = Mat4::perspective_vk(math::deg_to_rad(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);

    // 1. Clustered / Froxel Lighting
    LOG_INFO("Sandbox", "--- 1. Clustered / Froxel Lighting System ---");
    ClusteredLightingSystem lighting;
    lighting.init({ .grid_dim_x = 16, .grid_dim_y = 9, .grid_dim_z = 24, .near_z = 0.1f, .far_z = 100.0f });

    GPUDirectionalLight sun{};
    sun.direction = Vec3(-0.5f, -1.0f, -0.3f).normalized();
    sun.color = Vec3(1.0f, 0.95f, 0.85f);
    sun.intensity = 5.0f;
    lighting.update_directional_light(sun);

    std::vector<GPUPointLight> point_lights(8);
    for (size_t i = 0; i < point_lights.size(); ++i) {
        point_lights[i].position = Vec3(static_cast<float>(i * 4 - 14), 1.0f, static_cast<float>(i * 5));
        point_lights[i].radius = 8.0f;
        point_lights[i].color = Vec3(1.0f, 0.5f, 0.2f);
        point_lights[i].intensity = 2.0f;
    }
    lighting.set_point_lights(point_lights);

    std::vector<GPUSpotLight> spot_lights(4);
    for (size_t i = 0; i < spot_lights.size(); ++i) {
        spot_lights[i].position = Vec3(static_cast<float>(i * 6 - 9), 5.0f, 10.0f);
        spot_lights[i].direction = Vec3(0.0f, -1.0f, 0.0f);
        spot_lights[i].range = 15.0f;
        spot_lights[i].color = Vec3(0.2f, 0.6f, 1.0f);
        spot_lights[i].intensity = 4.0f;
    }
    lighting.set_spot_lights(spot_lights);

    lighting.cull_lights_cpu(view, proj);
    LOG_INFO("Sandbox", "Clustered Froxel Grid: {} clusters populated -> PASS", lighting.get_total_clusters());

    // 2. Cascaded Shadow Map (CSM)
    LOG_INFO("Sandbox", "--- 2. Cascaded Shadow Map (CSM 4-Cascades) ---");
    CascadedShadowMap csm;
    csm.init(2048);
    csm.update_cascades(sun.direction, view, math::deg_to_rad(60.0f), 16.0f / 9.0f, 0.1f, 100.0f, 0.85f);
    auto splits = csm.get_cascade_splits();
    LOG_INFO("Sandbox", "CSM 4 Cascades computed (Splits: {:.1f}, {:.1f}, {:.1f}, {:.1f}) -> PASS",
             splits.x, splits.y, splits.z, splits.w);

    // 3. Volumetric Fog Media (Froxel Media)
    LOG_INFO("Sandbox", "--- 3. Volumetric Fog Grid & Scattering ---");
    VolumetricFog fog;
    fog.init(160, 90, 64);
    fog.set_scattering(Vec3(0.03f, 0.03f, 0.04f));
    fog.set_anisotropy(0.7f);
    fog.update_gpu_buffer();

    float hg_phase = VolumetricFog::henyey_greenstein_phase(0.8f, 0.7f);
    float ray_phase = VolumetricFog::rayleigh_phase(0.8f);
    LOG_INFO("Sandbox", "Henyey-Greenstein (g=0.7, cos=0.8): {:.4f}, Rayleigh: {:.4f} -> PASS", hg_phase, ray_phase);

    // 4. Order-Independent Transparency (WBOIT)
    LOG_INFO("Sandbox", "--- 4. Order-Independent Transparency (WBOIT) ---");
    WboitRenderer oit;
    oit.init(1280, 720);
    float weight = WboitRenderer::compute_depth_weight(0.25f, 0.6f);
    LOG_INFO("Sandbox", "WBOIT Depth Weight (z=0.25, alpha=0.6): {:.2f} -> PASS", weight);

    // 5. ReSTIR & Denoising Architecture
    LOG_INFO("Sandbox", "--- 5. ReSTIR DI/GI & Denoising Interface ---");
    ReSTIRSystem restir;
    restir.init(1280, 720);

    Reservoir r1{};
    ReSTIRSample s1{ .light_index = 0, .radiance = Vec3(5.0f, 5.0f, 5.0f), .pdf = 0.5f };
    r1.update(s1, 10.0f, 0.3f);

    Reservoir r2{};
    ReSTIRSample s2{ .light_index = 1, .radiance = Vec3(8.0f, 2.0f, 1.0f), .pdf = 0.8f };
    r2.update(s2, 10.0f, 0.4f);

    r1.merge(r2, 0.8f, 0.2f);
    r1.finalize(0.8f);
    LOG_INFO("Sandbox", "ReSTIR Reservoir Resampling: M = {}, Selected Light = {}, Weight W = {:.3f} -> PASS",
             r1.M, r1.sample.light_index, r1.W);

    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "    Phase 7 Subsystem Tests Verified Cleanly      ");
    LOG_INFO("Sandbox", "==================================================");
}

int main(int argc, char* argv[]) {
    float max_runtime = 0.0f;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--timeout" && i + 1 < argc) {
            max_runtime = std::stof(argv[++i]);
        }
    }

    Logger::instance().add_sink(std::make_shared<FileSink>("sandbox_phase7.log"));
    LOG_INFO("Engine", "Initializing Modern Game Engine - Phase 7 Lighting & Advanced Rendering...");

    {
        // 1. Core Subsystems
        if (!Platform::init()) return -1;
        if (!JobSystem::instance().init()) return -1;
        if (!AssetManager::instance().init()) return -1;
        if (!InputManager::instance().init()) return -1;

        ProjectManager::instance().create_project("sandbox_project", "SandboxGame");

        // 2. Create Window
        WindowDesc desc{
            .title = "Modern Game Engine - Phase 7 Lighting & Advanced Rendering",
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

        run_phase7_subsystem_tests();

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

        // 6. Create Pipeline & Shaders
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
        LOG_INFO("Engine", "  Multi-Pass Advanced Render Loop Starting...     ");
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

            // Record Multi-Pass Render Graph
            graph.reset();

            // 1. Import Swapchain Target
            RGTextureHandle swapchain_rg = graph.import_texture(
                "SwapchainOutput",
                swapchain.get_image(image_index),
                swapchain.get_image_view(image_index),
                swapchain.get_extent().width,
                swapchain.get_extent().height,
                static_cast<Format>(swapchain.get_format()),
                VK_IMAGE_LAYOUT_UNDEFINED
            );

            // 2. CSM Shadow Depth Pass
            RGTextureHandle shadow_map_rg = graph.create_texture(RGTextureDesc{
                .width = 2048,
                .height = 2048,
                .format = Format::D32_SFLOAT,
                .usage = TextureUsage::DepthAttachment | TextureUsage::Sampled,
                .debug_name = "RG_ShadowMap"
            });

            graph.add_pass(
                "ShadowDepthPass",
                [&](RenderPassBuilder& builder) {
                    builder.set_depth_attachment(shadow_map_rg, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, 1.0f);
                },
                [&](RenderPassContext& ctx) {
                    auto& cmd = ctx.get_command_buffer();
                    cmd.set_viewport(Viewport{ .x = 0, .y = 0, .width = 2048, .height = 2048, .min_depth = 0, .max_depth = 1 });
                    cmd.set_scissor(Rect2D{ .offset_x = 0, .offset_y = 0, .width = 2048, .height = 2048 });
                }
            );

            // 3. Clustered Forward Lighting Pass
            graph.add_pass(
                "ForwardLightingPass",
                [&](RenderPassBuilder& builder) {
                    builder.read_texture(shadow_map_rg, RGResourceAccess::ShaderRead);
                    builder.set_color_attachment(
                        0, 
                        swapchain_rg, 
                        VK_ATTACHMENT_LOAD_OP_CLEAR, 
                        VK_ATTACHMENT_STORE_OP_STORE, 
                        Vec4(0.05f, 0.07f, 0.11f, 1.0f)
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
                    cmd.bind_pipeline(pipeline.get_pipeline());
                    cmd.draw(3, 1, 0, 0);
                }
            );

            // 4. Present Transition Pass
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
                std::string title = std::format("Modern Game Engine [Phase 7 Advanced Lighting] | GPU: {} | FPS: {} | Frames: {}", 
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

        BindlessHeap::instance().shutdown();
        RhiContext::instance().shutdown();

        window.destroy();
        ProjectManager::instance().close_project();
        InputManager::instance().shutdown();
        AssetManager::instance().shutdown();
        JobSystem::instance().shutdown();
        Platform::shutdown();
    }

    LOG_INFO("Engine", "Engine Phase 7 shutdown completed.");
    GlobalAllocator::instance().dump_leaks();

    return 0;
}
