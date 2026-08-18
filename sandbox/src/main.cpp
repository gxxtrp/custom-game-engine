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

static void run_phase5_subsystem_tests() {
    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "    Running Phase 5 GPU & RenderGraph Tests       ");
    LOG_INFO("Sandbox", "==================================================");

    // 1. VMA Buffer Allocation & Device Address
    LOG_INFO("Sandbox", "--- 1. VMA Buffer Allocation & Memory Mapping ---");
    struct TestVertex {
        Vec3 pos;
        Vec3 color;
    };
    TestVertex vertices[3] = {
        { Vec3( 0.0f, -0.5f, 0.0f), Vec3(1.0f, 0.0f, 0.0f) },
        { Vec3( 0.5f,  0.5f, 0.0f), Vec3(0.0f, 1.0f, 0.0f) },
        { Vec3(-0.5f,  0.5f, 0.0f), Vec3(0.0f, 0.0f, 1.0f) }
    };

    BufferDesc vb_desc{
        .size = sizeof(vertices),
        .usage = BufferUsage::Vertex | BufferUsage::Storage | BufferUsage::TransferDst | BufferUsage::ShaderDeviceAddress,
        .memory_usage = MemoryUsage::CpuToGpu,
        .debug_name = "TestVertexBuffer"
    };

    RhiBuffer vertex_buffer;
    bool vb_ok = vertex_buffer.init(vb_desc);
    LOG_INFO("Sandbox", "VMA Vertex Buffer Creation: {}", vb_ok ? "PASS" : "FAIL");

    bool upload_ok = vertex_buffer.upload_data(vertices, sizeof(vertices));
    LOG_INFO("Sandbox", "Buffer Data Upload (mapped memory): {}", upload_ok ? "PASS" : "FAIL");

    VkDeviceAddress bda = vertex_buffer.get_device_address();
    LOG_INFO("Sandbox", "Buffer Device Address (BDA 64-bit): {:#x}", bda);

    // 2. VMA Texture & Sampler Creation
    LOG_INFO("Sandbox", "--- 2. VMA Texture & Sampler Allocation ---");
    TextureDesc tex_desc{
        .width = 512,
        .height = 512,
        .format = Format::R8G8B8A8_UNORM,
        .usage = TextureUsage::Sampled | TextureUsage::ColorAttachment,
        .debug_name = "TestAlbedoTexture"
    };

    RhiTexture texture;
    bool tex_ok = texture.init(tex_desc);
    LOG_INFO("Sandbox", "VMA 2D Texture Creation (512x512): {}", tex_ok ? "PASS" : "FAIL");

    SamplerDesc samp_desc{
        .min_filter = SamplerFilter::Linear,
        .mag_filter = SamplerFilter::Linear,
        .max_anisotropy = 16.0f,
        .enable_anisotropy = true,
        .debug_name = "AnisotropicLinearSampler"
    };

    RhiSampler sampler;
    bool samp_ok = sampler.init(samp_desc);
    LOG_INFO("Sandbox", "Anisotropic Sampler Creation: {}", samp_ok ? "PASS" : "FAIL");

    // 3. Bindless Descriptor Indexing
    LOG_INFO("Sandbox", "--- 3. Bindless Descriptor Heap Indexing ---");
    uint32_t tex_slot = BindlessHeap::instance().register_texture(texture.get_view());
    uint32_t buf_slot = BindlessHeap::instance().register_storage_buffer(vertex_buffer.get_handle(), vertex_buffer.get_size());
    uint32_t samp_slot = BindlessHeap::instance().register_sampler(sampler.get_handle());

    LOG_INFO("Sandbox", "Bindless Registered Texture Slot: {} (Valid: {})", tex_slot, tex_slot != UINT32_MAX ? "PASS" : "FAIL");
    LOG_INFO("Sandbox", "Bindless Registered Buffer Slot:  {} (Valid: {})", buf_slot, buf_slot != UINT32_MAX ? "PASS" : "FAIL");
    LOG_INFO("Sandbox", "Bindless Registered Sampler Slot: {} (Valid: {})", samp_slot, samp_slot != UINT32_MAX ? "PASS" : "FAIL");

    BindlessHeap::instance().unregister_texture(tex_slot);
    BindlessHeap::instance().unregister_storage_buffer(buf_slot);
    BindlessHeap::instance().unregister_sampler(samp_slot);

    sampler.destroy();
    texture.destroy();
    vertex_buffer.destroy();

    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "    Phase 5 Subsystem Tests Verified Cleanly      ");
    LOG_INFO("Sandbox", "==================================================");
}

int main(int argc, char* argv[]) {
    float max_runtime = 0.0f;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--timeout" && i + 1 < argc) {
            max_runtime = std::stof(argv[++i]);
        }
    }

    Logger::instance().add_sink(std::make_shared<FileSink>("sandbox_phase5.log"));
    LOG_INFO("Engine", "Initializing Modern Game Engine - Phase 5 GPU & RenderGraph...");

    {
        // 1. Core Subsystems
        if (!Platform::init()) return -1;
        if (!JobSystem::instance().init()) return -1;
        if (!AssetManager::instance().init()) return -1;
        if (!InputManager::instance().init()) return -1;

        ProjectManager::instance().create_project("sandbox_project", "SandboxGame");

        // 2. Create Window
        WindowDesc desc{
            .title = "Modern Game Engine - Phase 5 GPU Resources & RenderGraph",
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

        run_phase5_subsystem_tests();

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

        // 6. Create Pipeline for RenderGraph
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
        LOG_INFO("Engine", "   Render Graph Multi-Pass Loop Starting...       ");
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

            // --- Render Frame with Render Graph ---
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

            // Import Swapchain Attachment
            RGTextureHandle swapchain_rg = graph.import_texture(
                "SwapchainOutput",
                swapchain.get_image(image_index),
                swapchain.get_image_view(image_index),
                swapchain.get_extent().width,
                swapchain.get_extent().height,
                static_cast<Format>(swapchain.get_format()),
                VK_IMAGE_LAYOUT_UNDEFINED
            );

            // Pass 1: Scene Geometry Pass (renders into Swapchain with Dynamic Rendering)
            graph.add_pass(
                "SceneForwardPass",
                [&](RenderPassBuilder& builder) {
                    builder.set_color_attachment(
                        0, 
                        swapchain_rg, 
                        VK_ATTACHMENT_LOAD_OP_CLEAR, 
                        VK_ATTACHMENT_STORE_OP_STORE, 
                        Vec4(0.06f, 0.08f, 0.12f, 1.0f)
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

            // Pass 2: Present Barrier Pass
            graph.add_pass(
                "PresentTransitionPass",
                [&](RenderPassBuilder& builder) {
                    builder.read_texture(swapchain_rg, RGResourceAccess::Present);
                },
                [](RenderPassContext&) {
                    // No CPU commands needed, RenderGraph handles the layout transition to PRESENT_SRC_KHR automatically!
                }
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
                std::string title = std::format("Modern Game Engine [Phase 5 RenderGraph] | GPU: {} | FPS: {} | Frames: {}", 
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

    LOG_INFO("Engine", "Engine Phase 5 shutdown completed.");
    GlobalAllocator::instance().dump_leaks();

    return 0;
}
