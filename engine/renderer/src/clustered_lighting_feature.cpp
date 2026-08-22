#include "engine/renderer/clustered_lighting_feature.h"
#include "engine/renderer/camera.h"
#include "engine/core/log.h"

namespace engine::renderer {

ClusteredLightingFeature::~ClusteredLightingFeature() = default;

bool ClusteredLightingFeature::ensure_initialized() {
    if (m_initialized) return true;
    if (!m_system.init()) {
        LOG_FATAL("ClusteredLighting", "Failed to initialize clustered lighting system");
        return false;
    }
    m_initialized = true;
    return true;
}

void ClusteredLightingFeature::setup(RenderPassBuilder& builder, const SceneRenderView& view) {
    (void)builder;
    if (!view.services || !view.services->lights) return;
    if (!ensure_initialized()) return;

    FrameLightState& lights = *view.services->lights;

    // 1. Directional light (cascade matrices already computed by the shadow feature).
    GPUDirectionalLight dir{};
    dir.direction = lights.direction;
    dir.intensity = lights.intensity;
    dir.color = lights.color;
    dir.cascade_count = std::min(lights.cascade_count, 4u);
    for (uint32_t i = 0; i < 4; ++i) {
        dir.cascade_view_proj[i] = lights.cascade_view_proj[i];
    }
    dir.cascade_splits = lights.cascade_splits;
    dir.shadow_map_idx = (lights.cascades_valid && lights.cast_shadows) ? 0 : UINT32_MAX;
    m_system.update_directional_light(dir);

    // 2. Point & spot lights (host-extracted).
    m_system.set_point_lights(lights.point_lights);
    m_system.set_spot_lights(lights.spot_lights);

    // 3. CPU cluster culling against the view matrix.
    m_system.cull_lights_cpu(view.camera.view, view.camera.proj);
}

void ClusteredLightingFeature::execute(RenderPassContext& ctx, const SceneRenderView& view) {
    (void)ctx;
    (void)view;
    // All GPU buffer uploads happen in setup(); this pass records no commands.
}

} // namespace engine::renderer
