#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <vector>

namespace engine::rhi {

enum class Format : uint32_t {
    Undefined = VK_FORMAT_UNDEFINED,
    R8_UNORM = VK_FORMAT_R8_UNORM,
    R8G8B8A8_UNORM = VK_FORMAT_R8G8B8A8_UNORM,
    R8G8B8A8_SRGB = VK_FORMAT_R8G8B8A8_SRGB,
    B8G8R8A8_UNORM = VK_FORMAT_B8G8R8A8_UNORM,
    B8G8R8A8_SRGB = VK_FORMAT_B8G8R8A8_SRGB,
    R16G16_SFLOAT = VK_FORMAT_R16G16_SFLOAT,
    R16G16B16A16_SFLOAT = VK_FORMAT_R16G16B16A16_SFLOAT,
    R32G32B32A32_SFLOAT = VK_FORMAT_R32G32B32A32_SFLOAT,
    R32G32B32_SFLOAT = VK_FORMAT_R32G32B32_SFLOAT,
    R32G32_SFLOAT = VK_FORMAT_R32G32_SFLOAT,
    R32_SFLOAT = VK_FORMAT_R32_SFLOAT,
    D32_SFLOAT = VK_FORMAT_D32_SFLOAT,
    D24_UNORM_S8_UINT = VK_FORMAT_D24_UNORM_S8_UINT
};

enum class ShaderStage : uint32_t {
    Vertex = VK_SHADER_STAGE_VERTEX_BIT,
    Fragment = VK_SHADER_STAGE_FRAGMENT_BIT,
    Compute = VK_SHADER_STAGE_COMPUTE_BIT,
    Mesh = VK_SHADER_STAGE_MESH_BIT_EXT,
    Task = VK_SHADER_STAGE_TASK_BIT_EXT,
    Raygen = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
    Miss = VK_SHADER_STAGE_MISS_BIT_KHR,
    ClosestHit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
};

struct Viewport {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};
    float min_depth{0.0f};
    float max_depth{1.0f};
};

struct Rect2D {
    int32_t offset_x{0};
    int32_t offset_y{0};
    uint32_t width{0};
    uint32_t height{0};
};

struct ColorAttachmentDesc {
    VkImageView image_view{VK_NULL_HANDLE};
    VkImageLayout image_layout{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentLoadOp load_op{VK_ATTACHMENT_LOAD_OP_CLEAR};
    VkAttachmentStoreOp store_op{VK_ATTACHMENT_STORE_OP_STORE};
    core::Vec4 clear_color{0.05f, 0.05f, 0.08f, 1.0f};
};

struct DepthAttachmentDesc {
    VkImageView image_view{VK_NULL_HANDLE};
    VkImageLayout image_layout{VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkAttachmentLoadOp load_op{VK_ATTACHMENT_LOAD_OP_CLEAR};
    VkAttachmentStoreOp store_op{VK_ATTACHMENT_STORE_OP_DONT_CARE};
    float clear_depth{1.0f};
    uint32_t clear_stencil{0};
};

struct RenderingDesc {
    Rect2D render_area{};
    std::vector<ColorAttachmentDesc> color_attachments;
    DepthAttachmentDesc depth_attachment{};
    bool has_depth{false};
};

} // namespace engine::rhi
