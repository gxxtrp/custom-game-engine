#pragma once

#include "engine/core/config.h"
#include <cstdint>
#include <vulkan/vulkan.h>

namespace engine::rhi {

class IViewportPresenter {
public:
    virtual ~IViewportPresenter() = default;

    virtual bool initialize(uint32_t width, uint32_t height, bool vsync) = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
    virtual VkResult acquire_next_image(VkSemaphore signal_semaphore, VkFence signal_fence, uint32_t& out_image_index) = 0;
    virtual VkResult present(VkQueue queue, VkSemaphore wait_semaphore, uint32_t image_index) = 0;
    virtual void shutdown() = 0;

    [[nodiscard]] virtual bool is_headless() const noexcept = 0;
    [[nodiscard]] virtual uint32_t get_width() const noexcept = 0;
    [[nodiscard]] virtual uint32_t get_height() const noexcept = 0;
    [[nodiscard]] virtual VkFormat get_format() const noexcept = 0;
    [[nodiscard]] virtual uint32_t get_image_count() const noexcept = 0;
    [[nodiscard]] virtual VkImage get_image(uint32_t index) const noexcept = 0;
    [[nodiscard]] virtual VkImageView get_image_view(uint32_t index) const noexcept = 0;
};

} // namespace engine::rhi
