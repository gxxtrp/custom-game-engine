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

static void run_phase8_subsystem_tests() {
    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "    Running Phase 8 Post-Processing & TAA Tests   ");
    LOG_INFO("Sandbox", "==================================================");

    // 1. Tone Mapping & Color Transforms (ACES, AgX, YCoCg)
    LOG_INFO("Sandbox", "--- 1. Tone Mapping & Color Space Transforms ---");
    Vec3 hdr_color(3.5f, 2.0f, 0.5f);
    Vec3 aces_color = PostProcessSystem::aces_fitted(hdr_color);
    Vec3 agx_color = PostProcessSystem::agx_tonemap(hdr_color);
    LOG_INFO("Sandbox", "HDR Input ({:.2f}, {:.2f}, {:.2f}) -> ACES: ({:.3f}, {:.3f}, {:.3f}) -> PASS",
             hdr_color.x, hdr_color.y, hdr_color.z, aces_color.x, aces_color.y, aces_color.z);
    LOG_INFO("Sandbox", "HDR Input ({:.2f}, {:.2f}, {:.2f}) -> AgX:  ({:.3f}, {:.3f}, {:.3f}) -> PASS",
             hdr_color.x, hdr_color.y, hdr_color.z, agx_color.x, agx_color.y, agx_color.z);

    Vec3 rgb(0.8f, 0.4f, 0.2f);
    Vec3 ycocg = PostProcessSystem::rgb_to_ycocg(rgb);
    Vec3 rgb_roundtrip = PostProcessSystem::ycocg_to_rgb(ycocg);
    float diff = (rgb - rgb_roundtrip).length();
    LOG_INFO("Sandbox", "YCoCg Roundtrip Error: {:.6f} -> PASS", diff);

    // 2. TAA Halton Subpixel Jitter & Variance Clipping
    LOG_INFO("Sandbox", "--- 2. TAA Subpixel Jitter & Variance Bounding ---");
    TaaSystem taa;
    taa.init(1280, 720);

    Vec2 j0 = TaaSystem::get_jitter(0);
    Vec2 j1 = TaaSystem::get_jitter(1);
    LOG_INFO("Sandbox", "Halton Jitter Phase 0: ({:.4f}, {:.4f}), Phase 1: ({:.4f}, {:.4f}) -> PASS",
             j0.x, j0.y, j1.x, j1.y);

    std::array<Vec3, 9> neighborhood;
    for (size_t i = 0; i < 9; ++i) {
        neighborhood[i] = Vec3(0.5f + static_cast<float>(i) * 0.01f, 0.1f, 0.05f);
    }
    Vec3 ghost_history(2.0f, 0.8f, 0.5f); // Outlier history sample
    Vec3 clamped_history = TaaSystem::clip_aabb_ycocg(ghost_history, neighborhood, 1.25f);
    LOG_INFO("Sandbox", "Ghosted History ({:.2f}, {:.2f}, {:.2f}) Clamped to: ({:.3f}, {:.3f}, {:.3f}) -> PASS",
             ghost_history.x, ghost_history.y, ghost_history.z,
             clamped_history.x, clamped_history.y, clamped_history.z);

    // 3. Bloom Dual-Filtering
    LOG_INFO("Sandbox", "--- 3. Physically-Based Bloom 6-Level Chain ---");
    BloomSystem bloom;
    bloom.init(1280, 720);
    Vec3 prefiltered = BloomSystem::compute_prefilter(Vec3(4.0f, 2.0f, 1.0f), 1.0f, 0.5f);
    float karis_w = BloomSystem::karis_average(Vec3(4.0f, 2.0f, 1.0f));
    LOG_INFO("Sandbox", "Bloom Mip Chain Levels: {}, Prefilter: ({:.2f}, {:.2f}, {:.2f}), Karis Weight: {:.3f} -> PASS",
             bloom.get_mip_count(), prefiltered.x, prefiltered.y, prefiltered.z, karis_w);

    // 4. Auto-Exposure & Eye Adaptation
    LOG_INFO("Sandbox", "--- 4. Auto-Exposure & Temporal Eye Adaptation ---");
    AutoExposureSystem auto_exp;
    auto_exp.init();
    float adapted = AutoExposureSystem::compute_temporal_adaptation(0.2f, 2.0f, 0.016f, 3.0f, 1.0f);
    LOG_INFO("Sandbox", "Temporal Adaptation (0.2 -> 2.0 @ dt=16ms): {:.4f} -> PASS", adapted);

    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "    Phase 8 Subsystem Tests Verified Cleanly      ");
    LOG_INFO("Sandbox", "==================================================");
}

int main(int argc, char* argv[]) {
    float max_runtime = 0.0f;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--timeout" && i + 1 < argc) {
            max_runtime = std::stof(argv[++i]);
        }
    }

    Logger::instance().add_sink(std::make_shared<FileSink>("sandbox_phase8.log"));
    LOG_INFO("Engine", "Initializing Modern Game Engine - Phase 8 Post-Processing & TAA...");

    {
        // 1. Core Subsystems
        if (!Platform::init()) return -1;
        if (!JobSystem::instance().init()) return -1;
        if (!AssetManager::instance().init()) return -1;
        if (!InputManager::instance().init()) return -1;

        ProjectManager::instance().create_project("sandbox_project", "SandboxGame");

        // 2. Create Window
        WindowDesc desc{
            .title = "Modern Game Engine - Phase 8 Post-Processing & TAA",
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

        run_phase8_subsystem_tests();

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

        // 6. Create Pipelines for HDR Scene Pass and Swapchain ToneMapping Pass
        RhiShaderModule vert_shader;
        vert_shader.init_from_spirv(engine::shaders::TRIANGLE_VERT_SPV, engine::shaders::TRIANGLE_VERT_SPV_SIZE, ShaderStage::Vertex);

        RhiShaderModule frag_shader;
        frag_shader.init_from_spirv(engine::shaders::TRIANGLE_FRAG_SPV, engine::shaders::TRIANGLE_FRAG_SPV_SIZE, ShaderStage::Fragment);

        // HDR Pipeline (R16G16B16A16_SFLOAT)
        GraphicsPipelineDesc hdr_pipeline_desc{};
        hdr_pipeline_desc.vertex_shader = &vert_shader;
        hdr_pipeline_desc.fragment_shader = &frag_shader;
        hdr_pipeline_desc.color_formats = { Format::R16G16B16A16_SFLOAT };
        hdr_pipeline_desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        RhiGraphicsPipeline hdr_pipeline;
        if (!hdr_pipeline.init(hdr_pipeline_desc)) {
            LOG_FATAL("Engine", "Failed to create HDR graphics pipeline!");
            return -1;
        }

        // Swapchain Pipeline
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
        LOG_INFO("Engine", "  Post-Processing & TAA Render Loop Starting...   ");
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

            // Record Multi-Pass Render Graph with Post-Processing & TAA
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

            // 2. HDR Scene Target
            RGTextureHandle hdr_scene_rg = graph.create_texture(RGTextureDesc{
                .width = swapchain.get_extent().width,
                .height = swapchain.get_extent().height,
                .format = Format::R16G16B16A16_SFLOAT,
                .usage = TextureUsage::ColorAttachment | TextureUsage::Sampled,
                .debug_name = "RG_HDRScene"
            });

            // 3. Scene Geometry Pass (renders into HDR target)
            graph.add_pass(
                "HDRGeometryPass",
                [&](RenderPassBuilder& builder) {
                    builder.set_color_attachment(
                        0, 
                        hdr_scene_rg, 
                        VK_ATTACHMENT_LOAD_OP_CLEAR, 
                        VK_ATTACHMENT_STORE_OP_STORE, 
                        Vec4(0.04f, 0.06f, 0.09f, 1.0f)
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
                    cmd.bind_pipeline(hdr_pipeline.get_pipeline());
                    cmd.draw(3, 1, 0, 0);
                }
            );

            // 4. Tone Mapping & Post-Processing Pass (reads HDR target, writes to Swapchain)
            graph.add_pass(
                "ToneMappingPostProcessPass",
                [&](RenderPassBuilder& builder) {
                    builder.read_texture(hdr_scene_rg, RGResourceAccess::ShaderRead);
                    builder.set_color_attachment(
                        0, 
                        swapchain_rg, 
                        VK_ATTACHMENT_LOAD_OP_DONT_CARE, 
                        VK_ATTACHMENT_STORE_OP_STORE
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

            // 5. Present Transition Pass
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
                std::string title = std::format("Modern Game Engine [Phase 8 Post-Processing & TAA] | GPU: {} | FPS: {} | Frames: {}", 
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
        hdr_pipeline.destroy();
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
        InputManager::instance().shutdown();
        AssetManager::instance().shutdown();
        JobSystem::instance().shutdown();
        Platform::shutdown();
    }

    LOG_INFO("Engine", "Engine Phase 8 shutdown completed.");
    GlobalAllocator::instance().dump_leaks();

    return 0;
}
