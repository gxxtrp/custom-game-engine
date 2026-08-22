#pragma once

#include "engine/core/config.h"
#include "engine/renderer/render_feature.h"
#include "engine/rhi/rhi_pipeline.h"

namespace engine::renderer {

// RenderStage::Opaque — forward-shaded PBR rasterization of opaque geometry into
// the HDR scene color target. Consumes the depth prepass (LOAD + equal test) and
// samples the CSM array for cascade-shadowed direct lighting.
class ForwardOpaqueFeature final : public IRenderFeature {
public:
    ForwardOpaqueFeature() = default;

    std::string_view get_name() const noexcept override { return "ForwardOpaque"; }
    RenderStage get_stage() const noexcept override { return RenderStage::Opaque; }

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
