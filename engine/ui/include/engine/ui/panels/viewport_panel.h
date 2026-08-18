#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/rhi/rhi_texture.h"
#include <vulkan/vulkan.h>
#include <imgui.h>

namespace engine::ui {

class ViewportPanel {
public:
    ViewportPanel();
    ~ViewportPanel();

    void set_texture(VkSampler sampler, VkImageView image_view, VkImageLayout layout);
    void render();

    core::Vec2 get_size() const { return m_size; }
    bool is_hovered() const { return m_is_hovered; }
    bool is_focused() const { return m_is_focused; }

private:
    VkDescriptorSet m_texture_descriptor{VK_NULL_HANDLE};
    core::Vec2 m_size{1280.0f, 720.0f};
    bool m_is_hovered{false};
    bool m_is_focused{false};
};

} // namespace engine::ui
