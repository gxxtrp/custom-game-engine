#include "engine/rhi/rhi_sync.h"
#include "engine/rhi/rhi_context.h"
#include "engine/core/log.h"

namespace engine::rhi {

RhiFence::~RhiFence() {
    destroy();
}

bool RhiFence::init(bool signaled) {
    VkFenceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

    VkResult res = vkCreateFence(RhiContext::instance().get_device(), &info, nullptr, &m_fence);
    return res == VK_SUCCESS;
}

void RhiFence::destroy() {
    if (m_fence != VK_NULL_HANDLE) {
        vkDestroyFence(RhiContext::instance().get_device(), m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;
    }
}

bool RhiFence::wait(uint64_t timeout_ns) {
    if (m_fence == VK_NULL_HANDLE) return false;
    return vkWaitForFences(RhiContext::instance().get_device(), 1, &m_fence, VK_TRUE, timeout_ns) == VK_SUCCESS;
}

void RhiFence::reset() {
    if (m_fence != VK_NULL_HANDLE) {
        vkResetFences(RhiContext::instance().get_device(), 1, &m_fence);
    }
}

bool RhiFence::is_signaled() {
    if (m_fence == VK_NULL_HANDLE) return false;
    return vkGetFenceStatus(RhiContext::instance().get_device(), m_fence) == VK_SUCCESS;
}

RhiSemaphore::~RhiSemaphore() {
    destroy();
}

bool RhiSemaphore::init(bool is_timeline, uint64_t initial_value) {
    m_is_timeline = is_timeline;

    VkSemaphoreTypeCreateInfo type_info{};
    type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    type_info.semaphoreType = is_timeline ? VK_SEMAPHORE_TYPE_TIMELINE : VK_SEMAPHORE_TYPE_BINARY;
    type_info.initialValue = initial_value;

    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.pNext = is_timeline ? &type_info : nullptr;

    VkResult res = vkCreateSemaphore(RhiContext::instance().get_device(), &info, nullptr, &m_semaphore);
    return res == VK_SUCCESS;
}

void RhiSemaphore::destroy() {
    if (m_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(RhiContext::instance().get_device(), m_semaphore, nullptr);
        m_semaphore = VK_NULL_HANDLE;
    }
}

} // namespace engine::rhi
