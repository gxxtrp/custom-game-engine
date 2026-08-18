#include "engine/renderer/render_pass.h"
#include "engine/renderer/render_graph.h"

namespace engine::renderer {

RGTextureHandle RenderPassBuilder::create_texture(const RGTextureDesc& desc) {
    RGTextureHandle handle = m_graph.create_texture(desc);
    write_texture(handle, RGResourceAccess::ColorAttachmentWrite);
    return handle;
}

RGBufferHandle RenderPassBuilder::create_buffer(const RGBufferDesc& desc) {
    RGBufferHandle handle = m_graph.create_buffer(desc);
    write_buffer(handle, RGResourceAccess::ShaderWrite);
    return handle;
}

void RenderPassBuilder::read_texture(RGTextureHandle handle, RGResourceAccess access) {
    m_graph.add_resource_read(m_pass_index, handle, access);
}

void RenderPassBuilder::write_texture(RGTextureHandle handle, RGResourceAccess access) {
    m_graph.add_resource_write(m_pass_index, handle, access);
}

void RenderPassBuilder::read_buffer(RGBufferHandle handle, RGResourceAccess access) {
    m_graph.add_resource_read(m_pass_index, handle, access);
}

void RenderPassBuilder::write_buffer(RGBufferHandle handle, RGResourceAccess access) {
    m_graph.add_resource_write(m_pass_index, handle, access);
}

void RenderPassBuilder::set_color_attachment(uint32_t slot, RGTextureHandle handle, 
                                            VkAttachmentLoadOp load_op, 
                                            VkAttachmentStoreOp store_op, 
                                            core::Vec4 clear_color) {
    write_texture(handle, RGResourceAccess::ColorAttachmentWrite);
    RGAttachmentInfo info{
        .handle = handle,
        .load_op = load_op,
        .store_op = store_op,
        .clear_color = clear_color
    };
    m_graph.set_pass_color_attachment(m_pass_index, info);
}

void RenderPassBuilder::set_depth_attachment(RGTextureHandle handle, 
                                            VkAttachmentLoadOp load_op, 
                                            VkAttachmentStoreOp store_op, 
                                            float clear_depth) {
    write_texture(handle, RGResourceAccess::DepthAttachmentWrite);
    RGAttachmentInfo info{
        .handle = handle,
        .load_op = load_op,
        .store_op = store_op,
        .clear_depth = clear_depth
    };
    m_graph.set_pass_depth_attachment(m_pass_index, info);
}

VkImageView RenderPassContext::get_texture_view(RGTextureHandle handle) const {
    return m_graph.get_texture_view(handle);
}

VkImage RenderPassContext::get_texture_image(RGTextureHandle handle) const {
    return m_graph.get_texture_image(handle);
}

VkBuffer RenderPassContext::get_buffer_handle(RGBufferHandle handle) const {
    return m_graph.get_buffer_handle(handle);
}

} // namespace engine::renderer
