#pragma once

#include "engine/core/config.h"
#include "engine/renderer/render_feature.h"
#include "engine/rhi/rhi_pipeline.h"

namespace engine::renderer {

// RenderStage::DepthPrePass — renders all visible opaque geometry into the scene
// depth buffer (early-Z for the forward pass) and outputs per-pixel motion vectors
// (previous-frame reprojection) into an R32G32 velocity target for TAA.
class DepthPrePassFeature final : public IRenderFeature {
public:
    DepthPrePassFeature() = default;

    std::string_view get_name() const noexcept override { return "DepthPrePass"; }
    RenderStage get_stage() const noexcept override { return RenderStage::DepthPrePass; }

    void setup(RenderPassBuilder& builder, const SceneRenderView& view) override;
    void execute(RenderPassContext& ctx, const SceneRenderView& view) override;

private:
    bool ensure_pipeline(VkDescriptorSetLayout frame_set_layout);

    rhi::RhiShaderModule m_vert_shader;
    rhi::RhiShaderModule m_frag_shader;
    rhi::RhiGraphicsPipeline m_pipeline;
    VkDescriptorSetLayout m_bound_layout{VK_NULL_HANDLE};
    bool m_initialized{false};
};

} // namespace engine::renderer
