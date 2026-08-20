#include "editor/core/editor_app.h"
#include "engine/renderer/embedded_shaders.h"
#include "engine/renderer/scene_renderer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <ImGuizmo.h>
#include <algorithm>
#include <format>
#include <ctime>

namespace editor {
// ============================================================================
// EditorApp Implementation
// ============================================================================

EditorApp::EditorApp() : m_active_scene("DefaultEditorScene") {
}

EditorApp::~EditorApp() {
    shutdown();
}

EditorApp& EditorApp::instance() {
    static EditorApp s_instance;
    return s_instance;
}

void EditorApp::apply_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Dark Modern Theme (Obsidian, Slate & Precision Highlights)
    colors[ImGuiCol_Text]                  = ImVec4(0.93f, 0.94f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.53f, 0.58f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.13f, 0.14f, 0.17f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.20f, 0.22f, 0.26f, 0.70f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.15f, 0.17f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.30f, 0.36f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.13f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.08f, 0.09f, 0.11f, 0.80f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.09f, 0.10f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.24f, 0.27f, 0.32f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.32f, 0.36f, 0.42f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.40f, 0.45f, 0.52f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.38f, 0.68f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.38f, 0.68f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.48f, 0.78f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.18f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.26f, 0.31f, 0.39f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.32f, 0.40f, 0.52f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.18f, 0.21f, 0.26f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.24f, 0.29f, 0.36f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.30f, 0.37f, 0.46f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.22f, 0.26f, 0.70f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.35f, 0.65f, 0.95f, 0.78f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.35f, 0.65f, 0.95f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.20f, 0.22f, 0.26f, 0.40f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.35f, 0.65f, 0.95f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.35f, 0.65f, 0.95f, 0.95f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.12f, 0.14f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.23f, 0.27f, 0.34f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.19f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.10f, 0.11f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_DockingPreview]        = ImVec4(0.35f, 0.65f, 0.95f, 0.35f);
    colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.38f, 0.68f, 0.98f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.35f, 0.75f, 0.55f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.45f, 0.85f, 0.65f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.22f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 4.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.IndentSpacing     = 18.0f;
}

bool EditorApp::init(const EditorAppDesc& desc) {
    if (m_initialized) return true;
    m_desc = desc;

    // Attach Editor Console Log Sink
    m_console_sink = std::make_shared<EditorConsoleSink>(2000);
    engine::core::Logger::instance().add_sink(m_console_sink);
    m_console_panel.set_sink(m_console_sink);

    LOG_INFO("Editor", "==================================================");
    LOG_INFO("Editor", "   Initializing Modern Game Engine Standalone Editor ");
    LOG_INFO("Editor", "==================================================");

    // 1. Core Subsystems
    if (!engine::core::Platform::init()) return false;
    if (!engine::jobs::JobSystem::instance().init()) return false;
    if (!engine::assets::AssetManager::instance().init()) return false;
    if (!engine::input::InputManager::instance().init()) return false;

    // 2. Physics, Audio & Scripting
    if (!engine::physics::PhysicsSystem::instance().init()) return false;
    if (!engine::audio::AudioEngine::instance().init()) return false;
    if (!engine::scripting::ScriptEngine::instance().init()) return false;

    // 3. Editor Preferences
    m_preferences.load_from_file(".engine/editor_preferences.toml");
    m_camera_speed = m_preferences.camera_speed;
    m_use_snap = m_preferences.snap_enabled;

    std::string project_dir = m_desc.project_directory;
    if (project_dir.empty() && !m_preferences.last_project_path.empty()) {
        project_dir = m_preferences.last_project_path;
    }

    // 4. Window & Vulkan 1.3 Context
    engine::core::WindowDesc win_desc{
        .title = m_desc.title + (!m_desc.project_name.empty() ? " - " + m_desc.project_name : ""),
        .width = m_desc.width,
        .height = m_desc.height,
        .resizable = true,
        .vulkan_compatible = true
    };

    if (!m_window.create(win_desc)) {
        LOG_FATAL("Editor", "Failed to create editor window!");
        return false;
    }

    if (!engine::rhi::RhiContext::instance().init(m_window, m_desc.enable_validation)) {
        LOG_FATAL("Editor", "Failed to initialize Vulkan 1.3 RHI!");
        return false;
    }

    if (!engine::rhi::BindlessHeap::instance().init()) {
        LOG_FATAL("Editor", "Failed to initialize Bindless Heap!");
        return false;
    }

    // 5. Swapchain & Command Synchronization
    if (!m_swapchain.init(m_window.get_width(), m_window.get_height(), m_desc.vsync)) {
        LOG_FATAL("Editor", "Failed to create RHI swapchain!");
        return false;
    }

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

    // 6. Graphics Pipelines
    m_vert_shader.init_from_spirv(engine::shaders::TRIANGLE_VERT_SPV, engine::shaders::TRIANGLE_VERT_SPV_SIZE, engine::rhi::ShaderStage::Vertex);
    m_frag_shader.init_from_spirv(engine::shaders::TRIANGLE_FRAG_SPV, engine::shaders::TRIANGLE_FRAG_SPV_SIZE, engine::rhi::ShaderStage::Fragment);

    engine::rhi::GraphicsPipelineDesc pipeline_desc{};
    pipeline_desc.vertex_shader = &m_vert_shader;
    pipeline_desc.fragment_shader = &m_frag_shader;
    pipeline_desc.color_formats = { engine::rhi::Format::R8G8B8A8_UNORM };
    pipeline_desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (!m_scene_pipeline.init(pipeline_desc)) {
        LOG_FATAL("Editor", "Failed to create scene graphics pipeline!");
        return false;
    }

    // 7. Initialize ImGui Context with Docking
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    apply_theme();

    // Create ImGui Vulkan Descriptor Pool
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100 }
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    VkDevice device = engine::rhi::RhiContext::instance().get_device();
    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &m_imgui_descriptor_pool) != VK_SUCCESS) {
        LOG_FATAL("Editor", "Failed to create ImGui descriptor pool!");
        return false;
    }

    // Initialize SDL3 Backend
    ImGui_ImplSDL3_InitForVulkan(m_window.get_sdl_window());

    engine::core::Platform::set_raw_event_callback([](const void* sdl_event) {
        ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(sdl_event));
    });

    // Initialize Vulkan Backend with Dynamic Rendering
    VkFormat vk_color_fmt = static_cast<VkFormat>(m_swapchain.get_format());

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = engine::rhi::RhiContext::instance().get_instance();
    init_info.PhysicalDevice = engine::rhi::RhiContext::instance().get_physical_device();
    init_info.Device = device;
    init_info.QueueFamily = engine::rhi::RhiContext::instance().get_queue_families().graphics_family;
    init_info.Queue = engine::rhi::RhiContext::instance().get_graphics_queue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_imgui_descriptor_pool;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &vk_color_fmt;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        LOG_FATAL("Editor", "Failed to initialize ImGui Vulkan backend!");
        return false;
    }

    // 7. Initialize 3D Scene Mesh Renderer
    if (!engine::renderer::SceneRenderer::instance().init(engine::rhi::Format::R8G8B8A8_UNORM, engine::rhi::Format::R16G16B16A16_SFLOAT, engine::rhi::Format::D32_SFLOAT)) {
        LOG_FATAL("Editor", "Failed to initialize SceneRenderer!");
        return false;
    }

    // 8. Register Scene Subsystems
    engine::physics::PhysicsSystem::instance().register_scene(m_active_scene);
    engine::audio::AudioEngine::instance().register_scene(m_active_scene);
    engine::scripting::ScriptEngine::instance().register_scene(m_active_scene);

    // Initialize initial offscreen viewport render target
    create_or_resize_viewport_framebuffer(1280, 720);

    // Load Project Startup Scene ONLY if explicitly passed via command line (--project <path>)
    // When opened directly, start with a clean empty state and show the Project Hub
    if (!m_desc.project_path.empty() && std::filesystem::exists(m_desc.project_path)) {
        open_project(m_desc.project_path);
    } else {
        m_active_scene.clear();
        m_selection_context.clear();
        m_command_history.clear();
        m_current_scene_path = "";
        m_show_project_hub = true;
    }

    m_initialized = true;
    m_running = true;

    LOG_INFO("Editor", "Modern Game Engine Editor Shell Initialized Successfully!");
    return true;
}

bool EditorApp::is_running() const {
    return m_running && m_window.is_open();
}

void EditorApp::request_exit() {
    m_running = false;
    m_window.set_should_close(true);
}

EditorMode EditorApp::get_mode() const {
    return m_state_manager.get_mode();
}

void EditorApp::set_mode(EditorMode mode) {
    if (m_state_manager.get_mode() == mode) return;

    LOG_INFO("Editor", "Editor Mode Transition: {} -> {}", 
             static_cast<int>(m_state_manager.get_mode()), static_cast<int>(mode));

    if (mode == EditorMode::Play) {
        m_state_manager.start_play(m_active_scene, m_selection_context);
    } else if (mode == EditorMode::Simulate) {
        m_state_manager.start_simulate(m_active_scene, m_selection_context);
    } else if (mode == EditorMode::Paused) {
        m_state_manager.pause();
    } else if (mode == EditorMode::Edit) {
        m_state_manager.stop(m_active_scene, m_selection_context);
    }
}

void EditorApp::create_primitive_entity(std::string_view type) {
    using namespace engine::core;
    using namespace engine::scene;
    using namespace engine::physics;
    using namespace engine::audio;
    using namespace engine::scripting;

    Entity entity;

    if (type == "Cube") {
        entity = m_active_scene.create_entity("Cube");
        entity.set<TransformComponent>(TransformComponent{ 
            .position = Vec3(0.0f, 1.0f, 0.0f),
            .scale = Vec3(1.0f, 1.0f, 1.0f)
        });
        entity.set<MeshRendererComponent>(MeshRendererComponent{ .is_visible = true });
        entity.set<MaterialComponent>(MaterialComponent{ 
            .base_color = Vec4(0.95f, 0.55f, 0.20f, 1.0f), // Warm Orange
            .roughness = 0.5f 
        });
        entity.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Dynamic, .mass = 1.0f });
        entity.set<ColliderComponent>(ColliderComponent{ 
            .shape_type = ColliderShapeType::Box,
            .box_half_extents = Vec3(0.5f, 0.5f, 0.5f)
        });
    } else if (type == "Sphere") {
        entity = m_active_scene.create_entity("Sphere");
        entity.set<TransformComponent>(TransformComponent{ 
            .position = Vec3(0.0f, 1.0f, 0.0f),
            .scale = Vec3(1.0f, 1.0f, 1.0f)
        });
        entity.set<MeshRendererComponent>(MeshRendererComponent{ .is_visible = true });
        entity.set<MaterialComponent>(MaterialComponent{ 
            .base_color = Vec4(0.90f, 0.25f, 0.35f, 1.0f), // Crimson Red
            .roughness = 0.3f 
        });
        entity.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Dynamic, .mass = 1.0f });
        entity.set<ColliderComponent>(ColliderComponent{ 
            .shape_type = ColliderShapeType::Sphere,
            .radius = 0.5f
        });
    } else if (type == "GroundPlane" || type == "Plane") {
        entity = m_active_scene.create_entity("GroundPlane");
        entity.set<TransformComponent>(TransformComponent{ 
            .position = Vec3(0.0f, -0.05f, 0.0f),
            .scale = Vec3(20.0f, 0.1f, 20.0f)
        });
        entity.set<MeshRendererComponent>(MeshRendererComponent{ .is_visible = true });
        entity.set<MaterialComponent>(MaterialComponent{ 
            .base_color = Vec4(0.25f, 0.55f, 0.35f, 1.0f), // Forest Green
            .roughness = 0.8f 
        });
        entity.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Static });
        entity.set<ColliderComponent>(ColliderComponent{ 
            .shape_type = ColliderShapeType::Box,
            .box_half_extents = Vec3(10.0f, 0.05f, 10.0f)
        });
    } else if (type == "Capsule" || type == "Cylinder") {
        entity = m_active_scene.create_entity("Capsule");
        entity.set<TransformComponent>(TransformComponent{ 
            .position = Vec3(0.0f, 1.0f, 0.0f),
            .scale = Vec3(1.0f, 1.0f, 1.0f)
        });
        entity.set<MeshRendererComponent>(MeshRendererComponent{ .is_visible = true });
        entity.set<MaterialComponent>(MaterialComponent{ 
            .base_color = Vec4(0.35f, 0.65f, 0.85f, 1.0f), // Slate Blue
            .roughness = 0.4f 
        });
        entity.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Dynamic, .mass = 1.0f });
        entity.set<ColliderComponent>(ColliderComponent{ 
            .shape_type = ColliderShapeType::Capsule,
            .radius = 0.4f,
            .half_height = 0.5f
        });
    } else if (type == "DirectionalLight") {
        entity = m_active_scene.create_entity("SunLight");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(20.0f, 50.0f, -20.0f) });
        entity.set<DirectionalLightComponent>(DirectionalLightComponent{
            .color = Vec3(1.0f, 0.98f, 0.92f),
            .intensity = 1.5f,
            .cast_shadows = true
        });
    } else if (type == "PointLight") {
        entity = m_active_scene.create_entity("PointLight");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 3.0f, 0.0f) });
        entity.set<PointLightComponent>(PointLightComponent{
            .color = Vec3(0.4f, 0.8f, 1.0f),
            .intensity = 5.0f,
            .radius = 12.0f
        });
    } else if (type == "Camera") {
        entity = m_active_scene.create_entity("MainCamera");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 2.0f, -6.0f) });
        entity.set<CameraComponent>(CameraComponent{ .fov_deg = 60.0f, .is_primary = true });
    } else if (type == "PlayerController") {
        entity = m_active_scene.create_entity("PlayerController");
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 1.5f, 0.0f) });
        entity.set<MeshRendererComponent>(MeshRendererComponent{ .is_visible = true });
        entity.set<MaterialComponent>(MaterialComponent{ 
            .base_color = Vec4(0.20f, 0.60f, 1.0f, 1.0f), // Player Cyan
            .roughness = 0.35f 
        });
        entity.set<AudioSourceComponent>(AudioSourceComponent{ .sound_name = "sfx_footstep.wav", .volume = 0.8f });
        entity.set<ScriptComponent>(ScriptComponent{ .script_path = "/scripts/player_controller.lua", .class_name = "PlayerController" });
        entity.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Dynamic, .mass = 70.0f });
        entity.set<ColliderComponent>(ColliderComponent{ .shape_type = ColliderShapeType::Capsule, .radius = 0.4f, .half_height = 0.5f });
    } else {
        entity = m_active_scene.create_entity(type);
        entity.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 0.0f, 0.0f) });
    }

    if (entity.is_valid()) {
        m_selection_context.select(entity.get_raw(), false);
        m_command_history.push_executed_command(std::make_unique<EntityCreateCommand>(
            m_active_scene, EntitySnapshot::capture(entity.get_raw(), m_active_scene)
        ));
        LOG_INFO("Editor", "Created entity '{}' ({})", entity.get_name(), type);
    }
}

bool EditorApp::open_project(const std::string& project_dir) {
    if (project_dir.empty()) return false;

    // 1. Close current project
    engine::project::ProjectManager::instance().close_project();

    // 2. Resolve and load project manifest
    std::filesystem::path p(project_dir);
    bool loaded = false;
    if (std::filesystem::exists(p / "project.toml")) {
        loaded = engine::project::ProjectManager::instance().load_project((p / "project.toml").string());
    } else if (std::filesystem::exists(p) && p.extension() == ".toml") {
        loaded = engine::project::ProjectManager::instance().load_project(p.string());
    } else {
        LOG_WARN("Editor", "Could not find project.toml at '{}'", project_dir);
        return false;
    }

    if (!loaded) {
        LOG_ERROR("Editor", "Failed to load project at '{}'", project_dir);
        return false;
    }

    const auto& proj = engine::project::ProjectManager::instance().get_active_project();
    m_desc.project_name = proj.name;
    m_desc.project_directory = project_dir;

    // 3. Update Preferences
    m_preferences.add_recent_project(proj.name, project_dir);
    m_preferences.save_to_file(".engine/editor_preferences.toml");

    // 4. Update Window Title
    m_window.set_title(m_desc.title + " - " + proj.name);

    // 5. Load Project Startup Map
    if (!proj.default_map.empty() && (engine::vfs::VFS::instance().file_exists(proj.default_map) || std::filesystem::exists(proj.default_map))) {
        open_scene(proj.default_map);
    } else {
        new_scene();
    }

    LOG_INFO("Editor", "Successfully opened project '{}' ({})", proj.name, project_dir);
    return true;
}

void EditorApp::new_scene() {
    if (!m_state_manager.is_editing()) {
        set_mode(EditorMode::Edit);
    }
    m_active_scene.clear();
    m_selection_context.clear();
    m_command_history.clear();
    m_current_scene_path = "/maps/untitled.map";

    // Create default environment entities for authoring
    create_primitive_entity("DirectionalLight");
    create_primitive_entity("Camera");

    LOG_INFO("Editor", "Created new empty scene with default lighting");
}

void EditorApp::open_scene(const std::string& path) {
    if (!m_state_manager.is_editing()) {
        set_mode(EditorMode::Edit);
    }
    m_selection_context.clear();
    m_command_history.clear();

    if (engine::scene::MapSerializer::load_map(path, m_active_scene)) {
        m_current_scene_path = path;
        LOG_INFO("Editor", "Opened and deserialized scene: {}", path);
    } else {
        LOG_WARN("Editor", "Could not open scene from path '{}'", path);
    }
}

void EditorApp::save_scene() {
    if (m_current_scene_path.empty() || m_current_scene_path == "maps/untitled.map") {
        if (engine::project::ProjectManager::instance().is_project_loaded()) {
            const auto& proj = engine::project::ProjectManager::instance().get_active_project();
            save_scene_as(proj.default_map.empty() ? "maps/default.map" : proj.default_map);
        } else {
            save_scene_as("maps/untitled.map");
        }
        return;
    }
    if (engine::scene::MapSerializer::save_map(m_active_scene, m_current_scene_path)) {
        LOG_INFO("Editor", "Saved scene to {}", m_current_scene_path);
    } else {
        LOG_ERROR("Editor", "Failed to save scene to {}", m_current_scene_path);
    }
}

void EditorApp::save_scene_as(const std::string& path) {
    m_current_scene_path = path;
    if (engine::scene::MapSerializer::save_map(m_active_scene, m_current_scene_path)) {
        LOG_INFO("Editor", "Saved scene as {}", path);
    } else {
        LOG_ERROR("Editor", "Failed to save scene as {}", path);
    }
}

void EditorApp::create_or_resize_viewport_framebuffer(uint32_t width, uint32_t height) {
    width = std::max(width, 16u);
    height = std::max(height, 16u);

    if (m_viewport_texture.is_valid() && m_viewport_width == width && m_viewport_height == height) {
        return;
    }

    engine::rhi::RhiContext::instance().wait_idle();

    m_viewport_texture.destroy();

    if (m_viewport_sampler.get_handle() == VK_NULL_HANDLE) {
        m_viewport_sampler.init(engine::rhi::SamplerDesc{
            .min_filter = engine::rhi::SamplerFilter::Linear,
            .mag_filter = engine::rhi::SamplerFilter::Linear,
            .address_u = engine::rhi::SamplerAddressMode::ClampToEdge,
            .address_v = engine::rhi::SamplerAddressMode::ClampToEdge,
            .address_w = engine::rhi::SamplerAddressMode::ClampToEdge,
            .debug_name = "ViewportSampler"
        });
    }

    m_viewport_texture.init(engine::rhi::TextureDesc{
        .width = width,
        .height = height,
        .format = engine::rhi::Format::R8G8B8A8_UNORM,
        .usage = engine::rhi::TextureUsage::ColorAttachment | engine::rhi::TextureUsage::Sampled,
        .debug_name = "EditorViewportRT"
    });

    m_viewport_width = width;
    m_viewport_height = height;

    m_viewport.set_texture(
        m_viewport_sampler.get_handle(),
        m_viewport_texture.get_view(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    LOG_INFO("Editor", "Allocated Viewport Offscreen Framebuffer ({}x{})", width, height);
}

float EditorApp::get_toolbar_height() const {
    const ImGuiStyle& style = ImGui::GetStyle();
    return ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f + 6.0f;
}

float EditorApp::get_statusbar_height() const {
    const ImGuiStyle& style = ImGui::GetStyle();
    return ImGui::GetFrameHeight() + style.WindowPadding.y * 2.0f;
}

void EditorApp::setup_dockspace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    // Dynamically calculate work area boundaries from active ImGui font & frame metrics
    float toolbar_height = get_toolbar_height();
    float statusbar_height = get_statusbar_height();

    ImVec2 dockspace_pos = ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + toolbar_height);
    ImVec2 dockspace_size = ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - toolbar_height - statusbar_height);

    ImGui::SetNextWindowPos(dockspace_pos);
    ImGui::SetNextWindowSize(dockspace_size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                  ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("##EditorDockSpaceHost", nullptr, host_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("EditorDockSpace");
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    // Check if layout needs to be built or reset
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr || m_reset_layout) {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, dockspace_size);

        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.28f, nullptr, &dock_main_id);
        ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.22f, nullptr, &dock_main_id);
        ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.26f, nullptr, &dock_main_id);

        // Split left into Outliner (top) and Content Browser (bottom)
        ImGuiID dock_id_left_bottom = ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Down, 0.45f, nullptr, &dock_id_left);
        ImGuiID dock_id_left_top = dock_id_left;

        ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
        ImGui::DockBuilderDockWindow("Outliner", dock_id_left_top);
        ImGui::DockBuilderDockWindow("Content Browser", dock_id_left_bottom);
        ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
        ImGui::DockBuilderDockWindow("Environment & Skybox", dock_id_right);
        ImGui::DockBuilderDockWindow("Project Settings", dock_id_right);
        ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
        ImGui::DockBuilderDockWindow("Profiler", dock_id_bottom);

        ImGui::DockBuilderFinish(dockspace_id);
        m_reset_layout = false;
        LOG_INFO("Editor", "DockSpace Layout generated successfully");
    }

    ImGui::End();
}

void EditorApp::render_main_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        // --- FILE MENU ---
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                new_scene();
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                if (engine::project::ProjectManager::instance().is_project_loaded()) {
                    const auto& proj = engine::project::ProjectManager::instance().get_active_project();
                    save_scene_as(proj.default_map.empty() ? "maps/default.map" : proj.default_map);
                } else {
                    save_scene_as("maps/untitled.map");
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Project Hub...", "Ctrl+Shift+P")) {
                m_show_project_hub = true;
            }
            if (ImGui::MenuItem("Project Settings...", nullptr, false, engine::project::ProjectManager::instance().is_project_loaded())) {
                m_show_project_settings = true;
            }
            if (ImGui::MenuItem("Package Project...", "Ctrl+Shift+B", false, engine::project::ProjectManager::instance().is_project_loaded())) {
                m_show_packaging_dialog = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                request_exit();
            }
            ImGui::EndMenu();
        }

        // --- EDIT MENU ---
        if (ImGui::BeginMenu("Edit")) {
            std::string undo_label = m_command_history.can_undo()
                ? std::format("Undo {}##Menu", m_command_history.get_undo_name())
                : "Undo##Menu";
            if (ImGui::MenuItem(undo_label.c_str(), "Ctrl+Z", false, m_command_history.can_undo())) {
                m_command_history.undo();
            }

            std::string redo_label = m_command_history.can_redo()
                ? std::format("Redo {}##Menu", m_command_history.get_redo_name())
                : "Redo##Menu";
            if (ImGui::MenuItem(redo_label.c_str(), "Ctrl+Y", false, m_command_history.can_redo())) {
                m_command_history.redo();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
                if (m_selection_context.has_selection()) {
                    auto primary = m_selection_context.get_primary();
                    if (primary.is_valid() && primary.is_alive()) {
                        std::string tag = primary.has<engine::scene::TagComponent>() 
                            ? primary.get<engine::scene::TagComponent>().name 
                            : primary.name().c_str();
                        auto copy = m_active_scene.create_entity(tag + "_Copy");
                        if (primary.has<engine::scene::TransformComponent>()) copy.set<engine::scene::TransformComponent>(primary.get<engine::scene::TransformComponent>());
                        if (primary.has<engine::scene::MeshRendererComponent>()) copy.set<engine::scene::MeshRendererComponent>(primary.get<engine::scene::MeshRendererComponent>());
                        if (primary.has<engine::scene::DirectionalLightComponent>()) copy.set<engine::scene::DirectionalLightComponent>(primary.get<engine::scene::DirectionalLightComponent>());
                        if (primary.has<engine::scene::PointLightComponent>()) copy.set<engine::scene::PointLightComponent>(primary.get<engine::scene::PointLightComponent>());
                        if (primary.has<engine::scene::CameraComponent>()) copy.set<engine::scene::CameraComponent>(primary.get<engine::scene::CameraComponent>());
                        if (primary.parent().is_valid()) copy.get_raw().child_of(primary.parent());
                        m_selection_context.select(copy.get_raw(), false);
                        m_command_history.push_executed_command(std::make_unique<EntityCreateCommand>(
                            m_active_scene, EntitySnapshot::capture(copy.get_raw(), m_active_scene)
                        ));
                    }
                }
            }
            if (ImGui::MenuItem("Delete", "Delete")) {
                if (m_selection_context.has_selection()) {
                    for (uint64_t eid : m_selection_context.get_all_selected()) {
                        auto e = m_active_scene.get_world().entity(eid);
                        if (e.is_valid() && e.is_alive()) {
                            m_command_history.execute_command(std::make_unique<EntityDeleteCommand>(m_active_scene, e));
                        }
                    }
                    m_selection_context.clear();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "Ctrl+A")) {
                m_active_scene.get_world().each([&](flecs::entity e, const engine::scene::TagComponent&) {
                    if (e.is_valid() && e.is_alive()) {
                        m_selection_context.select(e, true);
                    }
                });
            }
            if (ImGui::MenuItem("Deselect All", "Escape")) {
                m_selection_context.clear();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Editor Preferences...")) {
                m_show_preferences = true;
            }
            ImGui::EndMenu();
        }

        // --- GAMEOBJECT MENU ---
        if (ImGui::BeginMenu("GameObject")) {
            if (ImGui::MenuItem("Create Empty Entity")) {
                create_primitive_entity("EmptyEntity");
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("3D Objects")) {
                if (ImGui::MenuItem("Cube")) create_primitive_entity("Cube");
                if (ImGui::MenuItem("Sphere")) create_primitive_entity("Sphere");
                if (ImGui::MenuItem("Plane")) create_primitive_entity("GroundPlane");
                if (ImGui::MenuItem("Cylinder")) create_primitive_entity("Cylinder");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Lights")) {
                if (ImGui::MenuItem("Directional Light")) create_primitive_entity("DirectionalLight");
                if (ImGui::MenuItem("Point Light")) create_primitive_entity("PointLight");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Audio")) {
                if (ImGui::MenuItem("Audio Source")) create_primitive_entity("AudioSource");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Camera")) {
                if (ImGui::MenuItem("Perspective Camera")) create_primitive_entity("Camera");
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        // --- WINDOW MENU ---
        if (ImGui::BeginMenu("Window")) {
            ImGui::MenuItem("Viewport", nullptr, &m_show_viewport);
            ImGui::MenuItem("Outliner", nullptr, &m_show_outliner);
            ImGui::MenuItem("Inspector", nullptr, &m_show_inspector);
            ImGui::MenuItem("Content Browser", nullptr, &m_show_content_browser);
            ImGui::MenuItem("Console", nullptr, &m_show_console);
            ImGui::MenuItem("Profiler", nullptr, &m_show_profiler);
            ImGui::MenuItem("Environment & Skybox", nullptr, &m_show_environment);
            ImGui::MenuItem("Project Settings", nullptr, &m_show_project_settings);
            ImGui::MenuItem("Package Project", nullptr, &m_show_packaging_dialog);
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo Window", nullptr, &m_show_imgui_demo);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset to Default Layout")) {
                m_reset_layout = true;
            }
            ImGui::EndMenu();
        }

        // --- HELP MENU ---
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Roadmap & Architecture Specification")) {
                LOG_INFO("Editor", "See docs/EDITOR_ROADMAP.md for complete architecture details");
            }
            if (ImGui::MenuItem("Game Project Specification")) {
                LOG_INFO("Editor", "See docs/GAME_PROJECT_SPECIFICATION.md for standalone project details");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("About Modern Game Engine...")) {
                m_show_about_dialog = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void EditorApp::render_toolbar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float toolbar_height = get_toolbar_height();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, toolbar_height));
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));

    if (ImGui::Begin("##EditorToolbar", nullptr, flags)) {
        // --- Transform Gizmo Selectors ---
        const char* gizmo_icons[] = { " [Q] Select ", " [W] Translate ", " [E] Rotate ", " [R] Scale " };
        for (int i = 0; i < 4; ++i) {
            bool selected = false;
            if (i == 0 && m_current_gizmo_op == 0) selected = true;
            if (i == 1 && m_current_gizmo_op == ImGuizmo::TRANSLATE) selected = true;
            if (i == 2 && m_current_gizmo_op == ImGuizmo::ROTATE) selected = true;
            if (i == 3 && m_current_gizmo_op == ImGuizmo::SCALE) selected = true;

            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.44f, 0.74f, 1.0f));
            }

            if (ImGui::Button(gizmo_icons[i])) {
                if (i == 0) m_current_gizmo_op = static_cast<ImGuizmo::OPERATION>(0);
                else if (i == 1) m_current_gizmo_op = ImGuizmo::TRANSLATE;
                else if (i == 2) m_current_gizmo_op = ImGuizmo::ROTATE;
                else if (i == 3) m_current_gizmo_op = ImGuizmo::SCALE;
            }

            if (selected) {
                ImGui::PopStyleColor();
            }
            ImGui::SameLine();
        }

        ImGui::SameLine(0, 16.0f);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0, 16.0f);

        // Coordinate Mode: World / Local
        if (ImGui::Button(m_current_gizmo_mode == ImGuizmo::WORLD ? " World Space " : " Local Space ")) {
            m_current_gizmo_mode = (m_current_gizmo_mode == ImGuizmo::WORLD) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        }

        ImGui::SameLine();
        ImGui::Checkbox("Snap", &m_use_snap);

        // Center: Play / Pause / Stop / Step / Simulate Controls
        float avail_w = ImGui::GetContentRegionAvail().x;
        float center_group_w = 260.0f;
        ImGui::SameLine(ImGui::GetCursorPosX() + (avail_w - center_group_w) * 0.5f);

        // Play Button
        bool is_playing = m_state_manager.is_playing();
        if (is_playing) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.70f, 0.35f, 1.0f));
        }
        if (ImGui::Button(" > Play ")) {
            set_mode(is_playing ? EditorMode::Edit : EditorMode::Play);
        }
        if (is_playing) {
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();

        // Pause Button
        bool is_paused = m_state_manager.is_paused();
        if (is_paused) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.15f, 1.0f));
        }
        if (ImGui::Button(" || Pause ")) {
            if (is_paused) {
                m_state_manager.resume();
            } else {
                m_state_manager.pause();
            }
        }
        if (is_paused) {
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();

        // Stop Button
        if (ImGui::Button(" [] Stop ")) {
            set_mode(EditorMode::Edit);
        }

        ImGui::SameLine();

        // Step Frame Button
        if (ImGui::Button(" >| Step ")) {
            m_state_manager.step_frame(m_active_scene, 1.0f / 60.0f);
        }

        ImGui::SameLine();

        // Simulate Button
        bool is_simulating = m_state_manager.is_simulating();
        if (is_simulating) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.65f, 0.85f, 1.0f));
        }
        if (ImGui::Button(" * Simulate ")) {
            set_mode(is_simulating ? EditorMode::Edit : EditorMode::Simulate);
        }
        if (is_simulating) {
            ImGui::PopStyleColor();
        }

        // Right side: Camera Speed
        ImGui::SameLine(ImGui::GetWindowWidth() - 180.0f);
        ImGui::SetNextItemWidth(80.0f);
        ImGui::SliderFloat("Cam Spd", &m_camera_speed, 0.1f, 10.0f, "%.1fx");
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void EditorApp::render_status_bar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float statusbar_height = get_statusbar_height();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - statusbar_height));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, statusbar_height));
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 3.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.11f, 1.00f));

    if (ImGui::Begin("##EditorStatusBar", nullptr, flags)) {
        // 1. Left: Project & Map Info
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Project: %s", m_desc.project_name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Text("Map: %s", m_current_scene_path.c_str());

        // 2. Mode Badge
        ImGui::SameLine(0, 16.0f);
        ImVec4 mode_color = ImVec4(0.5f, 0.55f, 0.65f, 1.0f);
        const char* mode_text = "[EDIT MODE]";
        EditorMode current_mode = m_state_manager.get_mode();
        if (current_mode == EditorMode::Play) {
            mode_color = ImVec4(0.25f, 0.85f, 0.40f, 1.0f);
            mode_text = "[PLAYING]";
        } else if (current_mode == EditorMode::Paused) {
            mode_color = ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
            mode_text = "[PAUSED]";
        } else if (current_mode == EditorMode::Simulate) {
            mode_color = ImVec4(0.30f, 0.75f, 0.95f, 1.0f);
            mode_text = "[SIMULATING]";
        }
        ImGui::TextColored(mode_color, "%s", mode_text);

        // 3. Right: Stats & GPU
        uint32_t entity_count = static_cast<uint32_t>(m_active_scene.get_entity_count());
        uint32_t mesh_count = static_cast<uint32_t>(m_active_scene.get_world().count<engine::scene::MeshRendererComponent>());
        float right_width = 560.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - right_width);
        ImGui::Text("FPS: %u (%.2f ms) | Entities: %u | Meshes: %u | GPU: %s",
                    m_timer.fps(),
                    m_timer.delta_time() * 1000.0f,
                    entity_count,
                    mesh_count,
                    engine::rhi::RhiContext::instance().get_caps().device_name.c_str());
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void EditorApp::render_viewport_panel() {
    float dt = static_cast<float>(m_timer.delta_time());
    m_viewport.set_gizmo_operation(m_current_gizmo_op);
    m_viewport.set_gizmo_mode(m_current_gizmo_mode);
    m_viewport.set_snapping_enabled(m_use_snap);

    engine::core::Vec2 vp_size = m_viewport.get_size();
    uint32_t req_w = static_cast<uint32_t>(std::max(vp_size.x, 64.0f));
    uint32_t req_h = static_cast<uint32_t>(std::max(vp_size.y, 64.0f));

    if (req_w != m_viewport_width || req_h != m_viewport_height || !m_viewport_texture.is_valid()) {
        create_or_resize_viewport_framebuffer(req_w, req_h);
    }

    m_viewport.render(m_active_scene, m_selection_context, m_command_history, dt, &m_show_viewport);
}

void EditorApp::render_outliner_panel() {
    m_outliner_panel.render(m_active_scene, m_selection_context, m_command_history, &m_show_outliner);
}

void EditorApp::render_inspector_panel() {
    m_inspector_panel.render(m_active_scene, m_selection_context, &m_show_inspector);
}


void EditorApp::render_content_browser_panel() {
    m_content_browser.render(m_active_scene, m_selection_context, m_command_history, &m_show_content_browser);
}

void EditorApp::render_console_panel() {
    m_console_panel.render(&m_show_console);
}

void EditorApp::render_profiler_panel() {
    m_profiler_panel.render(m_active_scene, static_cast<float>(m_timer.delta_time()), &m_show_profiler);
}

void EditorApp::render_environment_panel() {
    if (ImGui::Begin("Environment", &m_show_environment)) {
        if (ImGui::CollapsingHeader("Skybox & Directional Sun", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat("Sun Intensity", &m_sun_intensity, 0.05f, 0.0f, 10.0f);
            ImGui::ColorEdit3("Sun Color", m_sun_color);
            ImGui::DragFloat3("Sun Direction", m_sun_direction, 0.02f, -1.0f, 1.0f);
        }

        if (ImGui::CollapsingHeader("Atmosphere & Volumetric Fog", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat("Fog Density", &m_fog_density, 0.001f, 0.0f, 0.1f, "%.4f");
            ImGui::ColorEdit3("Fog Color", m_fog_color);
        }

        if (ImGui::CollapsingHeader("Post-Processing & Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat("Exposure", &m_exposure, 0.05f, 0.1f, 5.0f);
            ImGui::DragFloat("Bloom Intensity", &m_bloom_intensity, 0.01f, 0.0f, 1.0f);
        }
    }
    ImGui::End();
}

void EditorApp::render_project_settings_dialog() {
    if (!m_show_project_settings) return;

    ImGui::SetNextWindowSize(ImVec2(520, 380), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Project Settings", &m_show_project_settings)) {
        auto& pm = engine::project::ProjectManager::instance();
        auto settings = pm.get_active_project();

        char name_buf[128];
        strncpy_s(name_buf, settings.name.c_str(), sizeof(name_buf));
        if (ImGui::InputText("Project Name", name_buf, sizeof(name_buf))) {
            settings.name = name_buf;
        }

        char map_buf[128];
        strncpy_s(map_buf, settings.default_map.c_str(), sizeof(map_buf));
        if (ImGui::InputText("Startup Map", map_buf, sizeof(map_buf))) {
            settings.default_map = map_buf;
        }

        ImGui::Separator();
        ImGui::Text("Display Settings");
        int res[2] = { static_cast<int>(settings.window_width), static_cast<int>(settings.window_height) };
        if (ImGui::DragInt2("Default Resolution", res, 10, 640, 3840)) {
            settings.window_width = static_cast<uint32_t>(res[0]);
            settings.window_height = static_cast<uint32_t>(res[1]);
        }
        ImGui::Checkbox("VSync", &settings.vsync);

        ImGui::Separator();
        ImGui::Text("Physics Settings");
        ImGui::DragFloat("Fixed Timestep (s)", &settings.fixed_timestep, 0.001f, 0.005f, 0.05f);

        ImGui::Spacing();
        if (ImGui::Button("Save Settings", ImVec2(120, 30))) {
            pm.save_project();
            LOG_INFO("Editor", "Project Settings saved to project.toml");
            m_show_project_settings = false;
        }
    }
    ImGui::End();
}

void EditorApp::render_preferences_dialog() {
    if (!m_show_preferences) return;

    ImGui::SetNextWindowSize(ImVec2(520, 380), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Editor Preferences", &m_show_preferences)) {
        if (ImGui::CollapsingHeader("Viewport Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::DragFloat("Camera Speed", &m_preferences.camera_speed, 0.1f, 0.1f, 50.0f)) {
                m_camera_speed = m_preferences.camera_speed;
            }
            ImGui::DragFloat("Field of View", &m_preferences.camera_fov, 0.5f, 30.0f, 120.0f);
            ImGui::DragFloat("Flycam Sensitivity", &m_preferences.flycam_sensitivity, 0.01f, 0.05f, 2.0f);
        }

        if (ImGui::CollapsingHeader("Snapping Defaults", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox("Enable Snapping", &m_preferences.snap_enabled)) {
                m_use_snap = m_preferences.snap_enabled;
            }
            ImGui::DragFloat("Translation Snap (m)", &m_preferences.snap_translation, 0.05f, 0.01f, 10.0f);
            ImGui::DragFloat("Rotation Snap (deg)", &m_preferences.snap_rotation, 1.0f, 1.0f, 90.0f);
            ImGui::DragFloat("Scale Snap", &m_preferences.snap_scale, 0.01f, 0.01f, 1.0f);
        }

        if (ImGui::CollapsingHeader("Autosave Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enable Autosave", &m_preferences.autosave_enabled);
            ImGui::DragFloat("Autosave Interval (s)", &m_preferences.autosave_interval_seconds, 10.0f, 30.0f, 1800.0f);
            int max_backups = static_cast<int>(m_preferences.max_autosaves);
            if (ImGui::DragInt("Max Backups", &max_backups, 1, 1, 20)) {
                m_preferences.max_autosaves = static_cast<uint32_t>(max_backups);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Save Preferences", ImVec2(140, 28))) {
            m_preferences.camera_speed = m_camera_speed;
            m_preferences.snap_enabled = m_use_snap;
            m_preferences.save_to_file(".engine/editor_preferences.toml");
            LOG_INFO("Editor", "Saved preferences to .engine/editor_preferences.toml");
            m_show_preferences = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(100, 28))) {
            m_show_preferences = false;
        }
    }
    ImGui::End();
}

void EditorApp::render_project_hub_dialog() {
    m_project_hub.render(m_preferences, &m_show_project_hub, [this](const std::string& proj_path) {
        open_project(proj_path);
    });
}

void EditorApp::render_packaging_dialog() {
    m_game_exporter.render_dialog(m_active_scene, &m_show_packaging_dialog);
}

void EditorApp::render_about_dialog() {
    if (!m_show_about_dialog) return;

    ImGui::SetNextWindowSize(ImVec2(480, 260), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("About Modern Game Engine", &m_show_about_dialog)) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Modern Game Engine Editor");
        ImGui::Text("Version: 0.1.0 (Milestone 1 - Editor Shell & DockSpace)");
        ImGui::Separator();
        ImGui::Text("Architecture & Stack:");
        ImGui::BulletText("Language: C++23 (Clang / MSVC)");
        ImGui::BulletText("Graphics: Vulkan 1.3 Dynamic Rendering & Bindless Heap");
        ImGui::BulletText("Windowing & Input: SDL3");
        ImGui::BulletText("ECS: Flecs 4.x");
        ImGui::BulletText("Physics: Jolt Physics");
        ImGui::BulletText("Audio: miniaudio");
        ImGui::BulletText("Scripting: Lua 5.4 + Sol2");
        ImGui::BulletText("UI: Dear ImGui Docking + ImGuizmo");
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(100, 28))) {
            m_show_about_dialog = false;
        }
    }
    ImGui::End();
}

void EditorApp::step() {
    if (!m_initialized || !is_running()) return;

    m_timer.tick();
    engine::input::InputManager::instance().new_frame();

    // 1. Poll Platform & Window Events
    m_events.clear();
    engine::core::Platform::poll_events(m_events);

    for (const auto& event : m_events) {
        engine::core::Platform::process_window_events(m_window, event);
        engine::input::InputManager::instance().process_event(event);

        if (event.type == engine::core::EventType::KeyDown) {
            if (event.key.ctrl && event.key.key == engine::core::KeyCode::Z && !ImGui::GetIO().WantTextInput) {
                if (event.key.shift) {
                    m_command_history.redo();
                } else {
                    m_command_history.undo();
                }
            } else if (event.key.ctrl && event.key.key == engine::core::KeyCode::Y && !ImGui::GetIO().WantTextInput) {
                m_command_history.redo();
            } else if (event.key.ctrl && event.key.key == engine::core::KeyCode::D && !ImGui::GetIO().WantTextInput) {
                if (m_selection_context.has_selection()) {
                    auto primary = m_selection_context.get_primary();
                    if (primary.is_valid() && primary.is_alive()) {
                        std::string tag = primary.has<engine::scene::TagComponent>() 
                            ? primary.get<engine::scene::TagComponent>().name 
                            : primary.name().c_str();
                        auto copy = m_active_scene.create_entity(tag + "_Copy");
                        if (primary.has<engine::scene::TransformComponent>()) copy.set<engine::scene::TransformComponent>(primary.get<engine::scene::TransformComponent>());
                        if (primary.has<engine::scene::MeshRendererComponent>()) copy.set<engine::scene::MeshRendererComponent>(primary.get<engine::scene::MeshRendererComponent>());
                        if (primary.has<engine::scene::DirectionalLightComponent>()) copy.set<engine::scene::DirectionalLightComponent>(primary.get<engine::scene::DirectionalLightComponent>());
                        if (primary.has<engine::scene::PointLightComponent>()) copy.set<engine::scene::PointLightComponent>(primary.get<engine::scene::PointLightComponent>());
                        if (primary.has<engine::scene::CameraComponent>()) copy.set<engine::scene::CameraComponent>(primary.get<engine::scene::CameraComponent>());
                        if (primary.parent().is_valid()) copy.get_raw().child_of(primary.parent());
                        m_selection_context.select(copy.get_raw(), false);
                        m_command_history.push_executed_command(std::make_unique<EntityCreateCommand>(
                            m_active_scene, EntitySnapshot::capture(copy.get_raw(), m_active_scene)
                        ));
                    }
                }
            } else if (event.key.key == engine::core::KeyCode::Delete && !ImGui::GetIO().WantTextInput) {
                if (m_selection_context.has_selection()) {
                    for (uint64_t eid : m_selection_context.get_all_selected()) {
                        auto e = m_active_scene.get_world().entity(eid);
                        if (e.is_valid() && e.is_alive()) {
                            m_command_history.execute_command(std::make_unique<EntityDeleteCommand>(m_active_scene, e));
                        }
                    }
                    m_selection_context.clear();
                }
            } else if (event.key.ctrl && event.key.shift && event.key.key == engine::core::KeyCode::B && !ImGui::GetIO().WantTextInput) {
                m_show_packaging_dialog = true;
            } else if (event.key.key == engine::core::KeyCode::Escape && m_selection_context.has_selection()) {
                m_selection_context.clear(); // Deselect entities
            } else if (event.key.key == engine::core::KeyCode::W && !ImGui::GetIO().WantTextInput) {
                m_current_gizmo_op = ImGuizmo::TRANSLATE;
            } else if (event.key.key == engine::core::KeyCode::E && !ImGui::GetIO().WantTextInput) {
                m_current_gizmo_op = ImGuizmo::ROTATE;
            } else if (event.key.key == engine::core::KeyCode::R && !ImGui::GetIO().WantTextInput) {
                m_current_gizmo_op = ImGuizmo::SCALE;
            } else if (event.key.key == engine::core::KeyCode::Q && !ImGui::GetIO().WantTextInput) {
                m_current_gizmo_op = static_cast<ImGuizmo::OPERATION>(0);
            } else if (event.key.key == engine::core::KeyCode::F && !ImGui::GetIO().WantTextInput) {
                m_viewport.focus_on_selection(m_selection_context);
            }
        } else if (event.type == engine::core::EventType::WindowResize) {
            m_swapchain.resize(event.window_resize.width, event.window_resize.height);
        }
    }

    float dt = static_cast<float>(m_timer.delta_time());

    // 2. Simulation Tick via EditorStateManager & Autosave
    m_state_manager.update(m_active_scene, dt);
    m_autosave_manager.update(m_active_scene, dt);
    m_active_scene.update_transforms();

    // 3. ImGui New Frame & DockSpace
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();

    setup_dockspace();
    render_main_menu_bar();
    render_toolbar();

    // Render Panels
    if (m_show_viewport) render_viewport_panel();
    if (m_show_outliner) render_outliner_panel();
    if (m_show_inspector) render_inspector_panel();
    if (m_show_content_browser) render_content_browser_panel();
    if (m_show_console) render_console_panel();
    if (m_show_profiler) render_profiler_panel();
    if (m_show_environment) render_environment_panel();
    if (m_show_project_settings) render_project_settings_dialog();
    if (m_show_preferences) render_preferences_dialog();
    if (m_show_project_hub) render_project_hub_dialog();
    if (m_show_packaging_dialog) render_packaging_dialog();
    if (m_show_about_dialog) render_about_dialog();
    if (m_show_imgui_demo) ImGui::ShowDemoWindow(&m_show_imgui_demo);

    render_status_bar();

    ImGui::Render();

    // 4. Vulkan Swapchain Acquisition & Command Submission
    m_in_flight_fences[m_current_frame].wait();

    uint32_t image_index = 0;
    VkResult acquire_res = m_swapchain.acquire_next_image(
        m_image_available_semaphores[m_current_frame].get_handle(),
        VK_NULL_HANDLE,
        image_index
    );

    if (acquire_res == VK_ERROR_OUT_OF_DATE_KHR) {
        m_swapchain.resize(m_window.get_width(), m_window.get_height());
        return;
    }

    m_in_flight_fences[m_current_frame].reset();

    // 5. Render Graph Execution
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

    if (m_viewport_texture.is_valid()) {
        engine::renderer::RGTextureHandle viewport_rg = m_render_graph.import_texture(
            "ViewportColorTarget",
            m_viewport_texture.get_handle(),
            m_viewport_texture.get_view(),
            m_viewport_width,
            m_viewport_height,
            engine::rhi::Format::R8G8B8A8_UNORM,
            VK_IMAGE_LAYOUT_UNDEFINED
        );

        float aspect = static_cast<float>(m_viewport_width) / static_cast<float>(std::max(m_viewport_height, 1u));
        engine::core::Mat4 view = m_viewport.get_camera().get_view_matrix();
        engine::core::Mat4 proj = m_viewport.get_camera().get_projection_matrix(aspect);
        engine::core::Mat4 view_proj = proj * view;

        engine::renderer::RenderCamera camera{};
        camera.view = view;
        camera.proj = proj;
        camera.view_proj = view_proj;
        camera.position = m_viewport.get_camera().get_position();
        camera.frustum = engine::core::Frustum::from_view_projection(view_proj);
        camera.aspect = aspect;

        engine::renderer::GraphicsSettings settings{};
        settings.enable_frustum_culling = true;
        settings.enable_vignette = true;
        settings.vignette_intensity = 0.2f;
        settings.tone_mapper = engine::renderer::ToneMapper::ACES;

        engine::renderer::SceneRenderer::instance().setup_render_pipeline(
            m_render_graph,
            m_active_scene,
            camera,
            settings,
            viewport_rg,
            m_viewport_width,
            m_viewport_height
        );

        // Transition Viewport texture to Shader Read for ImGui Sampling
        m_render_graph.add_pass(
            "ViewportTransitionPass",
            [&](engine::renderer::RenderPassBuilder& builder) {
                builder.read_texture(viewport_rg, engine::renderer::RGResourceAccess::ShaderRead);
            },
            [](engine::renderer::RenderPassContext&) {}
        );
    }

    // Pass 3: Editor UI Pass (Renders ImGui over the main window swapchain)
    m_render_graph.add_pass(
        "EditorUIPass",
        [&](engine::renderer::RenderPassBuilder& builder) {
            builder.set_color_attachment(
                0,
                swapchain_rg,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                engine::core::Vec4(0.08f, 0.09f, 0.11f, 1.0f)
            );
        },
        [](engine::renderer::RenderPassContext& ctx) {
            auto& cmd = ctx.get_command_buffer();
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd.get_handle());
        }
    );

    // Pass 4: Present Transition Pass
    m_render_graph.add_pass(
        "PresentTransitionPass",
        [&](engine::renderer::RenderPassBuilder& builder) {
            builder.read_texture(swapchain_rg, engine::renderer::RGResourceAccess::Present);
        },
        [](engine::renderer::RenderPassContext&) {}
    );

    auto& cmd = m_cmd_buffers[m_current_frame];
    cmd.reset();
    cmd.begin();

    m_render_graph.compile();
    m_render_graph.execute(cmd);

    cmd.end();

    // 6. Submit & Present
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

    VkResult present_res = m_swapchain.present(
        engine::rhi::RhiContext::instance().get_graphics_queue(),
        m_render_finished_semaphores[image_index].get_handle(),
        image_index
    );

    if (present_res == VK_ERROR_OUT_OF_DATE_KHR || present_res == VK_SUBOPTIMAL_KHR) {
        m_swapchain.resize(m_window.get_width(), m_window.get_height());
    }

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    m_rendered_frames++;
}

void EditorApp::run() {
    while (is_running()) {
        step();
    }
}

void EditorApp::shutdown() {
    if (!m_initialized) return;

    engine::rhi::RhiContext::instance().wait_idle();

    m_render_graph.destroy();
    m_scene_pipeline.destroy();
    m_vert_shader.destroy();
    m_frag_shader.destroy();

    engine::core::Platform::set_raw_event_callback(nullptr);
    m_viewport.destroy();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_imgui_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(engine::rhi::RhiContext::instance().get_device(), m_imgui_descriptor_pool, nullptr);
        m_imgui_descriptor_pool = VK_NULL_HANDLE;
    }

    for (auto& sem : m_render_finished_semaphores) {
        sem.destroy();
    }
    m_render_finished_semaphores.clear();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        m_in_flight_fences[i].destroy();
        m_image_available_semaphores[i].destroy();
        m_cmd_buffers[i].destroy(m_cmd_pool.get_handle());
    }
    m_viewport_texture.destroy();
    m_viewport_sampler.destroy();

    m_cmd_pool.destroy();
    m_swapchain.destroy();

    engine::renderer::SceneRenderer::instance().shutdown();
    engine::rhi::BindlessHeap::instance().shutdown();
    engine::rhi::RhiContext::instance().shutdown();

    m_window.destroy();
    m_preferences.camera_speed = m_camera_speed;
    m_preferences.snap_enabled = m_use_snap;
    m_preferences.save_to_file(".engine/editor_preferences.toml");

    engine::project::ProjectManager::instance().close_project();
    engine::scripting::ScriptEngine::instance().shutdown();
    engine::audio::AudioEngine::instance().shutdown();
    engine::physics::PhysicsSystem::instance().shutdown();
    engine::input::InputManager::instance().shutdown();
    engine::assets::AssetManager::instance().shutdown();
    engine::jobs::JobSystem::instance().shutdown();
    engine::core::Platform::shutdown();

    m_initialized = false;
    LOG_INFO("Editor", "Modern Game Engine Standalone Editor shutdown completed cleanly.");
}

} // namespace editor
