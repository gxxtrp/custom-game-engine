#pragma once

#include "engine/core/config.h"
#include "engine/renderer/render_feature.h"
#include "engine/renderer/lighting.h"

namespace engine::renderer {

// RenderStage::Lighting — extracts scene lights (already populated by the host
// into FrameLightState), updates the clustered lighting GPU buffers, and CPU-culls
// light/cluster assignments against the camera frustum.
class ClusteredLightingFeature final : public IRenderFeature {
public:
    ClusteredLightingFeature() = default;
    ~ClusteredLightingFeature() override;

    std::string_view get_name() const noexcept override { return "ClusteredLighting"; }
    RenderStage get_stage() const noexcept override { return RenderStage::Lighting; }

    void setup(RenderPassBuilder& builder, const SceneRenderView& view) override;
    void execute(RenderPassContext& ctx, const SceneRenderView& view) override;

private:
    bool ensure_initialized();

    ClusteredLightingSystem m_system;
    bool m_initialized{false};
};

} // namespace engine::renderer
