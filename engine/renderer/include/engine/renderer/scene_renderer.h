#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/assets/uuid.h"
#include "engine/rhi/rhi_pipeline.h"
#include "engine/rhi/rhi_command_buffer.h"
#include "engine/scene/scene.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/render_graph.h"
#include "engine/renderer/render_settings.h"
#include "engine/renderer/shadow_map.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>

namespace engine::renderer {

struct MeshPushConstants {
    core::Mat4 view_proj;             // 64 bytes (offset 0)
    core::Mat4 model;                 // 64 bytes (offset 64)
    core::Vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};            // 16 bytes (offset 128)
    core::Vec4 light_dir_intensity{0.5f, 1.0f, 0.3f, 1.5f};   // 16 bytes (offset 144)
    core::Vec4 light_color_ambient{1.0f, 0.98f, 0.92f, 0.15f};// 16 bytes (offset 160)
    core::Vec4 camera_pos_roughness{0.0f, 0.0f, 0.0f, 0.5f};  // 16 bytes (offset 176)
    core::Vec4 material_params{0.0f, 0.0f, 0.0f, 0.0f};       // 16 bytes (offset 192): x=metallic, y=emissive_strength
};

struct ShadowPushConstants {
    core::Mat4 light_view_proj;       // 64 bytes (offset 0)
    core::Mat4 model;                 // 64 bytes (offset 64)
};

struct TonemapPushConstants {
    float exposure{1.0f};
    int32_t tone_mapper{0};           // 0=ACES, 1=AgX, 2=Reinhard, 3=Linear
    float vignette_intensity{0.25f};
    float pad{0.0f};
};

struct SceneRenderStats {
    uint32_t draw_calls{0};
    uint32_t triangles{0};
    uint32_t visible_meshes{0};
    uint32_t culled_meshes{0};
};

struct RenderCamera {
    core::Mat4 view{core::Mat4::identity()};
    core::Mat4 proj{core::Mat4::identity()};
    core::Mat4 view_proj{core::Mat4::identity()};
    core::Vec3 position{0.0f, 0.0f, 0.0f};
    core::Frustum frustum{};
    float fov_rad{core::math::deg_to_rad(60.0f)};
    float aspect{16.0f / 9.0f};
    float near_z{0.1f};
    float far_z{1000.0f};
};

class SceneRenderer {
public:
    static SceneRenderer& instance();

    bool init(rhi::Format sdr_format = rhi::Format::R8G8B8A8_UNORM,
              rhi::Format hdr_format = rhi::Format::R16G16B16A16_SFLOAT,
              rhi::Format depth_format = rhi::Format::D32_SFLOAT);
    void shutdown();

    // Centralized dynamic DAG pipeline builder
    RGTextureHandle setup_render_pipeline(RenderGraph& rg,
                                         scene::Scene& scene,
                                         const RenderCamera& camera,
                                         const GraphicsSettings& settings,
                                         RGTextureHandle output_sdr_target,
                                         uint32_t width,
                                         uint32_t height);

    // Direct Pass Renderers
    void render_scene(rhi::RhiCommandBuffer& cmd,
                      scene::Scene& scene,
                      const core::Mat4& view_proj,
                      const core::Vec3& camera_pos,
                      const core::Frustum& frustum,
                      bool enable_frustum_culling,
                      const rhi::Viewport& viewport,
                      const rhi::Rect2D& scissor);

    void render_shadow_pass(rhi::RhiCommandBuffer& cmd,
                            scene::Scene& scene,
                            const core::Mat4& light_view_proj,
                            const rhi::Viewport& viewport,
                            const rhi::Rect2D& scissor);

    void render_tonemapping(rhi::RhiCommandBuffer& cmd,
                            const TonemapPushConstants& pc,
                            const rhi::Viewport& viewport,
                            const rhi::Rect2D& scissor);

    std::shared_ptr<Mesh> get_or_create_primitive(std::string_view name);
    void register_mesh(const assets::UUID& uuid, std::shared_ptr<Mesh> mesh);
    std::shared_ptr<Mesh> get_mesh(const assets::UUID& uuid);

    const SceneRenderStats& get_stats() const { return m_stats; }
    rhi::RhiGraphicsPipeline& get_pipeline() { return m_pipeline; }
    CascadedShadowMap& get_shadow_map() { return m_shadow_map; }
    bool is_initialized() const { return m_initialized; }

private:
    SceneRenderer() = default;
    ~SceneRenderer();

    // 1. Mesh Forward Pipeline (HDR)
    rhi::RhiShaderModule m_vert_shader;
    rhi::RhiShaderModule m_frag_shader;
    rhi::RhiGraphicsPipeline m_pipeline;

    // 2. Shadow Depth Pipeline
    rhi::RhiShaderModule m_shadow_vert_shader;
    rhi::RhiShaderModule m_shadow_frag_shader;
    rhi::RhiGraphicsPipeline m_shadow_pipeline;

    // 3. Fullscreen Tonemap Pipeline
    rhi::RhiShaderModule m_tonemap_vert_shader;
    rhi::RhiShaderModule m_tonemap_frag_shader;
    rhi::RhiGraphicsPipeline m_tonemap_pipeline;

    CascadedShadowMap m_shadow_map;

    std::unordered_map<std::string, std::shared_ptr<Mesh>> m_primitive_meshes;
    std::unordered_map<assets::UUID, std::shared_ptr<Mesh>> m_mesh_registry;

    SceneRenderStats m_stats{};
    bool m_initialized{false};
};

} // namespace engine::renderer
