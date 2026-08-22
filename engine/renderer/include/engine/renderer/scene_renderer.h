#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/assets/uuid.h"
#include "engine/rhi/rhi_image_handle.h"
#include "engine/rhi/rhi_sync.h"
#include "engine/rhi/rhi_command_buffer.h"
#include "engine/rhi/rhi_buffer.h"
#include "engine/rhi/rhi_texture.h"
#include "engine/rhi/rhi_descriptor.h"
#include "engine/rhi/rhi_pipeline.h"
#include "engine/rhi/rhi_context.h"
#include "engine/scene/scene.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/mesh_resolver.h"
#include "engine/renderer/render_graph.h"
#include "engine/renderer/render_settings.h"
#include "engine/renderer/render_feature.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>
#include <vector>

namespace engine::renderer {

// ==========================================
// Shared per-frame GPU uniform state
// ==========================================

struct alignas(16) FrameUniforms {
    core::Mat4 view_proj;                       // offset 0
    core::Mat4 prev_view_proj;                  // offset 64
    core::Mat4 view;                            // offset 128
    core::Vec4 camera_pos;                      // offset 192
    core::Vec4 dir_light_dir_intensity;         // offset 208
    core::Vec4 dir_light_color_ambient;         // offset 224
    core::Vec4 cascade_splits;                  // offset 240
    core::Mat4 cascade_view_proj[4];            // offset 256 (4 x 64)
    core::Vec4 shadow_params;                   // offset 512: x=bias, y=enabled, zw=unused
};

static_assert(sizeof(FrameUniforms) % 16 == 0, "FrameUniforms must be 16-byte aligned");

struct alignas(16) MeshPushConstants {
    core::Mat4 model;                           // offset 0
    core::Vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f}; // offset 64
    core::Vec4 material_params{0.0f, 0.0f, 0.0f, 0.0f}; // offset 80: x=metallic, y=emissive_strength, z=roughness_override
};

// Shared frame-level GPU objects (owned by SceneRenderer, exposed to features
// through RenderFeatureServices). The frame descriptor set binds the per-frame
// uniform buffer (b0) and the CSM depth array sampler (b1).
struct FrameUniformState {
    FrameUniforms cpu_data{};
    rhi::RhiBuffer ubo;
    rhi::RhiSampler shadow_sampler;
    rhi::RhiDescriptorSetLayout set_layout;
    rhi::RhiDescriptorPool pool;
    rhi::RhiDescriptorSet descriptor;
    bool valid{false};
};

// ==========================================
// SceneRenderer — open, stage-ordered pipeline host
// ==========================================

class SceneRenderer {
public:
    explicit SceneRenderer(rhi::RhiContext* rhi);
    ~SceneRenderer();

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    // ----- Feature registration seam -----
    void register_feature(std::shared_ptr<IRenderFeature> feature);
    void unregister_feature(std::string_view feature_name);
    IRenderFeature* get_feature(std::string_view feature_name) const;

    // ----- Main render entry point -----
    // Compiles all active features into the internal RenderGraph in strict stage
    // order, records GPU commands, and submits the frame to the graphics queue.
    void render(const scene::Scene& scene, const Camera& camera, rhi::RHIImageHandle output_target);

    // ----- Host configuration -----
    void set_settings(const GraphicsSettings& settings) { m_settings = settings; }
    const GraphicsSettings& get_settings() const { return m_settings; }
    void set_delta_time(float dt) { m_delta_time = dt; }
    const SceneRenderStats& get_stats() const { return m_stats; }

    // Mesh registry (delegates to MeshResolver)
    void register_mesh(const assets::UUID& uuid, std::shared_ptr<Mesh> mesh);
    std::shared_ptr<Mesh> get_mesh(const assets::UUID& uuid) const;

    // Semaphore signaled when the frame's GPU work completes (present waits on this).
    VkSemaphore get_render_finished_semaphore() const;

    // Configures the number of render-finished semaphore slots. Must be >= the
    // swapchain image count so a present wait is never re-signaled before the
    // swapchain retires it. Call before the first render(); if the GPU is
    // already initialized, the caller must guarantee the queue is idle
    // (wait_idle) before invoking, and slots are destroyed/recreated.
    void set_frames_in_flight(uint32_t count);

    // Test/tool seam: compiles the active feature set into `rg` as stage-ordered
    // passes. GPU-free when features declare no transient resources; the graph can
    // then be compiled/executed against a default-constructed command buffer.
    void build_feature_graph(RenderGraph& rg,
                             const scene::Scene& scene,
                             const Camera& camera,
                             rhi::RHIImageHandle output_target);

private:
    void compile_and_execute_graph(RenderGraph& rg, const SceneRenderView& view);
    void extract_frame_lights(const scene::Scene& scene);
    void extract_point_and_spot_lights(const scene::Scene& scene);
    bool ensure_gpu_ready();
    void register_default_features();
    void reset_frame_resources();
    void upload_frame_uniforms(const SceneRenderView& view);

    rhi::RhiContext* m_rhi{nullptr};
    std::unordered_map<RenderStage, std::vector<std::shared_ptr<IRenderFeature>>> m_features_by_stage;
    std::unordered_map<std::string, std::shared_ptr<IRenderFeature>> m_features_by_name;

    GraphicsSettings m_settings{};
    float m_delta_time{1.0f / 60.0f};
    SceneRenderStats m_stats{};
    MeshResolver m_mesh_resolver;
    FrameLightState m_lights{};
    FrameResources m_frame_resources{};
    FrameUniformState m_frame_uniforms{};
    Camera m_working_camera{};
    core::Mat4 m_prev_view_proj{core::Mat4::identity()};
    uint32_t m_frame_index{0};
    float m_adapted_exposure{1.0f};

    // GPU execution state
    RenderGraph m_render_graph;
    rhi::RhiCommandPool m_cmd_pool;
    rhi::RhiCommandBuffer m_cmd_buffer;
    rhi::RhiFence m_frame_fence;
    std::vector<rhi::RhiSemaphore> m_render_finished_semaphores;
    uint32_t m_frames_in_flight{3}; // sized from the swapchain image count
    uint32_t m_render_finished_index{0}; // semaphore signaled by the last frame
    bool m_gpu_ready{false};

    friend class AutoExposureFeature;
    friend class CascadedShadowFeature;
    friend class PostProcessCompositeFeature;
};

} // namespace engine::renderer
