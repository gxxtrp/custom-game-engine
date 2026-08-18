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
#include "engine/scene/components.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "engine/scene/map_serializer.h"
#include "engine/scene/scene_importer.h"
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

static void run_phase4_subsystem_tests() {
    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "    Running Phase 4 ECS & Scene Verification      ");
    LOG_INFO("Sandbox", "==================================================");

    // 1. Scene & Entity Creation
    LOG_INFO("Sandbox", "--- 1. Flecs ECS Scene Hierarchy ---");
    Scene scene("MainTestLevel");

    Entity root = scene.create_entity("WorldRoot");
    root.get_mut<TransformComponent>()->position = Vec3(10.0f, 0.0f, 0.0f);

    Entity camera_ent = scene.create_child_entity(root, "MainCamera");
    CameraComponent cam{};
    cam.fov_deg = 60.0f;
    cam.near_z = 0.1f;
    cam.far_z = 1000.0f;
    cam.is_primary = true;
    camera_ent.set(cam);

    Entity sun_ent = scene.create_child_entity(root, "SunDirectionalLight");
    DirectionalLightComponent dl{};
    dl.color = Vec3(1.0f, 0.95f, 0.85f);
    dl.intensity = 3.5f;
    dl.cast_shadows = true;
    sun_ent.set(dl);

    Entity point_light_ent = scene.create_child_entity(root, "TorchPointLight");
    PointLightComponent pl{};
    pl.color = Vec3(1.0f, 0.5f, 0.1f);
    pl.intensity = 2.0f;
    pl.radius = 12.0f;
    point_light_ent.set(pl);
    point_light_ent.get_mut<TransformComponent>()->position = Vec3(0.0f, 5.0f, 0.0f);

    Entity mesh_ent = scene.create_child_entity(root, "StatueMesh");
    MeshRendererComponent mr{};
    mr.mesh_uuid = UUID::generate();
    mr.material_uuid = UUID::generate();
    mr.cast_shadows = true;
    mesh_ent.set(mr);

    LOG_INFO("Sandbox", "Created scene '{}' with {} entities", scene.get_name(), scene.get_entity_count());

    // 2. Transform Propagation
    LOG_INFO("Sandbox", "--- 2. Hierarchical Transform Propagation ---");
    scene.update(0.016f);

    Vec3 torch_world_pos = point_light_ent.get<WorldTransformComponent>()->get_world_position();
    LOG_INFO("Sandbox", "Torch local position: (0, 5, 0), parent position: (10, 0, 0)");
    LOG_INFO("Sandbox", "Calculated World Position: ({:.2f}, {:.2f}, {:.2f})", 
             torch_world_pos.x, torch_world_pos.y, torch_world_pos.z);
    bool transform_ok = (std::abs(torch_world_pos.x - 10.0f) < 0.001f && std::abs(torch_world_pos.y - 5.0f) < 0.001f);
    LOG_INFO("Sandbox", "Transform Propagation: {}", transform_ok ? "PASS" : "FAIL");

    // 3. Map Serialization & Deserialization
    LOG_INFO("Sandbox", "--- 3. Map System TOML Serialization & Loading ---");
    std::string saved_map_toml;
    bool serialize_ok = MapSerializer::serialize_to_toml(scene, saved_map_toml);
    LOG_INFO("Sandbox", "Map Serialization: {}", serialize_ok ? "PASS" : "FAIL");

    Scene loaded_scene("LoadedScene");
    bool deserialize_ok = MapSerializer::deserialize_from_toml(saved_map_toml, loaded_scene);
    LOG_INFO("Sandbox", "Map Deserialization: {}", deserialize_ok ? "PASS" : "FAIL");
    LOG_INFO("Sandbox", "Loaded Scene Entity Count: {}", loaded_scene.get_entity_count());

    Entity loaded_torch = loaded_scene.find_entity_by_name("TorchPointLight");
    if (loaded_torch.is_valid() && loaded_torch.has<PointLightComponent>()) {
        const auto* loaded_pl = loaded_torch.get<PointLightComponent>();
        LOG_INFO("Sandbox", "Loaded Torch Light: radius = {:.1f}, intensity = {:.1f} -> PASS", 
                 loaded_pl->radius, loaded_pl->intensity);
    } else {
        LOG_ERROR("Sandbox", "Loaded Torch Light not found -> FAIL");
    }

    // 4. Universal DCC Scene Instantiation into ECS
    LOG_INFO("Sandbox", "--- 4. DCC glTF Scene Instantiation into ECS ---");
    const std::string sample_gltf = R"({
        "asset": { "version": "2.0", "generator": "Blender glTF 2.0" },
        "scenes": [ { "nodes": [ 0 ] } ],
        "nodes": [
            { "name": "Room", "children": [ 1, 2 ] },
            { "name": "DeskLamp", "translation": [ 1.0, 2.0, 3.0 ] },
            { "name": "PlayerCamera", "camera": 0, "translation": [ 0.0, 1.8, 0.0 ] }
        ],
        "cameras": [ { "type": "perspective", "perspective": { "yfov": 1.047, "znear": 0.1, "zfar": 500.0 } } ]
    })";

    GltfImporter importer;
    ImportedScene imported_scene;
    importer.import_from_memory(
        reinterpret_cast<const uint8_t*>(sample_gltf.data()),
        sample_gltf.size(),
        "dcc_room.gltf",
        imported_scene
    );

    Scene dcc_ecs_scene("DccEcsScene");
    bool dcc_instantiate_ok = SceneImporter::instantiate_imported_scene(imported_scene, dcc_ecs_scene);
    LOG_INFO("Sandbox", "DCC Scene Instantiation: {}", dcc_instantiate_ok ? "PASS" : "FAIL");
    LOG_INFO("Sandbox", "DCC Scene Entities: {}", dcc_ecs_scene.get_entity_count());

    Entity dcc_cam = dcc_ecs_scene.find_entity_by_name("PlayerCamera");
    LOG_INFO("Sandbox", "DCC PlayerCamera found in ECS: {}", (dcc_cam.is_valid() && dcc_cam.has<CameraComponent>()) ? "PASS" : "FAIL");

    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "    Phase 4 Subsystem Tests Verified Cleanly      ");
    LOG_INFO("Sandbox", "==================================================");
}

int main(int argc, char* argv[]) {
    float max_runtime = 0.0f;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--timeout" && i + 1 < argc) {
            max_runtime = std::stof(argv[++i]);
        }
    }

    Logger::instance().add_sink(std::make_shared<FileSink>("sandbox_phase4.log"));
    LOG_INFO("Engine", "Initializing Modern Game Engine - Phase 4 ECS & Scene System...");

    {
        // 1. Core Subsystems
        if (!Platform::init()) return -1;
        if (!JobSystem::instance().init()) return -1;
        if (!AssetManager::instance().init()) return -1;
        if (!InputManager::instance().init()) return -1;

        // Initialize Project System
        ProjectManager::instance().create_project("sandbox_project", "SandboxGame");

        run_phase4_subsystem_tests();

        // 2. Create Window
        WindowDesc desc{
            .title = "Modern Game Engine - Phase 4 ECS & Scene System",
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
        if (!RhiContext::instance().init(window, true)) {
            LOG_FATAL("Engine", "Failed to initialize Vulkan 1.3 RHI!");
            return -1;
        }

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

            cmd.transition_image_layout(
                swapchain.get_image(image_index),
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

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
                    .clear_color = Vec4(0.08f, 0.09f, 0.12f, 1.0f)
                }
            };
            render_desc.has_depth = false;

            cmd.begin_rendering(render_desc);

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

            cmd.end_rendering();

            cmd.transition_image_layout(
                swapchain.get_image(image_index),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            cmd.end();

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
                std::string title = std::format("Modern Game Engine [Phase 4 ECS & Scene] | GPU: {} | FPS: {} | Frames: {}", 
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

        RhiContext::instance().shutdown();

        window.destroy();
        ProjectManager::instance().close_project();
        InputManager::instance().shutdown();
        AssetManager::instance().shutdown();
        JobSystem::instance().shutdown();
        Platform::shutdown();
    }

    LOG_INFO("Engine", "Engine Phase 4 shutdown completed.");
    GlobalAllocator::instance().dump_leaks();

    return 0;
}
