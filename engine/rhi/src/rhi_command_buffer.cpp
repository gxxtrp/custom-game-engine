#include "engine/rhi/rhi_command_buffer.h"
#include "engine/rhi/rhi_context.h"
#include "engine/core/log.h"

namespace engine::rhi {

RhiCommandPool::~RhiCommandPool() {
    destroy();
}

bool RhiCommandPool::init(uint32_t queue_family_index, VkCommandPoolCreateFlags flags) {
    VkDevice device = RhiContext::instance().get_device();
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = flags;
    pool_info.queueFamilyIndex = queue_family_index;

    VkResult res = vkCreateCommandPool(device, &pool_info, nullptr, &m_pool);
    if (res != VK_SUCCESS) {
        LOG_ERROR("RHI", "Failed to create command pool: {}", static_cast<int>(res));
        return false;
    }
    return true;
}

void RhiCommandPool::destroy() {
    if (m_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(RhiContext::instance().get_device(), m_pool, nullptr);
        m_pool = VK_NULL_HANDLE;
    }
}

RhiCommandBuffer::~RhiCommandBuffer() {
    // Command buffers are destroyed when their pool is destroyed or via explicit destroy
}

bool RhiCommandBuffer::init(VkCommandPool pool, VkCommandBufferLevel level) {
    VkDevice device = RhiContext::instance().get_device();
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pool;
    alloc_info.level = level;
    alloc_info.commandBufferCount = 1;

    VkResult res = vkAllocateCommandBuffers(device, &alloc_info, &m_cmd);
    if (res != VK_SUCCESS) {
        LOG_ERROR("RHI", "Failed to allocate command buffer: {}", static_cast<int>(res));
        return false;
    }
    return true;
}

void RhiCommandBuffer::destroy(VkCommandPool pool) {
    if (m_cmd != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(RhiContext::instance().get_device(), pool, 1, &m_cmd);
        m_cmd = VK_NULL_HANDLE;
    }
}

bool RhiCommandBuffer::begin(VkCommandBufferUsageFlags flags) {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = flags;

    VkResult res = vkBeginCommandBuffer(m_cmd, &begin_info);
    if (res == VK_SUCCESS) {
        m_is_recording = true;
        return true;
    }
    return false;
}

bool RhiCommandBuffer::end() {
    if (!m_is_recording) return false;
    if (m_is_rendering) end_rendering();

    VkResult res = vkEndCommandBuffer(m_cmd);
    m_is_recording = false;
    return res == VK_SUCCESS;
}

void RhiCommandBuffer::reset() {
    if (m_cmd != VK_NULL_HANDLE) {
        vkResetCommandBuffer(m_cmd, 0);
    }
    m_is_recording = false;
    m_is_rendering = false;
}

void RhiCommandBuffer::begin_rendering(const RenderingDesc& desc) {
    std::vector<VkRenderingAttachmentInfo> color_infos;
    color_infos.reserve(desc.color_attachments.size());

    for (const auto& att : desc.color_attachments) {
        VkRenderingAttachmentInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        info.imageView = att.image_view;
        info.imageLayout = att.image_layout;
        info.loadOp = att.load_op;
        info.storeOp = att.store_op;
        info.clearValue.color = {
            {att.clear_color.x, att.clear_color.y, att.clear_color.z, att.clear_color.w}
        };
        color_infos.push_back(info);
    }

    VkRenderingAttachmentInfo depth_info{};
    if (desc.has_depth) {
        depth_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_info.imageView = desc.depth_attachment.image_view;
        depth_info.imageLayout = desc.depth_attachment.image_layout;
        depth_info.loadOp = desc.depth_attachment.load_op;
        depth_info.storeOp = desc.depth_attachment.store_op;
        depth_info.clearValue.depthStencil.depth = desc.depth_attachment.clear_depth;
        depth_info.clearValue.depthStencil.stencil = desc.depth_attachment.clear_stencil;
    }

    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea.offset = {desc.render_area.offset_x, desc.render_area.offset_y};
    rendering_info.renderArea.extent = {desc.render_area.width, desc.render_area.height};
    rendering_info.layerCount = 1;
    rendering_info.colorAttachmentCount = static_cast<uint32_t>(color_infos.size());
    rendering_info.pColorAttachments = color_infos.data();
    rendering_info.pDepthAttachment = desc.has_depth ? &depth_info : nullptr;

    vkCmdBeginRendering(m_cmd, &rendering_info);
    m_is_rendering = true;
}

void RhiCommandBuffer::end_rendering() {
    if (m_is_rendering) {
        vkCmdEndRendering(m_cmd);
        m_is_rendering = false;
    }
}

void RhiCommandBuffer::transition_image_layout(VkImage image, 
                                               VkImageLayout old_layout, 
                                               VkImageLayout new_layout, 
                                               VkImageAspectFlags aspect_mask) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect_mask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else {
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    }

    vkCmdPipelineBarrier(m_cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void RhiCommandBuffer::set_viewport(const Viewport& vp) {
    VkViewport v{
        .x = vp.x,
        .y = vp.y,
        .width = vp.width,
        .height = vp.height,
        .minDepth = vp.min_depth,
        .maxDepth = vp.max_depth
    };
    vkCmdSetViewport(m_cmd, 0, 1, &v);
}

void RhiCommandBuffer::set_scissor(const Rect2D& sc) {
    VkRect2D s{
        .offset = {sc.offset_x, sc.offset_y},
        .extent = {sc.width, sc.height}
    };
    vkCmdSetScissor(m_cmd, 0, 1, &s);
}

void RhiCommandBuffer::bind_pipeline(VkPipeline pipeline, VkPipelineBindPoint bind_point) {
    vkCmdBindPipeline(m_cmd, bind_point, pipeline);
}

void RhiCommandBuffer::bind_vertex_buffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset) {
    vkCmdBindVertexBuffers(m_cmd, binding, 1, &buffer, &offset);
}

void RhiCommandBuffer::bind_index_buffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType index_type) {
    vkCmdBindIndexBuffer(m_cmd, buffer, offset, index_type);
}

void RhiCommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
    vkCmdDraw(m_cmd, vertex_count, instance_count, first_vertex, first_instance);
}

void RhiCommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
    vkCmdDrawIndexed(m_cmd, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void RhiCommandBuffer::push_constants(VkPipelineLayout layout, VkShaderStageFlags stages, uint32_t offset, uint32_t size, const void* values) {
    vkCmdPushConstants(m_cmd, layout, stages, offset, size, values);
}

} // namespace engine::rhi
