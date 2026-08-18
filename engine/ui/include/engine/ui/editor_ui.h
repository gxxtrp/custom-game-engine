#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/core/platform.h"
#include "engine/rhi/rhi_context.h"
#include "engine/scene/scene.h"
#include <imgui.h>
#include <memory>

namespace engine::ui {

class HierarchyPanel;
class InspectorPanel;
class ContentBrowserPanel;
class ProfilerPanel;
class ViewportPanel;

class EditorUI {
public:
    static EditorUI& instance();

    bool init(core::Window& window, rhi::Format color_format);
    void shutdown();

    void process_event(const core::PlatformEvent& event);
    void begin_frame();
    void render_panels(scene::Scene& active_scene, float dt, uint32_t fps);
    void end_frame();
    void render(VkCommandBuffer cmd_buffer);

    void set_viewport_texture(VkSampler sampler, VkImageView image_view, VkImageLayout layout);

    bool is_initialized() const { return m_initialized; }

    uint64_t get_selected_entity() const { return m_selected_entity; }
    void set_selected_entity(uint64_t id) { m_selected_entity = id; }

    ViewportPanel* get_viewport_panel() { return m_viewport_panel.get(); }

private:
    EditorUI();
    ~EditorUI();

    void apply_theme();

    VkDescriptorPool m_descriptor_pool{VK_NULL_HANDLE};
    uint64_t m_selected_entity{0};

    std::unique_ptr<ViewportPanel> m_viewport_panel;
    std::unique_ptr<HierarchyPanel> m_hierarchy_panel;
    std::unique_ptr<InspectorPanel> m_inspector_panel;
    std::unique_ptr<ContentBrowserPanel> m_content_browser_panel;
    std::unique_ptr<ProfilerPanel> m_profiler_panel;

    bool m_initialized{false};
};

} // namespace engine::ui
