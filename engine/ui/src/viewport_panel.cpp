#include "engine/ui/panels/viewport_panel.h"
#include <imgui_impl_vulkan.h>

namespace engine::ui {

ViewportPanel::ViewportPanel() = default;

ViewportPanel::~ViewportPanel() {
    // Descriptor sets created by ImGui_ImplVulkan_AddTexture are freed with the descriptor pool
}

void ViewportPanel::set_texture(VkSampler sampler, VkImageView image_view, VkImageLayout layout) {
    if (m_texture_descriptor != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(m_texture_descriptor);
        m_texture_descriptor = VK_NULL_HANDLE;
    }

    if (image_view != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE) {
        m_texture_descriptor = ImGui_ImplVulkan_AddTexture(sampler, image_view, layout);
    }
}

void ViewportPanel::render() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    m_is_focused = ImGui::IsWindowFocused();
    m_is_hovered = ImGui::IsWindowHovered();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x > 0.0f && avail.y > 0.0f) {
        m_size = { avail.x, avail.y };
    }

    if (m_texture_descriptor != VK_NULL_HANDLE) {
        ImGui::Image(reinterpret_cast<ImTextureID>(m_texture_descriptor), avail);
    } else {
        ImVec2 center = ImGui::GetCursorScreenPos();
        center.x += avail.x * 0.5f - 80.0f;
        center.y += avail.y * 0.5f - 10.0f;
        ImGui::SetCursorScreenPos(center);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[ 3D Scene Viewport ]");
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace engine::ui
