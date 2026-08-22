#pragma once

#include "engine/core/config.h"
#include "engine/renderer/render_feature.h"
#include "engine/renderer/post_process.h"
#include "engine/rhi/rhi_pipeline.h"
#include "engine/rhi/rhi_descriptor.h"
#include "engine/rhi/rhi_texture.h"

namespace engine::renderer {

// RenderStage::PostProcessStack (final) — color grading, bloom & volumetric fog
// application, ACES/AgX/Reinhard tonemapping, vignette and film grain. Writes the
// frame's final SDR target.
class PostProcessCompositeFeature final : public IRenderFeature {
public:
    PostProcessCompositeFeature() = default;

    std::string_view get_name() const noexcept override { return "PostProcessComposite"; }
    RenderStage get_stage() const noexcept override { return RenderStage::PostProcessStack; }

    void setup(RenderPassBuilder& builder, const SceneRenderView& view) override;
    void execute(RenderPassContext& ctx, const SceneRenderView& view) override;

private:
    bool ensure_pipeline(rhi::Format target_format);

    rhi::RhiShaderModule m_vert_shader;
    rhi::RhiShaderModule m_frag_shader;
    rhi::RhiGraphicsPipeline m_pipeline;
    rhi::Format m_pipeline_format{rhi::Format::Undefined};
    rhi::RhiSampler m_linear_sampler;
    rhi::RhiTexture m_dummy_black; // 1x1 black (disabled bloom)
    rhi::RhiTexture m_dummy_transparent; // 1x1 (0,0,0,1) = fog identity
    rhi::RhiDescriptorSetLayout m_set_layout;
    rhi::RhiDescriptorPool m_pool;
    rhi::RhiDescriptorSet m_descriptor;
    bool m_initialized{false};
    VkImageView m_bound_scene{VK_NULL_HANDLE};
    VkImageView m_bound_bloom{VK_NULL_HANDLE};
    VkImageView m_bound_fog{VK_NULL_HANDLE};
};

} // namespace engine::renderer
