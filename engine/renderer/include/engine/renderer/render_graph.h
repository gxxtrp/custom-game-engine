#pragma once

#include "engine/core/config.h"
#include "engine/renderer/render_graph_types.h"
#include "engine/renderer/render_pass.h"
#include "engine/rhi/rhi_buffer.h"
#include "engine/rhi/rhi_texture.h"
#include "engine/rhi/rhi_command_buffer.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

namespace engine::renderer {

struct RGTextureResource {
    RGTextureDesc desc;
    rhi::RhiTexture* physical_texture{nullptr};
    VkImage external_image{VK_NULL_HANDLE};
    VkImageView external_view{VK_NULL_HANDLE};
    VkImageLayout current_layout{VK_IMAGE_LAYOUT_UNDEFINED};
    bool is_external{false};
};

struct RGBufferResource {
    RGBufferDesc desc;
    rhi::RhiBuffer* physical_buffer{nullptr};
    VkBuffer external_buffer{VK_NULL_HANDLE};
    bool is_external{false};
};

class RenderGraph {
public:
    RenderGraph() = default;
    ~RenderGraph();

    template<typename SetupFn, typename ExecFn>
    void add_pass(std::string_view name, SetupFn&& setup, ExecFn&& execute) {
        uint32_t pass_index = static_cast<uint32_t>(m_pass_nodes.size());
        auto& node = m_pass_nodes.emplace_back();
        node.name = std::string(name);
        node.index = pass_index;
        node.execute_fn = std::forward<ExecFn>(execute);

        RenderPassBuilder builder(*this, pass_index);
        setup(builder);
    }

    RGTextureHandle import_texture(std::string_view name, 
                                  VkImage image, 
                                  VkImageView view, 
                                  uint32_t width, 
                                  uint32_t height, 
                                  rhi::Format format, 
                                  VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED);

    RGTextureHandle create_texture(const RGTextureDesc& desc);
    RGBufferHandle create_buffer(const RGBufferDesc& desc);

    void add_resource_read(uint32_t pass_index, RGResourceHandle handle, RGResourceAccess access);
    void add_resource_write(uint32_t pass_index, RGResourceHandle handle, RGResourceAccess access);
    void set_pass_color_attachment(uint32_t pass_index, const RGAttachmentInfo& attachment);
    void set_pass_depth_attachment(uint32_t pass_index, const RGAttachmentInfo& attachment);

    bool compile();
    void execute(rhi::RhiCommandBuffer& cmd);
    void reset();
    void destroy();

    VkImageView get_texture_view(RGTextureHandle handle) const;
    VkImage get_texture_image(RGTextureHandle handle) const;
    VkBuffer get_buffer_handle(RGBufferHandle handle) const;

    size_t get_pass_count() const { return m_pass_nodes.size(); }

private:
    std::vector<RenderPassNode> m_pass_nodes;
    std::vector<RGTextureResource> m_textures;
    std::vector<RGBufferResource> m_buffers;

    // Physical resource pool for transient textures/buffers
    std::vector<std::unique_ptr<rhi::RhiTexture>> m_texture_pool;
    std::vector<std::unique_ptr<rhi::RhiBuffer>> m_buffer_pool;

    bool m_is_compiled{false};
};

} // namespace engine::renderer
