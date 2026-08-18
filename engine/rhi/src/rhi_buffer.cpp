#include "engine/rhi/rhi_buffer.h"
#include "engine/rhi/rhi_context.h"
#include "engine/core/log.h"
#include <cstring>

namespace engine::rhi {

RhiBuffer::~RhiBuffer() {
    destroy();
}

bool RhiBuffer::init(const BufferDesc& desc) {
    destroy();
    m_desc = desc;

    if (desc.size == 0) {
        LOG_ERROR("RHI", "Buffer size cannot be 0 for '{}'", desc.debug_name);
        return false;
    }

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = desc.size;
    buffer_info.usage = static_cast<VkBufferUsageFlags>(desc.usage);
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    switch (desc.memory_usage) {
        case MemoryUsage::GpuOnly:
            alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            break;
        case MemoryUsage::CpuOnly:
            alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
            alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            break;
        case MemoryUsage::CpuToGpu:
            alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        case MemoryUsage::GpuToCpu:
            alloc_info.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
            alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
            break;
    }

    VmaAllocator allocator = RhiContext::instance().get_vma_allocator();
    VkResult res = vmaCreateBuffer(allocator, &buffer_info, &alloc_info, &m_buffer, &m_allocation, &m_alloc_info);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to create VMA buffer '{}' (size: {} bytes): {}", desc.debug_name, desc.size, static_cast<int>(res));
        return false;
    }

    if (desc.memory_usage == MemoryUsage::CpuToGpu) {
        m_mapped_ptr = m_alloc_info.pMappedData;
    }

    return true;
}

void RhiBuffer::destroy() {
    if (m_buffer != VK_NULL_HANDLE) {
        unmap();
        VmaAllocator allocator = RhiContext::instance().get_vma_allocator();
        if (allocator != VK_NULL_HANDLE && m_allocation != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, m_buffer, m_allocation);
        }
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
        m_mapped_ptr = nullptr;
    }
}

void* RhiBuffer::map() {
    if (m_mapped_ptr) return m_mapped_ptr;
    if (m_allocation == VK_NULL_HANDLE) return nullptr;

    VmaAllocator allocator = RhiContext::instance().get_vma_allocator();
    VkResult res = vmaMapMemory(allocator, m_allocation, &m_mapped_ptr);
    if (res != VK_SUCCESS) {
        LOG_ERROR("RHI", "Failed to map buffer memory for '{}'", m_desc.debug_name);
        return nullptr;
    }
    return m_mapped_ptr;
}

void RhiBuffer::unmap() {
    if (m_mapped_ptr && m_desc.memory_usage != MemoryUsage::CpuToGpu) {
        VmaAllocator allocator = RhiContext::instance().get_vma_allocator();
        if (allocator != VK_NULL_HANDLE && m_allocation != VK_NULL_HANDLE) {
            vmaUnmapMemory(allocator, m_allocation);
        }
        m_mapped_ptr = nullptr;
    }
}

bool RhiBuffer::upload_data(const void* data, size_t size, size_t offset) {
    if (!data || size == 0 || offset + size > m_desc.size) return false;

    void* ptr = map();
    if (!ptr) return false;

    std::memcpy(static_cast<uint8_t*>(ptr) + offset, data, size);

    if (m_desc.memory_usage != MemoryUsage::CpuToGpu) {
        unmap();
    }
    return true;
}

VkDeviceAddress RhiBuffer::get_device_address() const {
    if (m_buffer == VK_NULL_HANDLE) return 0;

    VkBufferDeviceAddressInfo address_info{};
    address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    address_info.buffer = m_buffer;

    return vkGetBufferDeviceAddress(RhiContext::instance().get_device(), &address_info);
}

} // namespace engine::rhi
