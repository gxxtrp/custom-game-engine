#pragma once

#include "engine/core/config.h"
#include "engine/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <cstdint>
#include <string>

namespace engine::rhi {

enum class BufferUsage : uint32_t {
    Vertex = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    Index = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    Uniform = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    Storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    Indirect = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
    TransferSrc = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    TransferDst = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    ShaderDeviceAddress = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
};

inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(BufferUsage a, BufferUsage b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

enum class MemoryUsage : uint8_t {
    GpuOnly = 0,    // Device local memory
    CpuOnly,        // Host visible memory (staging)
    CpuToGpu,       // Host visible & coherent memory (dynamic uniforms)
    GpuToCpu        // Readback
};

struct BufferDesc {
    size_t size{0};
    BufferUsage usage{BufferUsage::Uniform};
    MemoryUsage memory_usage{MemoryUsage::GpuOnly};
    std::string debug_name{"RhiBuffer"};
};

class RhiBuffer {
public:
    RhiBuffer() = default;
    ~RhiBuffer();

    bool init(const BufferDesc& desc);
    void destroy();

    void* map();
    void unmap();
    bool upload_data(const void* data, size_t size, size_t offset = 0);

    VkBuffer get_handle() const { return m_buffer; }
    size_t get_size() const { return m_desc.size; }
    const BufferDesc& get_desc() const { return m_desc; }
    VkDeviceAddress get_device_address() const;

    bool is_valid() const { return m_buffer != VK_NULL_HANDLE; }

private:
    BufferDesc m_desc{};
    VkBuffer m_buffer{VK_NULL_HANDLE};
    VmaAllocation m_allocation{VK_NULL_HANDLE};
    VmaAllocationInfo m_alloc_info{};
    void* m_mapped_ptr{nullptr};
};

} // namespace engine::rhi
