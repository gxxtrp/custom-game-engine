#include "engine/rhi/rhi_texture.h"
#include "engine/rhi/rhi_context.h"
#include "engine/core/log.h"

namespace engine::rhi {

RhiTexture::~RhiTexture() {
    destroy();
}

bool RhiTexture::init(const TextureDesc& desc) {
    destroy();
    m_desc = desc;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    switch (desc.dimension) {
        case TextureDimension::Texture2D:
        case TextureDimension::Texture2DArray:
            image_info.imageType = VK_IMAGE_TYPE_2D;
            break;
        case TextureDimension::Texture3D:
            image_info.imageType = VK_IMAGE_TYPE_3D;
            break;
        case TextureDimension::TextureCube:
            image_info.imageType = VK_IMAGE_TYPE_2D;
            image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            break;
    }

    image_info.extent.width = desc.width;
    image_info.extent.height = desc.height;
    image_info.extent.depth = desc.depth;
    image_info.mipLevels = desc.mip_levels;
    image_info.arrayLayers = (desc.dimension == TextureDimension::TextureCube) ? 6 : desc.array_layers;
    image_info.format = static_cast<VkFormat>(desc.format);
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = static_cast<VkImageUsageFlags>(desc.usage);
    image_info.samples = desc.sample_count;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VmaAllocator allocator = RhiContext::instance().get_vma_allocator();
    VkResult res = vmaCreateImage(allocator, &image_info, &alloc_info, &m_image, &m_allocation, &m_alloc_info);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to create VMA texture '{}' ({}x{}): {}", desc.debug_name, desc.width, desc.height, static_cast<int>(res));
        return false;
    }

    // Create default image view
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = m_image;

    switch (desc.dimension) {
        case TextureDimension::Texture2D:
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            break;
        case TextureDimension::Texture2DArray:
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            break;
        case TextureDimension::Texture3D:
            view_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
        case TextureDimension::TextureCube:
            view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;
    }

    view_info.format = static_cast<VkFormat>(desc.format);
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = desc.mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = image_info.arrayLayers;

    if (desc.format == Format::D32_SFLOAT || desc.format == Format::D24_UNORM_S8_UINT) {
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else {
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkDevice device = RhiContext::instance().get_device();
    res = vkCreateImageView(device, &view_info, nullptr, &m_image_view);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to create image view for '{}': {}", desc.debug_name, static_cast<int>(res));
        return false;
    }

    return true;
}

void RhiTexture::destroy() {
    VkDevice device = RhiContext::instance().get_device();
    if (m_image_view != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_image_view, nullptr);
        m_image_view = VK_NULL_HANDLE;
    }

    if (m_image != VK_NULL_HANDLE) {
        VmaAllocator allocator = RhiContext::instance().get_vma_allocator();
        if (allocator != VK_NULL_HANDLE && m_allocation != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator, m_image, m_allocation);
        }
        m_image = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

RhiSampler::~RhiSampler() {
    destroy();
}

bool RhiSampler::init(const SamplerDesc& desc) {
    destroy();
    m_desc = desc;

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    auto map_filter = [](SamplerFilter f) -> VkFilter {
        return (f == SamplerFilter::Linear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    };

    auto map_address = [](SamplerAddressMode m) -> VkSamplerAddressMode {
        switch (m) {
            case SamplerAddressMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case SamplerAddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case SamplerAddressMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case SamplerAddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        }
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    };

    sampler_info.magFilter = map_filter(desc.mag_filter);
    sampler_info.minFilter = map_filter(desc.min_filter);
    sampler_info.mipmapMode = (desc.mipmap_mode == SamplerFilter::Linear) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_info.addressModeU = map_address(desc.address_u);
    sampler_info.addressModeV = map_address(desc.address_v);
    sampler_info.addressModeW = map_address(desc.address_w);
    sampler_info.anisotropyEnable = desc.enable_anisotropy ? VK_TRUE : VK_FALSE;
    sampler_info.maxAnisotropy = desc.max_anisotropy;
    sampler_info.compareEnable = desc.compare_enable ? VK_TRUE : VK_FALSE;
    sampler_info.compareOp = desc.compare_op;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = VK_LOD_CLAMP_NONE;
    sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    VkDevice device = RhiContext::instance().get_device();
    VkResult res = vkCreateSampler(device, &sampler_info, nullptr, &m_sampler);
    if (res != VK_SUCCESS) {
        LOG_FATAL("RHI", "Failed to create sampler '{}': {}", desc.debug_name, static_cast<int>(res));
        return false;
    }

    return true;
}

void RhiSampler::destroy() {
    if (m_sampler != VK_NULL_HANDLE) {
        VkDevice device = RhiContext::instance().get_device();
        if (device != VK_NULL_HANDLE) {
            vkDestroySampler(device, m_sampler, nullptr);
        }
        m_sampler = VK_NULL_HANDLE;
    }
}

} // namespace engine::rhi
