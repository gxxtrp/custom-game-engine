#pragma once

#include "engine/core/config.h"
#include "engine/core/platform.h"
#include "engine/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <string>
#include <memory>

namespace engine::rhi {

struct QueueFamilyIndices {
    uint32_t graphics_family{UINT32_MAX};
    uint32_t compute_family{UINT32_MAX};
    uint32_t transfer_family{UINT32_MAX};

    bool is_complete() const {
        return graphics_family != UINT32_MAX;
    }
};

struct RhiDeviceCaps {
    bool ray_tracing_supported{false};
    bool mesh_shader_supported{false};
    bool dynamic_rendering_supported{true};
    bool synchronization2_supported{true};
    bool timeline_semaphore_supported{true};
    bool buffer_device_address_supported{false};

    std::string device_name;
    uint32_t api_version{0};
    uint32_t driver_version{0};
};

class RhiContext {
public:
    static RhiContext& instance();

    bool init(core::Window& window, bool enable_validation = true);
    void shutdown();
    void wait_idle();

    VkInstance get_instance() const { return m_instance; }
    VkPhysicalDevice get_physical_device() const { return m_physical_device; }
    VkDevice get_device() const { return m_device; }
    VkSurfaceKHR get_surface() const { return m_surface; }
    VmaAllocator get_vma_allocator() const { return m_vma_allocator; }

    VkQueue get_graphics_queue() const { return m_graphics_queue; }
    VkQueue get_compute_queue() const { return m_compute_queue; }
    VkQueue get_transfer_queue() const { return m_transfer_queue; }

    const QueueFamilyIndices& get_queue_families() const { return m_queue_families; }
    const RhiDeviceCaps& get_caps() const { return m_caps; }

    bool is_initialized() const { return m_device != VK_NULL_HANDLE; }

private:
    RhiContext() = default;
    ~RhiContext();

    bool create_instance(bool enable_validation);
    bool create_surface(core::Window& window);
    bool select_physical_device();
    bool create_logical_device();
    bool init_vma();

    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debug_messenger{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VkPhysicalDevice m_physical_device{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};

    QueueFamilyIndices m_queue_families;
    VkQueue m_graphics_queue{VK_NULL_HANDLE};
    VkQueue m_compute_queue{VK_NULL_HANDLE};
    VkQueue m_transfer_queue{VK_NULL_HANDLE};

    VmaAllocator m_vma_allocator{VK_NULL_HANDLE};
    RhiDeviceCaps m_caps{};
    bool m_validation_enabled{false};
};

} // namespace engine::rhi
