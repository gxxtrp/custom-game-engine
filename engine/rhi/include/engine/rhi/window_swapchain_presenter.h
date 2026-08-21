#pragma once

#include "engine/rhi/viewport_presenter.h"
#include "engine/rhi/rhi_swapchain.h"

namespace engine::rhi {

class WindowSwapchainPresenter final : public IViewportPresenter {
public:
    WindowSwapchainPresenter();
    ~WindowSwapchainPresenter() override;

    bool initialize(uint32_t width, uint32_t height, bool vsync) override;
    void resize(uint32_t width, uint32_t height) override;
    VkResult acquire_next_image(VkSemaphore signal_semaphore, VkFence signal_fence, uint32_t& out_image_index) override;
    VkResult present(VkQueue queue, VkSemaphore wait_semaphore, uint32_t image_index) override;
    void shutdown() override;

    [[nodiscard]] bool is_headless() const noexcept override { return false; }
    [[nodiscard]] uint32_t get_width() const noexcept override { return m_swapchain.get_extent().width; }
    [[nodiscard]] uint32_t get_height() const noexcept override { return m_swapchain.get_extent().height; }
    [[nodiscard]] VkFormat get_format() const noexcept override { return m_swapchain.get_format(); }
    [[nodiscard]] uint32_t get_image_count() const noexcept override { return m_swapchain.get_image_count(); }
    [[nodiscard]] VkImage get_image(uint32_t index) const noexcept override { return m_swapchain.get_image(index); }
    [[nodiscard]] VkImageView get_image_view(uint32_t index) const noexcept override { return m_swapchain.get_image_view(index); }

    [[nodiscard]] RhiSwapchain& get_swapchain() noexcept { return m_swapchain; }
    [[nodiscard]] const RhiSwapchain& get_swapchain() const noexcept { return m_swapchain; }

private:
    RhiSwapchain m_swapchain;
};

} // namespace engine::rhi
