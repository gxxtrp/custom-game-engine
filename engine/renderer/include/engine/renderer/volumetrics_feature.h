#pragma once

#include "engine/core/config.h"
#include "engine/renderer/render_feature.h"
#include "engine/renderer/volumetrics.h"
#include "engine/rhi/rhi_pipeline.h"
#include "engine/rhi/rhi_texture.h"
#include "engine/rhi/rhi_descriptor.h"

namespace engine::renderer {

// RenderStage::Lighting — raymarched volumetric fog. Samples the scene depth
// buffer and writes a half-resolution scattering/transmittance LUT which the
// post-process composite applies to the final image.
class VolumetricsFeature final : public IRenderFeature {
public:
    VolumetricsFeature() = default;

    std::string_view get_name() const noexcept override { return "VolumetricFog"; }
    RenderStage get_stage() const noexcept override { return RenderStage::Lighting; }

    void setup(RenderPassBuilder& builder, const SceneRenderView& view) override;
    void execute(RenderPassContext& ctx, const SceneRenderView& view) override;

private:
    bool ensure_pipeline();

    rhi::RhiShaderModule m_vert_shader;
    rhi::RhiShaderModule m_frag_shader;
    rhi::RhiGraphicsPipeline m_pipeline;
    rhi::RhiSampler m_depth_sampler;
    rhi::RhiDescriptorSetLayout m_set_layout;
    rhi::RhiDescriptorPool m_pool;
    rhi::RhiDescriptorSet m_descriptor;
    bool m_initialized{false};
    VkImageView m_bound_depth_view{VK_NULL_HANDLE};
};

} // namespace engine::renderer
