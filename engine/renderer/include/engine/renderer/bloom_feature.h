#pragma once

#include "engine/core/config.h"
#include "engine/renderer/render_feature.h"
#include "engine/renderer/bloom.h"
#include "engine/rhi/rhi_pipeline.h"
#include "engine/rhi/rhi_descriptor.h"
#include <vector>
#include <array>

namespace engine::renderer {

// RenderStage::PostProcessStack — bright-pass bloom: prefilter (soft-knee +
// Karis average), 5-step downsample chain, 5-step tent upsample chain, outputting
// a half-resolution bloom texture consumed by the composite pass.
class BloomFeature final : public IRenderFeature {
public:
    BloomFeature() = default;

    std::string_view get_name() const noexcept override { return "Bloom"; }
    RenderStage get_stage() const noexcept override { return RenderStage::PostProcessStack; }

    void setup(RenderPassBuilder& builder, const SceneRenderView& view) override;
    void execute(RenderPassContext& ctx, const SceneRenderView& view) override;

private:
    struct BloomPassState {
        RGTextureHandle input_a;
        RGTextureHandle input_b; // valid only for upsample passes
        RGTextureHandle output;
        int32_t type{0}; // 0 = prefilter, 1 = downsample, 2 = upsample
        uint32_t width{0};
        uint32_t height{0};
        rhi::RhiDescriptorSet* descriptor{nullptr}; // persistent, allocated once
        VkImageView bound_a{VK_NULL_HANDLE};
        VkImageView bound_b{VK_NULL_HANDLE};
    };

    static constexpr size_t MAX_BLOOM_PASSES = 11; // 1 prefilter + 5 down + 5 up

    bool ensure_pipelines();
    void execute_pass(RenderPassContext& ctx, const SceneRenderView& view, BloomPassState& pass);

    rhi::RhiShaderModule m_vert_shader;
    rhi::RhiShaderModule m_prefilter_frag;
    rhi::RhiShaderModule m_downsample_frag;
    rhi::RhiShaderModule m_upsample_frag;
    rhi::RhiGraphicsPipeline m_prefilter_pipeline;
    rhi::RhiGraphicsPipeline m_downsample_pipeline;
    rhi::RhiGraphicsPipeline m_upsample_pipeline;
    rhi::RhiSampler m_linear_sampler;
    rhi::RhiDescriptorSetLayout m_set_layout;
    rhi::RhiDescriptorPool m_pool;

    std::vector<BloomPassState> m_passes;
    std::vector<RGTextureHandle> m_mips; // 6 levels (half-resolution each)
    std::array<rhi::RhiDescriptorSet, MAX_BLOOM_PASSES> m_pass_sets;
    BloomParams m_params{}; // threshold, soft knee, intensity, filter radius
    bool m_initialized{false};
};

} // namespace engine::renderer
