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

    bool init(bool is_timeline = false, uint64_t initial_value = 0);
    void destroy();

    VkSemaphore get_handle() const { return m_semaphore; }
    bool is_timeline() const { return m_is_timeline; }

private:
    VkSemaphore m_semaphore{VK_NULL_HANDLE};
    bool m_is_timeline{false};
};

} // namespace engine::rhi
