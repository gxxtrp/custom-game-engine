#include "engine/rhi/window_swapchain_presenter.h"

namespace engine::rhi {

WindowSwapchainPresenter::WindowSwapchainPresenter() = default;

WindowSwapchainPresenter::~WindowSwapchainPresenter() {
    shutdown();
}

bool WindowSwapchainPresenter::initialize(uint32_t width, uint32_t height, bool vsync) {
    return m_swapchain.init(width, height, vsync);
}

void WindowSwapchainPresenter::resize(uint32_t width, uint32_t height) {
    m_swapchain.resize(width, height);
}

VkResult WindowSwapchainPresenter::acquire_next_image(VkSemaphore signal_semaphore, VkFence signal_fence, uint32_t& out_image_index) {
    return m_swapchain.acquire_next_image(signal_semaphore, signal_fence, out_image_index);
}

VkResult WindowSwapchainPresenter::present(VkQueue queue, VkSemaphore wait_semaphore, uint32_t image_index) {
    return m_swapchain.present(queue, wait_semaphore, image_index);
}

void WindowSwapchainPresenter::shutdown() {
    m_swapchain.destroy();
}

} // namespace engine::rhi
