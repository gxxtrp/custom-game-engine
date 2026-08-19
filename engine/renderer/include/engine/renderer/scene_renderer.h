#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/assets/uuid.h"
#include "engine/rhi/rhi_pipeline.h"
#include "engine/rhi/rhi_command_buffer.h"
#include "engine/scene/scene.h"
#include "engine/renderer/mesh.h"
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
};

struct SceneRenderStats {
    uint32_t draw_calls{0};
    uint32_t triangles{0};
    uint32_t visible_meshes{0};
};

class SceneRenderer {
public:
    static SceneRenderer& instance();

    bool init(rhi::Format color_format, rhi::Format depth_format = rhi::Format::D32_SFLOAT);
    void shutdown();

    // Renders active scene from a specified camera View-Projection and Camera Position
    void render_scene(rhi::RhiCommandBuffer& cmd,
                      scene::Scene& scene,
                      const core::Mat4& view_proj,
                      const core::Vec3& camera_pos,
                      const rhi::Viewport& viewport,
                      const rhi::Rect2D& scissor);

    std::shared_ptr<Mesh> get_or_create_primitive(std::string_view name);
    void register_mesh(const assets::UUID& uuid, std::shared_ptr<Mesh> mesh);
    std::shared_ptr<Mesh> get_mesh(const assets::UUID& uuid);

    const SceneRenderStats& get_stats() const { return m_stats; }
    rhi::RhiGraphicsPipeline& get_pipeline() { return m_pipeline; }
    bool is_initialized() const { return m_initialized; }

private:
    SceneRenderer() = default;
    ~SceneRenderer();

    rhi::RhiShaderModule m_vert_shader;
    rhi::RhiShaderModule m_frag_shader;
    rhi::RhiGraphicsPipeline m_pipeline;

    std::unordered_map<std::string, std::shared_ptr<Mesh>> m_primitive_meshes;
    std::unordered_map<assets::UUID, std::shared_ptr<Mesh>> m_mesh_registry;

    SceneRenderStats m_stats{};
    bool m_initialized{false};
};

} // namespace engine::renderer
