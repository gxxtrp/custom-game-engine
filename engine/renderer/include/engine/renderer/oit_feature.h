#pragma once

#include "engine/core/config.h"
#include "engine/renderer/render_feature.h"
#include "engine/renderer/oit.h"
#include "engine/rhi/rhi_pipeline.h"
#include "engine/rhi/rhi_descriptor.h"

namespace engine::renderer {

// RenderStage::Translucent — Weighted Blended Order-Independent Transparency.
// Pass 1 accumulates weighted premultiplied color/revealage of transparent
// geometry; pass 2 resolves the accumulation over the opaque HDR color into the
// composite scene target.
class OitFeature final : public IRenderFeature {
public:
    OitFeature() = default;

    std::string_view get_name() const noexcept override { return "WboitOIT"; }
    RenderStage get_stage() const noexcept override { return RenderStage::Translucent; }

    void setup(RenderPassBuilder& builder, const SceneRenderView& view) override;
    void execute(RenderPassContext& ctx, const SceneRenderView& view) override;

private:
    bool ensure_pipelines(const SceneRenderView& view);
    void execute_accumulate(RenderPassContext& ctx, const SceneRenderView& view);
    void execute_resolve(RenderPassContext& ctx, const SceneRenderView& view);

    // Accumulation pipelines (transparent shading -> accumulation + revealage)
    rhi::RhiShaderModule m_wboit_vert_shader;
    rhi::RhiShaderModule m_wboit_frag_shader;
    rhi::RhiGraphicsPipeline m_accumulate_pipeline;

    // Resolve pipeline (fullscreen composite over opaque)
    rhi::RhiShaderModule m_resolve_vert_shader;
    rhi::RhiShaderModule m_resolve_frag_shader;
    rhi::RhiGraphicsPipeline m_resolve_pipeline;

    rhi::RhiSampler m_linear_sampler;
    rhi::RhiDescriptorSetLayout m_resolve_set_layout;
    rhi::RhiDescriptorPool m_resolve_pool;
    rhi::RhiDescriptorSet m_resolve_descriptor;

    bool m_initialized{false};
    VkImageView m_bound_opaque_view{VK_NULL_HANDLE};
    VkImageView m_bound_accum_view{VK_NULL_HANDLE};
    VkImageView m_bound_reveal_view{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_frame_layout{VK_NULL_HANDLE};
};

} // namespace engine::renderer
