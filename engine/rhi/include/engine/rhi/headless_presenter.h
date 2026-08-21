#pragma once

#include "engine/rhi/viewport_presenter.h"

namespace engine::rhi {

class HeadlessPresenter final : public IViewportPresenter {
public:
    HeadlessPresenter();
    ~HeadlessPresenter() override;

    bool initialize(uint32_t width, uint32_t height, bool vsync) override;
    void resize(uint32_t width, uint32_t height) override;
    VkResult acquire_next_image(VkSemaphore signal_semaphore, VkFence signal_fence, uint32_t& out_image_index) override;
    VkResult present(VkQueue queue, VkSemaphore wait_semaphore, uint32_t image_index) override;
    void shutdown() override;

    [[nodiscard]] bool is_headless() const noexcept override { return true; }
    [[nodiscard]] uint32_t get_width() const noexcept override { return m_width; }
    [[nodiscard]] uint32_t get_height() const noexcept override { return m_height; }
    [[nodiscard]] VkFormat get_format() const noexcept override { return VK_FORMAT_R8G8B8A8_UNORM; }
    [[nodiscard]] uint32_t get_image_count() const noexcept override { return 1; }
    [[nodiscard]] VkImage get_image(uint32_t index) const noexcept override { (void)index; return VK_NULL_HANDLE; }
    [[nodiscard]] VkImageView get_image_view(uint32_t index) const noexcept override { (void)index; return VK_NULL_HANDLE; }

private:
    uint32_t m_width{1280};
    uint32_t m_height{720};
    uint32_t m_frame_count{0};
};

} // namespace engine::rhi
