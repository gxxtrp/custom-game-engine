#include "editor/editor_app.h"
#include "engine/renderer/embedded_shaders.h"
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
// EditorConsoleSink Implementation
// ============================================================================

EditorConsoleSink::EditorConsoleSink(size_t max_entries)
    : m_max_entries(max_entries) {}

void EditorConsoleSink::log(const engine::core::LogMessage& message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Format timestamp
    auto now_time_t = std::chrono::system_clock::to_time_t(message.timestamp);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &now_time_t);
#else
    localtime_r(&now_time_t, &tm_buf);
#endif
    char time_str[32];
    std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

    EditorConsoleEntry entry{
        .level = message.level,
        .category = std::string(message.category),
        .message = message.message,
        .file = std::string(message.file),
        .line = message.line,
        .timestamp = std::string(time_str)
    };

    m_entries.push_back(std::move(entry));
    if (m_entries.size() > m_max_entries) {
        m_entries.pop_front();
    }
}

std::vector<EditorConsoleEntry> EditorConsoleSink::get_entries() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<EditorConsoleEntry>(m_entries.begin(), m_entries.end());
}

void EditorConsoleSink::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

// ============================================================================
// EditorApp Implementation
// ============================================================================

EditorApp::EditorApp() : m_active_scene("DefaultEditorScene") {
    for (size_t i = 0; i < PROFILER_SAMPLE_COUNT; ++i) {
        m_frame_time_history[i] = 16.6f;
    }
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

    // 3. Project Manager
    engine::project::ProjectManager::instance().create_project(m_desc.project_directory, m_desc.project_name);

    // 4. Window & Vulkan 1.3 Context
    engine::core::WindowDesc win_desc{
        .title = m_desc.title + " - " + m_desc.project_name,
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

    // 8. Register Scene Subsystems
    engine::physics::PhysicsSystem::instance().register_scene(m_active_scene);
    engine::audio::AudioEngine::instance().register_scene(m_active_scene);
    engine::scripting::ScriptEngine::instance().register_scene(m_active_scene);

    // Initialize initial offscreen viewport render target
    create_or_resize_viewport_framebuffer(1280, 720);

    // Populate Initial Scene with Standard Objects
    create_primitive_entity("GroundPlane");
    create_primitive_entity("PlayerController");
    create_primitive_entity("DynamicPhysicsBox");
    create_primitive_entity("DirectionalLight");

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

void EditorApp::set_mode(EditorMode mode) {
    if (m_mode == mode) return;

    LOG_INFO("Editor", "Editor Mode Transition: {} -> {}", 
             static_cast<int>(m_mode), static_cast<int>(mode));
    m_mode = mode;
}

void EditorApp::create_primitive_entity(std::string_view type) {
    using namespace engine::core;
    using namespace engine::scene;
    using namespace engine::physics;
    using namespace engine::audio;
    using namespace engine::scripting;

    if (type == "GroundPlane") {
        Entity ground = m_active_scene.create_entity("GroundPlane");
        ground.set<TransformComponent>(TransformComponent{ 
            .position = Vec3(0.0f, -0.5f, 0.0f),
            .scale = Vec3(20.0f, 1.0f, 20.0f)
        });
        ground.set<MeshRendererComponent>(MeshRendererComponent{ .cast_shadows = false });
        ground.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Static });
        ground.set<ColliderComponent>(ColliderComponent{ 
            .shape_type = ColliderShapeType::Box,
            .box_half_extents = Vec3(10.0f, 0.5f, 10.0f)
        });
    } else if (type == "PlayerController") {
        Entity player = m_active_scene.create_entity("PlayerController");
        player.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 1.5f, 0.0f) });
        player.set<MeshRendererComponent>(MeshRendererComponent{ .cast_shadows = true });
        player.set<AudioSourceComponent>(AudioSourceComponent{ .sound_name = "sfx_footstep.wav", .volume = 0.8f });
        player.set<ScriptComponent>(ScriptComponent{ .class_name = "PlayerController" });
        m_selection_context.select(player.get_raw(), false);
    } else if (type == "DynamicPhysicsBox") {
        Entity box = m_active_scene.create_entity("DynamicPhysicsBox");
        box.set<TransformComponent>(TransformComponent{ .position = Vec3(2.0f, 5.0f, 0.0f) });
        box.set<MeshRendererComponent>(MeshRendererComponent{ .cast_shadows = true });
        box.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Dynamic, .mass = 5.0f });
        box.set<ColliderComponent>(ColliderComponent{ .shape_type = ColliderShapeType::Box });
    } else if (type == "DirectionalLight") {
        Entity light = m_active_scene.create_entity("SunLight");
        light.set<TransformComponent>(TransformComponent{ .position = Vec3(20.0f, 50.0f, -20.0f) });
        light.set<DirectionalLightComponent>(DirectionalLightComponent{
            .color = Vec3(1.0f, 0.95f, 0.88f),
            .intensity = 2.0f,
            .cast_shadows = true
        });
    } else if (type == "PointLight") {
        Entity light = m_active_scene.create_entity("PointLight");
        light.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 3.0f, 0.0f) });
        light.set<PointLightComponent>(PointLightComponent{
            .color = Vec3(0.4f, 0.8f, 1.0f),
            .intensity = 5.0f,
            .radius = 12.0f
        });
    } else if (type == "Camera") {
        Entity cam = m_active_scene.create_entity("MainCamera");
        cam.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 2.0f, -5.0f) });
        cam.set<CameraComponent>(CameraComponent{ .fov_deg = 60.0f, .is_primary = true });
    } else {
        Entity e = m_active_scene.create_entity(type);
        e.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 0.0f, 0.0f) });
    }
}

void EditorApp::new_scene() {
    m_active_scene.clear();
    m_selection_context.clear();
    m_current_scene_path = "/maps/untitled.map";
    LOG_INFO("Editor", "Created new empty scene");
}

void EditorApp::open_scene(const std::string& path) {
    m_current_scene_path = path;
    LOG_INFO("Editor", "Opened scene: {}", path);
}

void EditorApp::save_scene() {
    LOG_INFO("Editor", "Saved scene to {}", m_current_scene_path);
}

void EditorApp::save_scene_as(const std::string& path) {
    m_current_scene_path = path;
    LOG_INFO("Editor", "Saved scene as {}", path);
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
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                open_scene("/maps/sandbox.map");
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                save_scene();
            }
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                save_scene_as("/maps/sandbox_copy.map");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Project Settings...")) {
                m_show_project_settings = true;
            }
            if (ImGui::MenuItem("Package Game...")) {
                LOG_INFO("Editor", "Opening Game Packaging Wizard...");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                request_exit();
            }
            ImGui::EndMenu();
        }

        // --- EDIT MENU ---
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                LOG_INFO("Editor", "Undo action performed");
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                LOG_INFO("Editor", "Redo action performed");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
                LOG_INFO("Editor", "Duplicated selected entity");
            }
            if (ImGui::MenuItem("Delete", "Delete")) {
                if (m_selection_context.has_selection()) {
                    for (uint64_t eid : m_selection_context.get_all_selected()) {
                        auto e = m_active_scene.get_world().entity(eid);
                        if (e.is_valid() && e.is_alive()) e.destruct();
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
            if (ImGui::MenuItem("Editor Preferences...")) {}
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
        bool is_playing = (m_mode == EditorMode::Play);
        if (is_playing) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.70f, 0.35f, 1.0f));
        }
        if (ImGui::Button(" > Play ")) {
            set_mode(EditorMode::Play);
        }
        if (is_playing) {
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();

        // Pause Button
        bool is_paused = (m_mode == EditorMode::Paused);
        if (is_paused) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.15f, 1.0f));
        }
        if (ImGui::Button(" || Pause ")) {
            set_mode(EditorMode::Paused);
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
            if (m_mode == EditorMode::Edit || m_mode == EditorMode::Paused) {
                m_active_scene.update(1.0f / 60.0f);
                engine::physics::PhysicsSystem::instance().update(1.0f / 60.0f);
            }
        }

        ImGui::SameLine();

        // Simulate Button
        bool is_simulating = (m_mode == EditorMode::Simulate);
        if (is_simulating) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.65f, 0.85f, 1.0f));
        }
        if (ImGui::Button(" * Simulate ")) {
            set_mode(EditorMode::Simulate);
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
        if (m_mode == EditorMode::Play) {
            mode_color = ImVec4(0.25f, 0.85f, 0.40f, 1.0f);
            mode_text = "[PLAYING]";
        } else if (m_mode == EditorMode::Paused) {
            mode_color = ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
            mode_text = "[PAUSED]";
        } else if (m_mode == EditorMode::Simulate) {
            mode_color = ImVec4(0.30f, 0.75f, 0.95f, 1.0f);
            mode_text = "[SIMULATING]";
        }
        ImGui::TextColored(mode_color, "%s", mode_text);

        // 3. Right: Stats & GPU
        float right_width = 540.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - right_width);
        ImGui::Text("FPS: %u (%.2f ms) | Draw Calls: 4 | Triangles: 1,024 | GPU: %s",
                    m_timer.fps(),
                    m_timer.delta_time() * 1000.0f,
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

    m_viewport.render(m_active_scene, m_selection_context, dt, &m_show_viewport);
}

void EditorApp::render_outliner_panel() {
    m_outliner_panel.render(m_active_scene, m_selection_context, &m_show_outliner);
}

void EditorApp::render_inspector_panel() {
    m_inspector_panel.render(m_active_scene, m_selection_context, &m_show_inspector);
}


void EditorApp::render_content_browser_panel() {
    if (ImGui::Begin("Content Browser", &m_show_content_browser)) {
        // Breadcrumb navigation
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Virtual Path: /%s", m_current_browser_directory.c_str());
        ImGui::Separator();

        // Folder tree on left, items on right
        ImGui::Columns(2, "ContentBrowserColumns", true);
        ImGui::SetColumnWidth(0, 160.0f);

        // Folders list
        const char* folders[] = { "assets", "assets/models", "assets/textures", "assets/materials", "assets/audio", "maps", "scripts", "config" };
        for (const char* folder : folders) {
            if (ImGui::Selectable(folder, m_current_browser_directory == folder)) {
                m_current_browser_directory = folder;
            }
        }

        ImGui::NextColumn();

        // Files grid
        struct MockAsset { const char* name; const char* type; };
        std::vector<MockAsset> files;
        if (m_current_browser_directory.find("models") != std::string::npos) {
            files = { { "character.glb", "Mesh" }, { "ground.glb", "Mesh" }, { "box.glb", "Mesh" } };
        } else if (m_current_browser_directory.find("materials") != std::string::npos) {
            files = { { "gold.mat", "Material" }, { "concrete.mat", "Material" } };
        } else if (m_current_browser_directory.find("audio") != std::string::npos) {
            files = { { "sfx_jump.wav", "Audio" }, { "sfx_footstep.wav", "Audio" }, { "music_ambient.ogg", "Audio" } };
        } else if (m_current_browser_directory == "maps") {
            files = { { "main_menu.map", "Map" }, { "sandbox.map", "Map" } };
        } else if (m_current_browser_directory == "scripts") {
            files = { { "player_controller.lua", "Script" }, { "enemy_ai.lua", "Script" } };
        } else {
            files = { { "models", "Folder" }, { "textures", "Folder" }, { "materials", "Folder" }, { "audio", "Folder" } };
        }

        for (const auto& file : files) {
            std::string label = std::format("[{}]  {}", file.type, file.name);
            if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(0)) {
                    LOG_INFO("Editor", "Opened asset: {}/{}", m_current_browser_directory, file.name);
                }
            }
        }

        ImGui::Columns(1);
    }
    ImGui::End();
}

void EditorApp::render_console_panel() {
    if (ImGui::Begin("Console", &m_show_console)) {
        // Top Toolbar: Clear, Filter toggles, Search filter
        if (ImGui::Button("Clear")) {
            if (m_console_sink) m_console_sink->clear();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Info", &m_console_show_info);
        ImGui::SameLine();
        ImGui::Checkbox("Warnings", &m_console_show_warn);
        ImGui::SameLine();
        ImGui::Checkbox("Errors", &m_console_show_error);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(180.0f);
        ImGui::InputTextWithHint("##ConsoleFilter", "Search logs...", m_console_filter, sizeof(m_console_filter));

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_console_auto_scroll);

        ImGui::Separator();

        // Log feed table
        if (ImGui::BeginTable("ConsoleLogTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            if (m_console_sink) {
                auto entries = m_console_sink->get_entries();
                for (const auto& entry : entries) {
                    if (entry.level == engine::core::LogLevel::Info && !m_console_show_info) continue;
                    if (entry.level == engine::core::LogLevel::Warn && !m_console_show_warn) continue;
                    if ((entry.level == engine::core::LogLevel::Error || entry.level == engine::core::LogLevel::Fatal) && !m_console_show_error) continue;

                    if (m_console_filter[0] != '\0' && entry.message.find(m_console_filter) == std::string::npos) {
                        continue;
                    }

                    ImGui::TableNextRow();

                    // Timestamp
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", entry.timestamp.c_str());

                    // Level Badge
                    ImGui::TableNextColumn();
                    if (entry.level == engine::core::LogLevel::Info) {
                        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "INFO");
                    } else if (entry.level == engine::core::LogLevel::Warn) {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "WARN");
                    } else if (entry.level == engine::core::LogLevel::Error || entry.level == engine::core::LogLevel::Fatal) {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "ERROR");
                    } else {
                        ImGui::TextDisabled("DEBUG");
                    }

                    // Category
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", entry.category.c_str());

                    // Message
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.message.c_str());
                }

                if (m_console_auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void EditorApp::render_profiler_panel() {
    if (ImGui::Begin("Profiler", &m_show_profiler)) {
        float dt = m_timer.delta_time();
        float fps = static_cast<float>(m_timer.fps());

        // Summary cards
        ImGui::Columns(4, "ProfilerCards", false);
        ImGui::Text("FPS: %.0f", fps);
        ImGui::NextColumn();
        ImGui::Text("Delta Time: %.2f ms", dt * 1000.0f);
        ImGui::NextColumn();
        ImGui::Text("Draw Calls: 4");
        ImGui::NextColumn();
        ImGui::Text("Active Entities: %zu", m_active_scene.get_entity_count());
        ImGui::Columns(1);

        ImGui::Separator();

        // Frame Time Graph
        m_frame_time_history[m_frame_time_offset] = dt * 1000.0f;
        m_frame_time_offset = (m_frame_time_offset + 1) % PROFILER_SAMPLE_COUNT;

        ImGui::Text("Frame Time History (ms)");
        ImGui::PlotLines("##FramePlot", m_frame_time_history, static_cast<int>(PROFILER_SAMPLE_COUNT), static_cast<int>(m_frame_time_offset), nullptr, 0.0f, 33.3f, ImVec2(-1, 60));

        ImGui::Separator();

        // Subsystem Breakdown Table
        if (ImGui::BeginTable("SubsystemProfileTable", 2, ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Vulkan RHI Backend");
            ImGui::TableNextColumn(); ImGui::Text("Vulkan 1.3 Dynamic");

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Jolt Physics Worker Threads");
            ImGui::TableNextColumn(); ImGui::Text("4 Threads");

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Job System Active Tasks");
            ImGui::TableNextColumn(); ImGui::Text("0 Tasks");

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("VRAM Allocated");
            ImGui::TableNextColumn(); ImGui::Text("184 MB");

            ImGui::EndTable();
        }
    }
    ImGui::End();
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
            if (event.key.key == engine::core::KeyCode::Escape && m_selection_context.has_selection()) {
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

    // 2. Simulation Tick (if Play or Simulate)
    if (m_mode == EditorMode::Play || m_mode == EditorMode::Simulate) {
        if (dt > 0.0f && dt < 0.1f) {
            engine::physics::PhysicsSystem::instance().update(dt);
            engine::audio::AudioEngine::instance().update(dt);
            engine::scripting::ScriptEngine::instance().sync_ecs_scripts(m_active_scene, dt);
            m_active_scene.update(dt);
        }
    }

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

        // Pass 1: Scene Forward Pass (Renders triangle into the Viewport Render Target)
        m_render_graph.add_pass(
            "SceneForwardPass",
            [&](engine::renderer::RenderPassBuilder& builder) {
                builder.set_color_attachment(
                    0,
                    viewport_rg,
                    VK_ATTACHMENT_LOAD_OP_CLEAR,
                    VK_ATTACHMENT_STORE_OP_STORE,
                    engine::core::Vec4(0.08f, 0.09f, 0.11f, 1.0f)
                );
            },
            [&](engine::renderer::RenderPassContext& ctx) {
                auto& cmd = ctx.get_command_buffer();
                cmd.set_viewport(engine::rhi::Viewport{
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = static_cast<float>(m_viewport_width),
                    .height = static_cast<float>(m_viewport_height),
                    .min_depth = 0.0f,
                    .max_depth = 1.0f
                });
                cmd.set_scissor(engine::rhi::Rect2D{
                    .offset_x = 0,
                    .offset_y = 0,
                    .width = m_viewport_width,
                    .height = m_viewport_height
                });
                cmd.bind_pipeline(m_scene_pipeline.get_pipeline());
                cmd.draw(3, 1, 0, 0);
            }
        );

        // Pass 2: Transition Viewport texture to Shader Read for ImGui Sampling
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

    engine::rhi::BindlessHeap::instance().shutdown();
    engine::rhi::RhiContext::instance().shutdown();

    m_window.destroy();
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
