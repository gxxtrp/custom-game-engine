#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/renderer/render_graph_types.h"
#include "engine/rhi/rhi_command_buffer.h"
#include <functional>
#include <vector>
#include <memory>

namespace engine::renderer {

class RenderGraph;

struct RGAttachmentInfo {
    RGTextureHandle handle;
    VkAttachmentLoadOp load_op{VK_ATTACHMENT_LOAD_OP_CLEAR};
    VkAttachmentStoreOp store_op{VK_ATTACHMENT_STORE_OP_STORE};
    core::Vec4 clear_color{0.0f, 0.0f, 0.0f, 1.0f};
    float clear_depth{1.0f};
    uint32_t clear_stencil{0};
};

class RenderPassBuilder {
public:
    explicit RenderPassBuilder(RenderGraph& graph, uint32_t pass_index)
        : m_graph(graph), m_pass_index(pass_index) {}

    RGTextureHandle create_texture(const RGTextureDesc& desc);
    RGBufferHandle create_buffer(const RGBufferDesc& desc);

    void read_texture(RGTextureHandle handle, RGResourceAccess access = RGResourceAccess::ShaderRead);
    void write_texture(RGTextureHandle handle, RGResourceAccess access = RGResourceAccess::ColorAttachmentWrite);

    void read_buffer(RGBufferHandle handle, RGResourceAccess access = RGResourceAccess::ShaderRead);
    void write_buffer(RGBufferHandle handle, RGResourceAccess access = RGResourceAccess::ShaderWrite);

    void set_color_attachment(uint32_t slot, RGTextureHandle handle, 
                             VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR, 
                             VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE, 
                             core::Vec4 clear_color = {0.0f, 0.0f, 0.0f, 1.0f});

    void set_depth_attachment(RGTextureHandle handle, 
                             VkAttachmentLoadOp load_op = VK_ATTACHMENT_LOAD_OP_CLEAR, 
                             VkAttachmentStoreOp store_op = VK_ATTACHMENT_STORE_OP_STORE, 
                             float clear_depth = 1.0f);

private:
    RenderGraph& m_graph;
    uint32_t m_pass_index;
};

class RenderPassContext {
public:
    RenderPassContext(RenderGraph& graph, rhi::RhiCommandBuffer& cmd, rhi::Rect2D render_area)
        : m_graph(graph), m_cmd(cmd), m_render_area(render_area) {}

    rhi::RhiCommandBuffer& get_command_buffer() { return m_cmd; }
    rhi::Rect2D get_render_area() const { return m_render_area; }

    VkImageView get_texture_view(RGTextureHandle handle) const;
    VkImage get_texture_image(RGTextureHandle handle) const;
    VkBuffer get_buffer_handle(RGBufferHandle handle) const;

private:
    RenderGraph& m_graph;
    rhi::RhiCommandBuffer& m_cmd;
    rhi::Rect2D m_render_area;
};

struct RenderPassNode {
    std::string name;
    uint32_t index{0};

    std::vector<std::pair<RGResourceHandle, RGResourceAccess>> reads;
    std::vector<std::pair<RGResourceHandle, RGResourceAccess>> writes;

    std::vector<RGAttachmentInfo> color_attachments;
    RGAttachmentInfo depth_attachment{};
    bool has_depth_attachment{false};

    std::function<void(RenderPassContext&)> execute_fn;
};

} // namespace engine::renderer
