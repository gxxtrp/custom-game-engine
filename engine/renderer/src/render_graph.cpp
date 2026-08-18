#include "engine/renderer/render_graph.h"
#include "engine/core/log.h"
#include <algorithm>

namespace engine::renderer {

RenderGraph::~RenderGraph() {
    destroy();
}

void RenderGraph::destroy() {
    reset();
    m_texture_pool.clear();
    m_buffer_pool.clear();
}

RGTextureHandle RenderGraph::import_texture(std::string_view name, 
                                           VkImage image, 
                                           VkImageView view, 
                                           uint32_t width, 
                                           uint32_t height, 
                                           rhi::Format format, 
                                           VkImageLayout initial_layout) {
    uint32_t id = static_cast<uint32_t>(m_textures.size());
    auto& res = m_textures.emplace_back();
    res.desc.width = width;
    res.desc.height = height;
    res.desc.format = format;
    res.desc.debug_name = std::string(name);
    res.external_image = image;
    res.external_view = view;
    res.current_layout = initial_layout;
    res.is_external = true;

    return RGTextureHandle(id, 0);
}

RGTextureHandle RenderGraph::create_texture(const RGTextureDesc& desc) {
    uint32_t id = static_cast<uint32_t>(m_textures.size());
    auto& res = m_textures.emplace_back();
    res.desc = desc;
    res.is_external = false;
    res.current_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    return RGTextureHandle(id, 0);
}

RGBufferHandle RenderGraph::create_buffer(const RGBufferDesc& desc) {
    uint32_t id = static_cast<uint32_t>(m_buffers.size());
    auto& res = m_buffers.emplace_back();
    res.desc = desc;
    res.is_external = false;

    return RGBufferHandle(id, 0);
}

void RenderGraph::add_resource_read(uint32_t pass_index, RGResourceHandle handle, RGResourceAccess access) {
    if (pass_index < m_pass_nodes.size()) {
        m_pass_nodes[pass_index].reads.emplace_back(handle, access);
    }
}

void RenderGraph::add_resource_write(uint32_t pass_index, RGResourceHandle handle, RGResourceAccess access) {
    if (pass_index < m_pass_nodes.size()) {
        m_pass_nodes[pass_index].writes.emplace_back(handle, access);
    }
}

void RenderGraph::set_pass_color_attachment(uint32_t pass_index, const RGAttachmentInfo& attachment) {
    if (pass_index < m_pass_nodes.size()) {
        m_pass_nodes[pass_index].color_attachments.push_back(attachment);
    }
}

void RenderGraph::set_pass_depth_attachment(uint32_t pass_index, const RGAttachmentInfo& attachment) {
    if (pass_index < m_pass_nodes.size()) {
        m_pass_nodes[pass_index].depth_attachment = attachment;
        m_pass_nodes[pass_index].has_depth_attachment = true;
    }
}

static VkImageLayout get_required_layout(RGResourceAccess access, rhi::Format format) {
    bool is_depth = (format == rhi::Format::D32_SFLOAT || format == rhi::Format::D24_UNORM_S8_UINT);
    if (access & RGResourceAccess::ColorAttachmentWrite) return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (access & RGResourceAccess::DepthAttachmentWrite) return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    if (access & RGResourceAccess::DepthAttachmentRead) return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    if (access & RGResourceAccess::ShaderRead) return is_depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (access & RGResourceAccess::ShaderWrite) return VK_IMAGE_LAYOUT_GENERAL;
    if (access & RGResourceAccess::TransferSrc) return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    if (access & RGResourceAccess::TransferDst) return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    if (access & RGResourceAccess::Present) return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

bool RenderGraph::compile() {
    // 1. Allocate / reuse physical resources for transient textures from pool
    size_t pool_tex_idx = 0;
    for (auto& res : m_textures) {
        if (!res.is_external && !res.physical_texture) {
            // Find existing matching texture in pool
            bool found = false;
            while (pool_tex_idx < m_texture_pool.size()) {
                const auto& pooled = m_texture_pool[pool_tex_idx++];
                if (pooled->get_desc().width == res.desc.width &&
                    pooled->get_desc().height == res.desc.height &&
                    pooled->get_desc().format == res.desc.format) {
                    res.physical_texture = pooled.get();
                    found = true;
                    break;
                }
            }

            if (!found) {
                rhi::TextureDesc tex_desc{};
                tex_desc.width = res.desc.width;
                tex_desc.height = res.desc.height;
                tex_desc.format = res.desc.format;
                tex_desc.usage = res.desc.usage;
                tex_desc.debug_name = res.desc.debug_name;

                auto new_tex = std::make_unique<rhi::RhiTexture>();
                if (!new_tex->init(tex_desc)) {
                    LOG_ERROR("RenderGraph", "Failed to allocate physical texture '{}'", res.desc.debug_name);
                    return false;
                }
                res.physical_texture = new_tex.get();
                m_texture_pool.push_back(std::move(new_tex));
            }
        }
    }

    // 2. Allocate / reuse physical resources for transient buffers from pool
    size_t pool_buf_idx = 0;
    for (auto& res : m_buffers) {
        if (!res.is_external && !res.physical_buffer) {
            bool found = false;
            while (pool_buf_idx < m_buffer_pool.size()) {
                const auto& pooled = m_buffer_pool[pool_buf_idx++];
                if (pooled->get_desc().size >= res.desc.size) {
                    res.physical_buffer = pooled.get();
                    found = true;
                    break;
                }
            }

            if (!found) {
                rhi::BufferDesc buf_desc{};
                buf_desc.size = res.desc.size;
                buf_desc.usage = res.desc.usage;
                buf_desc.memory_usage = res.desc.memory_usage;
                buf_desc.debug_name = res.desc.debug_name;

                auto new_buf = std::make_unique<rhi::RhiBuffer>();
                if (!new_buf->init(buf_desc)) {
                    LOG_ERROR("RenderGraph", "Failed to allocate physical buffer '{}'", res.desc.debug_name);
                    return false;
                }
                res.physical_buffer = new_buf.get();
                m_buffer_pool.push_back(std::move(new_buf));
            }
        }
    }

    m_is_compiled = true;
    return true;
}

void RenderGraph::execute(rhi::RhiCommandBuffer& cmd) {
    if (!m_is_compiled) {
        if (!compile()) return;
    }

    for (const auto& node : m_pass_nodes) {
        // 1. Image Layout Transitions for Reads
        for (const auto& [handle, access] : node.reads) {
            if (handle.type == RGResourceType::Texture && handle.id < m_textures.size()) {
                auto& tex = m_textures[handle.id];
                VkImageLayout target_layout = get_required_layout(access, tex.desc.format);
                if (target_layout != VK_IMAGE_LAYOUT_UNDEFINED && tex.current_layout != target_layout) {
                    VkImage img = get_texture_image(RGTextureHandle(handle.id, handle.version));
                    VkImageAspectFlags aspect = (tex.desc.format == rhi::Format::D32_SFLOAT || tex.desc.format == rhi::Format::D24_UNORM_S8_UINT) 
                        ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

                    cmd.transition_image_layout(img, tex.current_layout, target_layout, aspect);
                    tex.current_layout = target_layout;
                }
            }
        }

        // 2. Image Layout Transitions for Writes
        for (const auto& [handle, access] : node.writes) {
            if (handle.type == RGResourceType::Texture && handle.id < m_textures.size()) {
                auto& tex = m_textures[handle.id];
                VkImageLayout target_layout = get_required_layout(access, tex.desc.format);
                if (target_layout != VK_IMAGE_LAYOUT_UNDEFINED && tex.current_layout != target_layout) {
                    VkImage img = get_texture_image(RGTextureHandle(handle.id, handle.version));
                    VkImageAspectFlags aspect = (tex.desc.format == rhi::Format::D32_SFLOAT || tex.desc.format == rhi::Format::D24_UNORM_S8_UINT) 
                        ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

                    cmd.transition_image_layout(img, tex.current_layout, target_layout, aspect);
                    tex.current_layout = target_layout;
                }
            }
        }

        // 3. Begin Dynamic Rendering if Raster Pass
        bool is_raster_pass = !node.color_attachments.empty() || node.has_depth_attachment;
        rhi::Rect2D render_area{};

        if (is_raster_pass) {
            rhi::RenderingDesc render_desc{};

            for (const auto& att : node.color_attachments) {
                if (att.handle.id < m_textures.size()) {
                    const auto& tex = m_textures[att.handle.id];
                    render_area.width = std::max(render_area.width, tex.desc.width);
                    render_area.height = std::max(render_area.height, tex.desc.height);

                    rhi::ColorAttachmentDesc color_att{};
                    color_att.image_view = get_texture_view(att.handle);
                    color_att.image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    color_att.load_op = att.load_op;
                    color_att.store_op = att.store_op;
                    color_att.clear_color = att.clear_color;
                    render_desc.color_attachments.push_back(color_att);
                }
            }

            if (node.has_depth_attachment && node.depth_attachment.handle.id < m_textures.size()) {
                const auto& tex = m_textures[node.depth_attachment.handle.id];
                render_area.width = std::max(render_area.width, tex.desc.width);
                render_area.height = std::max(render_area.height, tex.desc.height);

                rhi::DepthAttachmentDesc depth_att{};
                depth_att.image_view = get_texture_view(node.depth_attachment.handle);
                depth_att.image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                depth_att.load_op = node.depth_attachment.load_op;
                depth_att.store_op = node.depth_attachment.store_op;
                depth_att.clear_depth = node.depth_attachment.clear_depth;
                depth_att.clear_stencil = node.depth_attachment.clear_stencil;
                render_desc.depth_attachment = depth_att;
                render_desc.has_depth = true;
            }

            render_desc.render_area = render_area;
            cmd.begin_rendering(render_desc);
        }

        // 4. Execute Pass Lambda
        RenderPassContext ctx(*this, cmd, render_area);
        if (node.execute_fn) {
            node.execute_fn(ctx);
        }

        // 5. End Dynamic Rendering
        if (is_raster_pass) {
            cmd.end_rendering();
        }
    }
}

void RenderGraph::reset() {
    m_pass_nodes.clear();
    m_textures.clear();
    m_buffers.clear();
    m_is_compiled = false;
}

VkImageView RenderGraph::get_texture_view(RGTextureHandle handle) const {
    if (handle.id < m_textures.size()) {
        const auto& res = m_textures[handle.id];
        if (res.is_external) return res.external_view;
        if (res.physical_texture) return res.physical_texture->get_view();
    }
    return VK_NULL_HANDLE;
}

VkImage RenderGraph::get_texture_image(RGTextureHandle handle) const {
    if (handle.id < m_textures.size()) {
        const auto& res = m_textures[handle.id];
        if (res.is_external) return res.external_image;
        if (res.physical_texture) return res.physical_texture->get_handle();
    }
    return VK_NULL_HANDLE;
}

VkBuffer RenderGraph::get_buffer_handle(RGBufferHandle handle) const {
    if (handle.id < m_buffers.size()) {
        const auto& res = m_buffers[handle.id];
        if (res.is_external) return res.external_buffer;
        if (res.physical_buffer) return res.physical_buffer->get_handle();
    }
    return VK_NULL_HANDLE;
}

} // namespace engine::renderer
