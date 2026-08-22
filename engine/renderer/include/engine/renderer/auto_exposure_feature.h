#pragma once

#include "engine/core/config.h"
#include "engine/renderer/render_feature.h"
#include "engine/renderer/auto_exposure.h"
#include "engine/rhi/rhi_pipeline.h"
#include "engine/rhi/rhi_descriptor.h"

namespace engine::renderer {

// RenderStage::PostProcessStack — GPU log-luminance histogram (compute) with
// CPU-side temporal adaptation and 1-frame-latency readback. The adapted exposure
// is published into RenderFeatureServices::adapted_exposure for the composite pass.
class AutoExposureFeature final : public IRenderFeature {
public:
    AutoExposureFeature() = default;

    std::string_view get_name() const noexcept override { return "AutoExposure"; }
    RenderStage get_stage() const noexcept override { return RenderStage::PostProcessStack; }

    void setup(RenderPassBuilder& builder, const SceneRenderView& view) override;
    void execute(RenderPassContext& ctx, const SceneRenderView& view) override;
    void post_frame(const SceneRenderView& view) override;

private:
    bool ensure_pipeline();

    AutoExposureSystem m_system;
    rhi::RhiShaderModule m_compute_shader;
    rhi::RhiComputePipeline m_pipeline;
    rhi::RhiDescriptorSetLayout m_set_layout;
    rhi::RhiDescriptorPool m_pool;
    rhi::RhiDescriptorSet m_descriptor;
    bool m_initialized{false};
    bool m_histogram_written{false};
    VkImageView m_bound_view{VK_NULL_HANDLE};
};

} // namespace engine::renderer
