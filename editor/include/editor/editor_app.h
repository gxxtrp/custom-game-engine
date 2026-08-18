#pragma once

#include "engine/engine.h"
#include "editor/selection_context.h"
#include "editor/outliner_panel.h"
#include "editor/inspector_panel.h"
#include "editor/scene_viewport.h"
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

enum class EditorMode {
    Edit,
    Play,
    Simulate,
    Paused
};

struct EditorConsoleEntry {
    engine::core::LogLevel level{engine::core::LogLevel::Info};
    std::string category;
    std::string message;
    std::string file;
    uint32_t line{0};
    std::string timestamp;
};

class EditorConsoleSink : public engine::core::ILogSink {
public:
    explicit EditorConsoleSink(size_t max_entries = 2000);
    void log(const engine::core::LogMessage& message) override;
    void flush() override {}

    std::vector<EditorConsoleEntry> get_entries() const;
    void clear();

private:
    size_t m_max_entries;
    mutable std::mutex m_mutex;
    std::deque<EditorConsoleEntry> m_entries;
};

struct EditorAppDesc {
    std::string title{"Modern Game Engine Editor"};
    uint32_t width{1600};
    uint32_t height{900};
    bool vsync{true};
    bool enable_validation{true};
    std::string project_directory{"sandbox_project"};
    std::string project_name{"SandboxGame"};
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

    EditorMode get_mode() const { return m_mode; }
    void set_mode(EditorMode mode);

    engine::scene::Scene& get_active_scene() { return m_active_scene; }
    engine::core::Window& get_window() { return m_window; }
    SelectionContext& get_selection_context() { return m_selection_context; }
    OutlinerPanel& get_outliner_panel() { return m_outliner_panel; }
    InspectorPanel& get_inspector_panel() { return m_inspector_panel; }
    SceneViewport& get_viewport() { return m_viewport; }

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
    void render_about_dialog();

    // Scene & Project helpers
    void new_scene();
    void open_scene(const std::string& path);
    void save_scene();
    void save_scene_as(const std::string& path);
    void create_primitive_entity(std::string_view type);

    EditorAppDesc m_desc{};
    engine::core::Window m_window;
    engine::rhi::RhiSwapchain m_swapchain;

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

    EditorMode m_mode{EditorMode::Edit};
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
    bool m_show_about_dialog{false};
    bool m_show_imgui_demo{false};

    // Subsystem Panels & Context
    SelectionContext m_selection_context;
    OutlinerPanel m_outliner_panel;
    InspectorPanel m_inspector_panel;
    SceneViewport m_viewport;

    // Toolbar & Gizmo State
    ImGuizmo::OPERATION m_current_gizmo_op{ImGuizmo::TRANSLATE};
    ImGuizmo::MODE m_current_gizmo_mode{ImGuizmo::WORLD};
    bool m_use_snap{false};
    float m_camera_speed{1.0f};

    // Console Panel State
    bool m_console_show_info{true};
    bool m_console_show_warn{true};
    bool m_console_show_error{true};
    bool m_console_auto_scroll{true};
    char m_console_filter[128]{""};

    // Profiler State
    static constexpr size_t PROFILER_SAMPLE_COUNT = 120;
    float m_frame_time_history[PROFILER_SAMPLE_COUNT]{0.0f};
    size_t m_frame_time_offset{0};

    // Content Browser State
    std::string m_current_browser_directory{"assets"};

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
