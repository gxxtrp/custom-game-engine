#pragma once

#include "engine/engine.h"
#include "editor/core/selection_context.h"
#include "editor/panels/outliner_panel.h"
#include "editor/panels/inspector_panel.h"
#include "editor/panels/scene_viewport.h"
#include "editor/core/editor_state.h"
#include "editor/core/command_history.h"
#include "editor/panels/content_browser.h"
#include "editor/assets/prefab_manager.h"
#include "editor/assets/asset_importer.h"
#include "editor/panels/console_panel.h"
#include "editor/panels/profiler_panel.h"
#include "editor/core/autosave_manager.h"
#include "editor/core/editor_preferences.h"
#include "editor/tools/game_exporter.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <deque>
#include <mutex>
#include <chrono>

namespace editor {

struct EditorAppDesc {
    std::string title{"Modern Game Engine Editor"};
    uint32_t width{1600};
    uint32_t height{900};
    bool vsync{true};
    bool enable_validation{true};
    std::string project_directory{""};
    std::string project_name{""};
};

class EditorApp {
public:
    static EditorApp& instance();

    bool init(const EditorAppDesc& desc = {});
    void run();
    void step();
    void shutdown();

    bool is_running() const;
    void request_exit();

    EditorMode get_mode() const;
    void set_mode(EditorMode mode);

    engine::scene::Scene& get_active_scene() { return m_active_scene; }
    engine::core::Window& get_window() { return m_window; }
    SelectionContext& get_selection_context() { return m_selection_context; }
    OutlinerPanel& get_outliner_panel() { return m_outliner_panel; }
    InspectorPanel& get_inspector_panel() { return m_inspector_panel; }
    SceneViewport& get_viewport() { return m_viewport; }
    EditorStateManager& get_state_manager() { return m_state_manager; }
    CommandHistory& get_command_history() { return m_command_history; }
    ContentBrowserPanel& get_content_browser() { return m_content_browser; }
    ConsolePanel& get_console_panel() { return m_console_panel; }
    ProfilerPanel& get_profiler_panel() { return m_profiler_panel; }
    AutosaveManager& get_autosave_manager() { return m_autosave_manager; }
    GameExporter& get_game_exporter() { return m_game_exporter; }
    EditorPreferences& get_preferences() { return m_preferences; }

private:
    EditorApp();
    ~EditorApp();

    void apply_theme();
    void setup_dockspace();
    void render_main_menu_bar();
    void render_toolbar();
    void render_status_bar();

    float get_toolbar_height() const;
    float get_statusbar_height() const;
    
    // Panel renderers
    void render_viewport_panel();
    void render_outliner_panel();
    void render_inspector_panel();
    void render_content_browser_panel();
    void render_console_panel();
    void render_profiler_panel();
    void render_environment_panel();
    void render_project_settings_dialog();
    void render_preferences_dialog();
    void render_packaging_dialog();
    void render_about_dialog();

    // Scene & Project helpers
    void new_scene();
    void open_scene(const std::string& path);
    void save_scene();
    void save_scene_as(const std::string& path);
    void create_primitive_entity(std::string_view type);
    void create_or_resize_viewport_framebuffer(uint32_t width, uint32_t height);

    EditorAppDesc m_desc{};
    engine::core::Window m_window;
    engine::rhi::RhiSwapchain m_swapchain;

    // Viewport Offscreen Render Target
    engine::rhi::RhiTexture m_viewport_texture;
    engine::rhi::RhiSampler m_viewport_sampler;
    uint32_t m_viewport_width{1280};
    uint32_t m_viewport_height{720};

    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
    engine::rhi::RhiCommandPool m_cmd_pool;
    engine::rhi::RhiCommandBuffer m_cmd_buffers[MAX_FRAMES_IN_FLIGHT];
    engine::rhi::RhiFence m_in_flight_fences[MAX_FRAMES_IN_FLIGHT];
    engine::rhi::RhiSemaphore m_image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    std::vector<engine::rhi::RhiSemaphore> m_render_finished_semaphores;

    engine::rhi::RhiShaderModule m_vert_shader;
    engine::rhi::RhiShaderModule m_frag_shader;
    engine::rhi::RhiGraphicsPipeline m_scene_pipeline;

    engine::renderer::RenderGraph m_render_graph;
    engine::scene::Scene m_active_scene;
    engine::core::FrameTimer m_timer;
    engine::core::DynamicArray<engine::core::PlatformEvent> m_events;

    VkDescriptorPool m_imgui_descriptor_pool{VK_NULL_HANDLE};
    std::shared_ptr<EditorConsoleSink> m_console_sink;

    uint32_t m_current_frame{0};
    uint32_t m_rendered_frames{0};
    bool m_initialized{false};
    bool m_running{false};
    bool m_reset_layout{false};
    std::string m_current_scene_path{"/maps/sandbox.map"};

    // Panel visibility toggles
    bool m_show_viewport{true};
    bool m_show_outliner{true};
    bool m_show_inspector{true};
    bool m_show_content_browser{true};
    bool m_show_console{true};
    bool m_show_profiler{true};
    bool m_show_environment{true};
    bool m_show_project_settings{false};
    bool m_show_preferences{false};
    bool m_show_packaging_dialog{false};
    bool m_show_about_dialog{false};
    bool m_show_imgui_demo{false};

    // Subsystem Panels & Context
    EditorStateManager m_state_manager;
    CommandHistory m_command_history;
    SelectionContext m_selection_context;
    OutlinerPanel m_outliner_panel;
    InspectorPanel m_inspector_panel;
    SceneViewport m_viewport;

    // Toolbar & Gizmo State
    ImGuizmo::OPERATION m_current_gizmo_op{ImGuizmo::TRANSLATE};
    ImGuizmo::MODE m_current_gizmo_mode{ImGuizmo::WORLD};
    bool m_use_snap{false};
    float m_camera_speed{1.0f};

    // Subsystem Panels
    ConsolePanel m_console_panel;
    ProfilerPanel m_profiler_panel;
    ContentBrowserPanel m_content_browser;
    AutosaveManager m_autosave_manager;
    GameExporter m_game_exporter;
    EditorPreferences m_preferences;

    // Environment Panel State
    float m_sun_intensity{1.5f};
    float m_sun_color[3]{1.0f, 0.98f, 0.92f};
    float m_sun_direction[3]{-0.5f, -1.0f, -0.3f};
    float m_fog_density{0.005f};
    float m_fog_color[3]{0.6f, 0.7f, 0.85f};
    float m_exposure{1.0f};
    float m_bloom_intensity{0.04f};
};

} // namespace editor
