#pragma once

#include "engine/core/config.h"
#include <vulkan/vulkan.h>

namespace engine::rhi {

class RhiFence {
public:
    RhiFence() = default;
    ~RhiFence();

    bool init(bool signaled = false);
    void destroy();

    bool wait(uint64_t timeout_ns = UINT64_MAX);
    void reset();
    bool is_signaled();

    VkFence get_handle() const { return m_fence; }

private:
    VkFence m_fence{VK_NULL_HANDLE};
};

class RhiSemaphore {
public:
    RhiSemaphore() = default;
    ~RhiSemaphore();
    RhiSemaphore(const RhiSemaphore&) = delete;
    RhiSemaphore& operator=(const RhiSemaphore&) = delete;
    RhiSemaphore(RhiSemaphore&& other) noexcept
        : m_semaphore(other.m_semaphore), m_is_timeline(other.m_is_timeline) {
        other.m_semaphore = VK_NULL_HANDLE;
    }
    RhiSemaphore& operator=(RhiSemaphore&& other) noexcept {
        if (this != &other) {
            destroy();
            m_semaphore = other.m_semaphore;
            m_is_timeline = other.m_is_timeline;
            other.m_semaphore = VK_NULL_HANDLE;
        }
        return *this;
    }

    bool init(bool is_timeline = false, uint64_t initial_value = 0);
    void destroy();

    VkSemaphore get_handle() const { return m_semaphore; }
    bool is_timeline() const { return m_is_timeline; }

private:
    VkSemaphore m_semaphore{VK_NULL_HANDLE};
    bool m_is_timeline{false};
};

} // namespace engine::rhi
