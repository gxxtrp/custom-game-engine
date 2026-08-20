#pragma once

#include "engine/core/config.h"
#include "engine/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace engine::rhi {

class RhiSwapchain {
public:
    RhiSwapchain() = default;
    ~RhiSwapchain();

    bool init(uint32_t width, uint32_t height, bool vsync = true);
    void destroy();

    VkResult acquire_next_image(VkSemaphore signal_semaphore, VkFence signal_fence, uint32_t& out_image_index);
    VkResult present(VkQueue queue, VkSemaphore wait_semaphore, uint32_t image_index);
    bool resize(uint32_t width, uint32_t height);

    VkSwapchainKHR get_handle() const { return m_swapchain; }
    VkFormat get_format() const { return m_format.format; }
    VkExtent2D get_extent() const { return m_extent; }
    uint32_t get_image_count() const { return static_cast<uint32_t>(m_images.size()); }
    VkImage get_image(uint32_t index) const { return m_images[index]; }
    VkImageView get_image_view(uint32_t index) const { return m_image_views[index]; }
    bool is_vsync_enabled() const { return m_vsync; }

private:
    bool create_internal(uint32_t width, uint32_t height);

    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkSurfaceFormatKHR m_format{};
    VkPresentModeKHR m_present_mode{VK_PRESENT_MODE_FIFO_KHR};
    VkExtent2D m_extent{0, 0};

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_image_views;
    bool m_vsync{true};
};

} // namespace engine::rhi
