#pragma once

#include "engine/core/config.h"
#include "engine/rhi/rhi_types.h"
#include <vulkan/vulkan.h>

namespace engine::rhi {

// Non-owning reference to an externally-created Vulkan image (swapchain surface,
// offscreen presenter attachment, imported render target). Producers fill this from
// the presenter; consumers (SceneRenderer, render features) use it as the final
// frame target. The optional acquisition semaphore is waited on before the image is
// written, so the producer's acquire/present synchronization travels with the handle.
struct RHIImageHandle {
    VkImage image{VK_NULL_HANDLE};
    VkImageView image_view{VK_NULL_HANDLE};
    Format format{Format::Undefined};
    uint32_t width{0};
    uint32_t height{0};
    VkImageLayout initial_layout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkSemaphore acquire_semaphore{VK_NULL_HANDLE}; // optional, waited before first write

    [[nodiscard]] bool is_valid() const {
        return image != VK_NULL_HANDLE && image_view != VK_NULL_HANDLE;
    }

    [[nodiscard]] uint32_t get_width_or(uint32_t fallback) const { return width > 0 ? width : fallback; }
    [[nodiscard]] uint32_t get_height_or(uint32_t fallback) const { return height > 0 ? height : fallback; }
};

} // namespace engine::rhi
