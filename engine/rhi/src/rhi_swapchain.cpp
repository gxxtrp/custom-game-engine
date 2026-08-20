#include "engine/rhi/rhi_swapchain.h"
#include "engine/rhi/rhi_context.h"
#include "engine/core/log.h"
#include <algorithm>

namespace engine::rhi {

RhiSwapchain::~RhiSwapchain() {
    destroy();
}

bool RhiSwapchain::init(uint32_t width, uint32_t height, bool vsync) {
    m_vsync = vsync;
    return create_internal(width, height);
}

void RhiSwapchain::destroy() {
    VkDevice device = RhiContext::instance().get_device();
    if (device == VK_NULL_HANDLE) return;

    for (auto view : m_image_views) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
        }
    }
    m_image_views.clear();
    m_images.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

bool RhiSwapchain::create_internal(uint32_t width, uint32_t height) {
    auto& ctx = RhiContext::instance();
    VkPhysicalDevice phys_dev = ctx.get_physical_device();
    VkDevice device = ctx.get_device();
    VkSurfaceKHR surface = ctx.get_surface();

    // 1. Surface Capabilities
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_dev, surface, &caps);

    // Formats
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev, surface, &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_dev, surface, &format_count, formats.data());

    m_format = formats[0];
    for (const auto& fmt : formats) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            m_format = fmt;
            break;
        }
    }

    // Present Modes
    uint32_t mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys_dev, surface, &mode_count, nullptr);
    std::vector<VkPresentModeKHR> modes(mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(phys_dev, surface, &mode_count, modes.data());

    m_present_mode = VK_PRESENT_MODE_FIFO_KHR; // VSync default
    if (!m_vsync) {
        for (const auto& mode : modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                m_present_mode = mode;
                break;
            }
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                m_present_mode = mode;
            }
        }
    }

    // Extent
    if (caps.currentExtent.width != UINT32_MAX) {
        m_extent = caps.currentExtent;
    } else {
        m_extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
        m_extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = m_format.format;
    create_info.imageColorSpace = m_format.colorSpace;
    create_info.imageExtent = m_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;

    create_info.preTransform = caps.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = m_present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult res = vkCreateSwapchainKHR(device, &create_info, nullptr, &m_swapchain);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to create Vulkan swapchain: {}", static_cast<int>(res));
        return false;
    }

    // Get Images
    uint32_t actual_image_count = 0;
    vkGetSwapchainImagesKHR(device, m_swapchain, &actual_image_count, nullptr);
    m_images.resize(actual_image_count);
    vkGetSwapchainImagesKHR(device, m_swapchain, &actual_image_count, m_images.data());

    // Create Image Views
    m_image_views.resize(actual_image_count);
    for (size_t i = 0; i < actual_image_count; ++i) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = m_format.format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &view_info, nullptr, &m_image_views[i]) != VK_SUCCESS) {
            LOG_FATAL("RHI", "Failed to create swapchain image view {}", i);
            return false;
        }
    }

    LOG_INFO("RHI", "Created swapchain ({}x{}, {} images, format: {}, vsync: {})",
             m_extent.width, m_extent.height, actual_image_count, static_cast<int>(m_format.format), m_vsync);
    return true;
}

VkResult RhiSwapchain::acquire_next_image(VkSemaphore signal_semaphore, VkFence signal_fence, uint32_t& out_image_index) {
    return vkAcquireNextImageKHR(RhiContext::instance().get_device(), m_swapchain, UINT64_MAX, signal_semaphore, signal_fence, &out_image_index);
}

VkResult RhiSwapchain::present(VkQueue queue, VkSemaphore wait_semaphore, uint32_t image_index) {
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = wait_semaphore != VK_NULL_HANDLE ? 1 : 0;
    present_info.pWaitSemaphores = wait_semaphore != VK_NULL_HANDLE ? &wait_semaphore : nullptr;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swapchain;
    present_info.pImageIndices = &image_index;

    return vkQueuePresentKHR(queue, &present_info);
}

bool RhiSwapchain::resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return false;
    RhiContext::instance().wait_idle();
    destroy();
    return create_internal(width, height);
}

} // namespace engine::rhi
