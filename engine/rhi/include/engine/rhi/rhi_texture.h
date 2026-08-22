#pragma once

#include "engine/core/config.h"
#include "engine/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <string>
#include <vector>

namespace engine::rhi {

enum class TextureDimension : uint8_t {
    Texture2D = 0,
    Texture3D,
    TextureCube,
    Texture2DArray
};

enum class TextureUsage : uint32_t {
    Sampled = VK_IMAGE_USAGE_SAMPLED_BIT,
    Storage = VK_IMAGE_USAGE_STORAGE_BIT,
    ColorAttachment = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    DepthAttachment = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    TransferSrc = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    TransferDst = VK_IMAGE_USAGE_TRANSFER_DST_BIT
};

inline TextureUsage operator|(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(TextureUsage a, TextureUsage b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

struct TextureDesc {
    uint32_t width{1};
    uint32_t height{1};
    uint32_t depth{1};
    uint32_t mip_levels{1};
    uint32_t array_layers{1};
    Format format{Format::R8G8B8A8_UNORM};
    TextureDimension dimension{TextureDimension::Texture2D};
    TextureUsage usage{TextureUsage::Sampled | TextureUsage::TransferDst};
    VkSampleCountFlagBits sample_count{VK_SAMPLE_COUNT_1_BIT};
    std::string debug_name{"RhiTexture"};
};

class RhiTexture {
public:
    RhiTexture() = default;
    ~RhiTexture();

    bool init(const TextureDesc& desc);
    void destroy();

    VkImage get_handle() const { return m_image; }
    VkImageView get_view() const { return m_image_view; }
    const TextureDesc& get_desc() const { return m_desc; }
    uint32_t get_width() const { return m_desc.width; }
    uint32_t get_height() const { return m_desc.height; }
    Format get_format() const { return m_desc.format; }

    // Creates (and caches) a single-array-layer image view. Required for rendering
    // individual cascade slices of a Texture2DArray attachment (e.g. CSM).
    VkImageView get_or_create_layer_view(uint32_t array_layer) const;

    bool is_valid() const { return m_image != VK_NULL_HANDLE; }

private:
    TextureDesc m_desc{};
    VkImage m_image{VK_NULL_HANDLE};
    VkImageView m_image_view{VK_NULL_HANDLE};
    mutable VkImageView m_array_layer_views[8]{};
    mutable uint32_t m_array_layer_indices[8]{};
    mutable uint32_t m_array_layer_view_count{0};
    VmaAllocation m_allocation{VK_NULL_HANDLE};
    VmaAllocationInfo m_alloc_info{};
};

enum class SamplerFilter : uint8_t {
    Nearest = 0,
    Linear
};

enum class SamplerAddressMode : uint8_t {
    Repeat = 0,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder
};

struct SamplerDesc {
    SamplerFilter min_filter{SamplerFilter::Linear};
    SamplerFilter mag_filter{SamplerFilter::Linear};
    SamplerFilter mipmap_mode{SamplerFilter::Linear};
    SamplerAddressMode address_u{SamplerAddressMode::Repeat};
    SamplerAddressMode address_v{SamplerAddressMode::Repeat};
    SamplerAddressMode address_w{SamplerAddressMode::Repeat};
    float max_anisotropy{16.0f};
    bool enable_anisotropy{true};
    bool compare_enable{false};
    VkCompareOp compare_op{VK_COMPARE_OP_ALWAYS};
    std::string debug_name{"RhiSampler"};
};

class RhiSampler {
public:
    RhiSampler() = default;
    ~RhiSampler();

    bool init(const SamplerDesc& desc);
    void destroy();

    VkSampler get_handle() const { return m_sampler; }

private:
    SamplerDesc m_desc{};
    VkSampler m_sampler{VK_NULL_HANDLE};
};

} // namespace engine::rhi
