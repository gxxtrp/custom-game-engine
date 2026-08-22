#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/rhi/rhi_image_handle.h"
#include "engine/renderer/render_stage.h"
#include "engine/renderer/render_graph_types.h"
#include "engine/renderer/render_settings.h"
#include "engine/renderer/lighting.h"
#include <memory>
#include <string_view>
#include <vector>

namespace engine::scene {
class Scene;
} // namespace engine::scene

namespace engine::renderer {

class RenderPassBuilder;
class RenderPassContext;
class Camera;
class Mesh;
class MeshResolver;
struct FrameUniformState; // defined in scene_renderer.h (shared UBO + descriptor set)

struct SceneRenderStats {
    uint32_t draw_calls{0};
    uint32_t triangles{0};
    uint32_t visible_meshes{0};
    uint32_t culled_meshes{0};
};

// Per-frame GPU resources shared between features. Producers fill these during
// setup(); consumers read them in later stages (setup runs in stage order).
struct FrameResources {
    RGTextureHandle scene_depth;            // DepthPrePassFeature output (D32F)
    RGTextureHandle velocity_buffer;        // DepthPrePassFeature output (R32G32F)
    RGTextureHandle scene_color_hdr;        // ForwardOpaqueFeature output (RGBA16F)
    RGTextureHandle scene_color_composite;  // OIT resolve output (RGBA16F)
    RGTextureHandle scene_color_taa;        // TAA resolve output (RGBA16F)
    RGTextureHandle shadow_map;             // imported CSM depth array
    RGTextureHandle oit_accumulation;       // WBOIT accumulation target
    RGTextureHandle oit_revealage;          // WBOIT revealage target
    RGTextureHandle volumetric_fog_lut;     // VolumetricsFeature output
    RGTextureHandle bloom_result;           // BloomFeature composite output
    RGTextureHandle taa_history_read;       // TAA previous-frame color
    RGTextureHandle taa_history_write;      // TAA current-frame history write
    RGTextureHandle final_target_rg;        // imported frame output target (swapchain)
};

// Extracted light state for the current frame (filled by the host before graph
// compilation; consumed by lighting and opaque features).
struct FrameLightState {
    bool has_directional{false};
    core::Vec3 direction{0.5f, 1.0f, 0.3f};
    core::Vec3 color{1.0f, 0.98f, 0.92f};
    float intensity{1.5f};
    bool cast_shadows{true};
    uint32_t cascade_count{4};
    // Cascade data computed by the shadow feature during setup.
    core::Vec4 cascade_splits{10.0f, 25.0f, 50.0f, 100.0f};
    core::Mat4 cascade_view_proj[4]{};
    bool cascades_valid{false};
    std::vector<GPUPointLight> point_lights;
    std::vector<GPUSpotLight> spot_lights;
};

// Non-owning service bundle the host provides to every feature.
struct RenderFeatureServices {
    MeshResolver* meshes{nullptr};
    SceneRenderStats* stats{nullptr};
    const GraphicsSettings* settings{nullptr};
    FrameLightState* lights{nullptr};
    FrameResources* resources{nullptr};
    core::Mat4 prev_view_proj{core::Mat4::identity()};
    uint32_t frame_index{0};
    // CPU-side adapted exposure (updated by AutoExposureFeature after readback).
    float adapted_exposure{1.0f};
    // Shared frame-level GPU objects (frame UBO + CSM sampler descriptor set).
    FrameUniformState* frame_uniforms{nullptr};
};

// Frame description passed to every feature's setup()/execute().
struct SceneRenderView {
    const scene::Scene& scene;
    const Camera& camera;
    rhi::RHIImageHandle final_target;
    uint32_t viewport_width;
    uint32_t viewport_height;
    float delta_time;
    RenderFeatureServices* services;
};

// Seam implemented by every render pass (built-in, tool, or third-party plugin).
// Features declare transient resources in setup() and record GPU commands in
// execute(). Features are registered with SceneRenderer and execute in strict
// RenderStage order; within a stage, registration order is preserved.
class IRenderFeature {
public:
    virtual ~IRenderFeature() = default;

    virtual std::string_view get_name() const noexcept = 0;
    virtual RenderStage get_stage() const noexcept = 0;

    // Declares pass inputs, outputs, and transient attachments to the RenderGraph.
    virtual void setup(RenderPassBuilder& builder, const SceneRenderView& view) = 0;

    // Records draw/compute commands into the graph's active render pass.
    virtual void execute(RenderPassContext& ctx, const SceneRenderView& view) = 0;

    // Optional intra-stage ordering (lower executes first). Default 0.
    virtual int32_t get_priority() const noexcept { return 0; }

    // Called by the host after the frame's GPU work completes (fence-waited).
    // Used for GPU readback (auto exposure), history ping-pong (TAA), etc.
    virtual void post_frame(const SceneRenderView& view) { (void)view; }
};

} // namespace engine::renderer
