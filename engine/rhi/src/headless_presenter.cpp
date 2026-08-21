#include "engine/rhi/headless_presenter.h"

namespace engine::rhi {

HeadlessPresenter::HeadlessPresenter() = default;

HeadlessPresenter::~HeadlessPresenter() = default;

bool HeadlessPresenter::initialize(uint32_t width, uint32_t height, bool vsync) {
    (void)vsync;
    m_width = width > 0 ? width : 1280;
    m_height = height > 0 ? height : 720;
    m_frame_count = 0;
    return true;
}

void HeadlessPresenter::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
}

VkResult HeadlessPresenter::acquire_next_image(VkSemaphore signal_semaphore, VkFence signal_fence, uint32_t& out_image_index) {
    (void)signal_semaphore;
    (void)signal_fence;
    out_image_index = m_frame_count % 2;
    return VK_SUCCESS;
}

VkResult HeadlessPresenter::present(VkQueue queue, VkSemaphore wait_semaphore, uint32_t image_index) {
    (void)queue;
    (void)wait_semaphore;
    (void)image_index;
    m_frame_count++;
    return VK_SUCCESS;
}

void HeadlessPresenter::shutdown() {
    m_frame_count = 0;
}

} // namespace engine::rhi
