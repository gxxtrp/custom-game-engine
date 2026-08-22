#pragma once

#include "engine/core/config.h"
#include "engine/renderer/render_feature.h"
#include "engine/renderer/shadow_map.h"
#include "engine/rhi/rhi_pipeline.h"
#include "engine/rhi/rhi_command_buffer.h"

namespace engine::renderer {

// Cascaded shadow evaluation. Registered in the DepthPrePass stage with negative
// priority: this renderer is forward-shaded, so shadow maps must be rasterized
// before the opaque pass samples them.
class CascadedShadowFeature final : public IRenderFeature {
public:
    CascadedShadowFeature() = default;
    ~CascadedShadowFeature() override;

    std::string_view get_name() const noexcept override { return "CascadedShadow"; }
    RenderStage get_stage() const noexcept override { return RenderStage::DepthPrePass; }
    int32_t get_priority() const noexcept override { return -1; }

    void setup(RenderPassBuilder& builder, const SceneRenderView& view) override;
    void execute(RenderPassContext& ctx, const SceneRenderView& view) override;

private:
    bool ensure_initialized(const SceneRenderView& view);
    void render_cascade(rhi::RhiCommandBuffer& cmd, uint32_t cascade, const SceneRenderView& view);

    CascadedShadowMap m_csm;
    rhi::RhiShaderModule m_vert_shader;
    rhi::RhiShaderModule m_frag_shader;
    rhi::RhiGraphicsPipeline m_pipeline;
    rhi::RhiTexture m_dummy_shadow_texture; // 1x1 white depth for shadows-disabled path
    uint32_t m_active_cascades{0};
    bool m_initialized{false};
};

} // namespace engine::renderer
