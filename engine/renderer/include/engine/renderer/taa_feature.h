#pragma once

#include "engine/core/config.h"
#include "engine/renderer/render_feature.h"
#include "engine/renderer/taa.h"
#include "engine/rhi/rhi_pipeline.h"
#include "engine/rhi/rhi_descriptor.h"

namespace engine::renderer {

// RenderStage::PostProcessStack — jittered temporal anti-aliasing with motion
// vector reprojection and YCoCg neighborhood clamping. Writes the resolved image
// into scene_color_taa and ping-pongs persistent history textures.
class TaaFeature final : public IRenderFeature {
public:
    TaaFeature() = default;

    std::string_view get_name() const noexcept override { return "TaaResolve"; }
    RenderStage get_stage() const noexcept override { return RenderStage::PostProcessStack; }

    void setup(RenderPassBuilder& builder, const SceneRenderView& view) override;
    void execute(RenderPassContext& ctx, const SceneRenderView& view) override;
    void post_frame(const SceneRenderView& view) override;

private:
    bool ensure_pipeline();

    TaaSystem m_system;
    rhi::RhiShaderModule m_vert_shader;
    rhi::RhiShaderModule m_frag_shader;
    rhi::RhiGraphicsPipeline m_pipeline;
    rhi::RhiSampler m_linear_sampler;
    rhi::RhiDescriptorSetLayout m_set_layout;
    rhi::RhiDescriptorPool m_pool;
    rhi::RhiDescriptorSet m_descriptor;
    bool m_initialized{false};
    VkImageView m_bound_current{VK_NULL_HANDLE};
    VkImageView m_bound_history{VK_NULL_HANDLE};
    VkImageView m_bound_velocity{VK_NULL_HANDLE};
};

} // namespace engine::renderer
