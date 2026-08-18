#include "engine/ui/editor_ui.h"
#include "engine/ui/panels/viewport_panel.h"
#include "engine/ui/panels/hierarchy_panel.h"
#include "engine/ui/panels/inspector_panel.h"
#include "engine/ui/panels/content_browser_panel.h"
#include "engine/ui/panels/profiler_panel.h"
#include "engine/core/log.h"
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

namespace engine::ui {

EditorUI::EditorUI() = default;

EditorUI::~EditorUI() {
    shutdown();
}

EditorUI& EditorUI::instance() {
    static EditorUI s_instance;
    return s_instance;
}

void EditorUI::apply_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Dark Modern Theme (Obsidian & Slate)
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.93f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.52f, 0.55f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.12f, 0.13f, 0.16f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.20f, 0.22f, 0.25f, 0.60f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.16f, 0.17f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.24f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.29f, 0.34f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.12f, 0.14f, 0.17f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.08f, 0.09f, 0.10f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.09f, 0.10f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.25f, 0.27f, 0.32f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.32f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.40f, 0.44f, 0.50f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.35f, 0.65f, 0.95f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.35f, 0.65f, 0.95f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.45f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.25f, 0.30f, 0.38f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.32f, 0.40f, 0.52f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.18f, 0.21f, 0.26f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.24f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.30f, 0.36f, 0.45f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.22f, 0.25f, 0.32f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.18f, 0.21f, 0.27f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_DockingPreview]        = ImVec4(0.35f, 0.65f, 0.95f, 0.30f);
    colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);

    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 4.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.ItemSpacing       = ImVec2(8.0f, 6.0f);
    style.FramePadding      = ImVec2(6.0f, 4.0f);
}

bool EditorUI::init(core::Window& window, rhi::Format color_format) {
    if (m_initialized) return true;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    apply_theme();

    // 1. Create Descriptor Pool for ImGui
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

    VkDevice device = rhi::RhiContext::instance().get_device();
    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &m_descriptor_pool) != VK_SUCCESS) {
        LOG_FATAL("UI", "Failed to create ImGui descriptor pool!");
        return false;
    }

    // 2. Initialize SDL3 Backend
    ImGui_ImplSDL3_InitForVulkan(window.get_sdl_window());

    core::Platform::set_raw_event_callback([](const void* sdl_event) {
        ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(sdl_event));
    });

    // 3. Initialize Vulkan Backend with Dynamic Rendering
    VkFormat vk_color_fmt = static_cast<VkFormat>(color_format);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = rhi::RhiContext::instance().get_instance();
    init_info.PhysicalDevice = rhi::RhiContext::instance().get_physical_device();
    init_info.Device = device;
    init_info.QueueFamily = rhi::RhiContext::instance().get_queue_families().graphics_family;
    init_info.Queue = rhi::RhiContext::instance().get_graphics_queue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_descriptor_pool;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &vk_color_fmt;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        LOG_FATAL("UI", "Failed to initialize ImGui Vulkan backend!");
        return false;
    }

    m_viewport_panel = std::make_unique<ViewportPanel>();
    m_hierarchy_panel = std::make_unique<HierarchyPanel>();
    m_inspector_panel = std::make_unique<InspectorPanel>();
    m_content_browser_panel = std::make_unique<ContentBrowserPanel>();
    m_profiler_panel = std::make_unique<ProfilerPanel>();

    m_initialized = true;
    LOG_INFO("UI", "Initialized Modern ImGui Editor UI (Docking & Dynamic Rendering)");
    return true;
}

void EditorUI::set_viewport_texture(VkSampler sampler, VkImageView image_view, VkImageLayout layout) {
    if (m_viewport_panel) {
        m_viewport_panel->set_texture(sampler, image_view, layout);
    }
}

void EditorUI::shutdown() {
    if (!m_initialized) return;

    m_profiler_panel.reset();
    m_content_browser_panel.reset();
    m_inspector_panel.reset();
    m_hierarchy_panel.reset();
    m_viewport_panel.reset();

    core::Platform::set_raw_event_callback(nullptr);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(rhi::RhiContext::instance().get_device(), m_descriptor_pool, nullptr);
        m_descriptor_pool = VK_NULL_HANDLE;
    }

    m_initialized = false;
    LOG_INFO("UI", "ImGui Editor UI shutdown cleanly");
}

void EditorUI::process_event(const core::PlatformEvent& /*event*/) {
    // SDL3 events are processed automatically via SDL event polling hook or platform adapter
}

void EditorUI::begin_frame() {
    if (!m_initialized) return;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Editor Dockspace
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
}

void EditorUI::render_panels(scene::Scene& active_scene, float dt, uint32_t fps) {
    if (!m_initialized) return;

    // Main Menu Bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {}
            if (ImGui::MenuItem("Exit", "Alt+F4")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Preferences")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (m_viewport_panel) m_viewport_panel->render();
    if (m_hierarchy_panel) m_hierarchy_panel->render(active_scene);
    if (m_inspector_panel) m_inspector_panel->render(active_scene);
    if (m_content_browser_panel) m_content_browser_panel->render();
    if (m_profiler_panel) m_profiler_panel->render(dt, fps);
}

void EditorUI::end_frame() {
    if (!m_initialized) return;
    ImGui::Render();
}

void EditorUI::render(VkCommandBuffer cmd_buffer) {
    if (!m_initialized) return;
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buffer);
}

} // namespace engine::ui
