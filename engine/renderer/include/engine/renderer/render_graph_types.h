#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/rhi/rhi_types.h"
#include "engine/rhi/rhi_buffer.h"
#include "engine/rhi/rhi_texture.h"
#include <cstdint>
#include <string>

namespace engine::renderer {

enum class RGResourceType : uint8_t {
    Invalid = 0,
    Texture,
    Buffer
};

struct RGResourceHandle {
    uint32_t id{UINT32_MAX};
    uint32_t version{0};
    RGResourceType type{RGResourceType::Invalid};

    bool is_valid() const { return id != UINT32_MAX && type != RGResourceType::Invalid; }
    bool operator==(const RGResourceHandle& other) const {
        return id == other.id && version == other.version && type == other.type;
    }
    bool operator!=(const RGResourceHandle& other) const { return !(*this == other); }
};

struct RGTextureHandle : public RGResourceHandle {
    RGTextureHandle() { type = RGResourceType::Texture; }
    explicit RGTextureHandle(uint32_t _id, uint32_t _ver = 0) {
        id = _id;
        version = _ver;
        type = RGResourceType::Texture;
    }
};

struct RGBufferHandle : public RGResourceHandle {
    RGBufferHandle() { type = RGResourceType::Buffer; }
    explicit RGBufferHandle(uint32_t _id, uint32_t _ver = 0) {
        id = _id;
        version = _ver;
        type = RGResourceType::Buffer;
    }
};

enum class RGResourceAccess : uint32_t {
    None = 0,
    ShaderRead = 1 << 0,
    ShaderWrite = 1 << 1,
    ColorAttachmentWrite = 1 << 2,
    ColorAttachmentReadWrite = 1 << 3,
    DepthAttachmentWrite = 1 << 4,
    DepthAttachmentRead = 1 << 5,
    TransferSrc = 1 << 6,
    TransferDst = 1 << 7,
    Present = 1 << 8
};

inline RGResourceAccess operator|(RGResourceAccess a, RGResourceAccess b) {
    return static_cast<RGResourceAccess>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(RGResourceAccess a, RGResourceAccess b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

struct RGTextureDesc {
    uint32_t width{1};
    uint32_t height{1};
    rhi::Format format{rhi::Format::R8G8B8A8_UNORM};
    rhi::TextureUsage usage{rhi::TextureUsage::Sampled | rhi::TextureUsage::ColorAttachment};
    std::string debug_name{"RGTexture"};
};

struct RGBufferDesc {
    size_t size{0};
    rhi::BufferUsage usage{rhi::BufferUsage::Storage};
    rhi::MemoryUsage memory_usage{rhi::MemoryUsage::GpuOnly};
    std::string debug_name{"RGBuffer"};
};

} // namespace engine::renderer
