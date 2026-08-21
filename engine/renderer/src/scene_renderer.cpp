#include "engine/renderer/scene_renderer.h"
#include "engine/renderer/embedded_shaders.h"
#include "engine/scene/components.h"
#include "engine/core/log.h"
#include <algorithm>

namespace engine::renderer {

SceneRenderer& SceneRenderer::instance() {
    static SceneRenderer s_instance;
    return s_instance;
}

SceneRenderer::~SceneRenderer() {
    shutdown();
}

bool SceneRenderer::init(rhi::Format sdr_format, rhi::Format hdr_format, rhi::Format depth_format) {
    if (m_initialized) return true;

    // 1. Forward Mesh Lit Pipeline (HDR Target)
    if (!m_vert_shader.init_from_spirv(shaders::MESH_VERT_SPV, shaders::MESH_VERT_SPV_SIZE, rhi::ShaderStage::Vertex)) {
        LOG_FATAL("SceneRenderer", "Failed to create mesh vertex shader module!");
        return false;
    }

    if (!m_frag_shader.init_from_spirv(shaders::MESH_FRAG_SPV, shaders::MESH_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment)) {
        LOG_FATAL("SceneRenderer", "Failed to create mesh fragment shader module!");
        return false;
    }

    rhi::GraphicsPipelineDesc mesh_pdesc{};
    mesh_pdesc.vertex_shader = &m_vert_shader;
    mesh_pdesc.fragment_shader = &m_frag_shader;
    mesh_pdesc.color_formats = { sdr_format };
    mesh_pdesc.depth_format = depth_format;

    mesh_pdesc.vertex_bindings = { MeshVertex::get_binding_description() };
    mesh_pdesc.vertex_attributes = MeshVertex::get_attribute_descriptions();

    VkPushConstantRange mesh_pc_range{};
    mesh_pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    mesh_pc_range.offset = 0;
    mesh_pc_range.size = sizeof(MeshPushConstants);
    mesh_pdesc.push_constant_ranges = { mesh_pc_range };

    mesh_pdesc.depth_test_enable = (depth_format != rhi::Format::Undefined);
    mesh_pdesc.depth_write_enable = (depth_format != rhi::Format::Undefined);
    mesh_pdesc.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    mesh_pdesc.cull_mode = VK_CULL_MODE_NONE;
    mesh_pdesc.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    mesh_pdesc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (!m_pipeline.init(mesh_pdesc)) {
        LOG_FATAL("SceneRenderer", "Failed to create 3D Scene Mesh Graphics Pipeline!");
        return false;
    }

    // 2. Cascaded Shadow Depth Pipeline (Depth-Only)
    if (!m_shadow_vert_shader.init_from_spirv(shaders::SHADOW_VERT_SPV, shaders::SHADOW_VERT_SPV_SIZE, rhi::ShaderStage::Vertex)) {
        LOG_FATAL("SceneRenderer", "Failed to create shadow vertex shader module!");
        return false;
    }

    if (!m_shadow_frag_shader.init_from_spirv(shaders::SHADOW_FRAG_SPV, shaders::SHADOW_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment)) {
        LOG_FATAL("SceneRenderer", "Failed to create shadow fragment shader module!");
        return false;
    }

    rhi::GraphicsPipelineDesc shadow_pdesc{};
    shadow_pdesc.vertex_shader = &m_shadow_vert_shader;
    shadow_pdesc.fragment_shader = &m_shadow_frag_shader;
    shadow_pdesc.color_formats = {}; // No color attachments
    shadow_pdesc.depth_format = depth_format;

    shadow_pdesc.vertex_bindings = { MeshVertex::get_binding_description() };
    shadow_pdesc.vertex_attributes = { MeshVertex::get_attribute_descriptions()[0] };

    VkPushConstantRange shadow_pc_range{};
    shadow_pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    shadow_pc_range.offset = 0;
    shadow_pc_range.size = sizeof(ShadowPushConstants);
    shadow_pdesc.push_constant_ranges = { shadow_pc_range };

    shadow_pdesc.depth_test_enable = true;
    shadow_pdesc.depth_write_enable = true;
    shadow_pdesc.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    shadow_pdesc.cull_mode = VK_CULL_MODE_NONE;
    shadow_pdesc.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    shadow_pdesc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (!m_shadow_pipeline.init(shadow_pdesc)) {
        LOG_FATAL("SceneRenderer", "Failed to create Shadow Depth Graphics Pipeline!");
        return false;
    }

    // 3. Fullscreen Tonemap Pipeline (HDR -> SDR)
    if (!m_tonemap_vert_shader.init_from_spirv(shaders::TONEMAP_VERT_SPV, shaders::TONEMAP_VERT_SPV_SIZE, rhi::ShaderStage::Vertex)) {
        LOG_FATAL("SceneRenderer", "Failed to create tonemap vertex shader module!");
        return false;
    }

    if (!m_tonemap_frag_shader.init_from_spirv(shaders::TONEMAP_FRAG_SPV, shaders::TONEMAP_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment)) {
        LOG_FATAL("SceneRenderer", "Failed to create tonemap fragment shader module!");
        return false;
    }

    rhi::GraphicsPipelineDesc tonemap_pdesc{};
    tonemap_pdesc.vertex_shader = &m_tonemap_vert_shader;
    tonemap_pdesc.fragment_shader = &m_tonemap_frag_shader;
    tonemap_pdesc.color_formats = { sdr_format };
    tonemap_pdesc.depth_format = rhi::Format::Undefined;

    tonemap_pdesc.vertex_bindings = {};
    tonemap_pdesc.vertex_attributes = {};

    VkPushConstantRange tonemap_pc_range{};
    tonemap_pc_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    tonemap_pc_range.offset = 0;
    tonemap_pc_range.size = sizeof(TonemapPushConstants);
    tonemap_pdesc.push_constant_ranges = { tonemap_pc_range };

    tonemap_pdesc.depth_test_enable = false;
    tonemap_pdesc.depth_write_enable = false;
    tonemap_pdesc.cull_mode = VK_CULL_MODE_NONE;
    tonemap_pdesc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (!m_tonemap_pipeline.init(tonemap_pdesc)) {
        LOG_FATAL("SceneRenderer", "Failed to create Tonemapping Graphics Pipeline!");
        return false;
    }

    // 4. Initialize Cascaded Shadow Map
    m_shadow_map.init(2048);

    // 5. Pre-upload procedural primitive meshes
    m_primitive_meshes["Cube"] = Mesh::create_cube(core::Vec3(1.0f, 1.0f, 1.0f));
    m_primitive_meshes["Sphere"] = Mesh::create_sphere(0.5f, 32, 16);
    m_primitive_meshes["Plane"] = Mesh::create_plane(1.0f, 1.0f, 1);
    m_primitive_meshes["GroundPlane"] = Mesh::create_cube(core::Vec3(1.0f, 1.0f, 1.0f));
    m_primitive_meshes["DynamicPhysicsBox"] = Mesh::create_cube(core::Vec3(1.0f, 1.0f, 1.0f));
    m_primitive_meshes["PlayerController"] = Mesh::create_cube(core::Vec3(1.0f, 1.0f, 1.0f));

    m_initialized = true;
    LOG_INFO("SceneRenderer", "Initialized Production Dynamic Multi-Pass Scene Renderer successfully");
    return true;
}

void SceneRenderer::shutdown() {
    if (!m_initialized) return;

    m_primitive_meshes.clear();
    m_mesh_registry.clear();

    m_shadow_map.destroy();
    m_pipeline.destroy();
    m_shadow_pipeline.destroy();
    m_tonemap_pipeline.destroy();

    m_vert_shader.destroy();
    m_frag_shader.destroy();
    m_shadow_vert_shader.destroy();
    m_shadow_frag_shader.destroy();
    m_tonemap_vert_shader.destroy();
    m_tonemap_frag_shader.destroy();

    m_initialized = false;
    LOG_INFO("SceneRenderer", "SceneRenderer shutdown cleanly");
}

std::shared_ptr<Mesh> SceneRenderer::get_or_create_primitive(std::string_view name) {
    if (name.find("Sphere") != std::string_view::npos || name.find("sphere") != std::string_view::npos) {
        return m_primitive_meshes["Sphere"];
    }
    if (name.find("Plane") != std::string_view::npos || name.find("plane") != std::string_view::npos || name.find("Ground") != std::string_view::npos) {
        return m_primitive_meshes["Plane"];
    }
    return m_primitive_meshes["Cube"];
}

void SceneRenderer::register_mesh(const assets::UUID& uuid, std::shared_ptr<Mesh> mesh) {
    m_mesh_registry[uuid] = mesh;
}

std::shared_ptr<Mesh> SceneRenderer::get_mesh(const assets::UUID& uuid) {
    auto it = m_mesh_registry.find(uuid);
    if (it != m_mesh_registry.end()) {
        return it->second;
    }
    return nullptr;
}

void SceneRenderer::render_shadow_pass(rhi::RhiCommandBuffer& cmd,
                                       scene::Scene& scene,
                                       const core::Mat4& light_view_proj,
                                       const rhi::Viewport& viewport,
                                       const rhi::Rect2D& scissor) {
    cmd.set_viewport(viewport);
    cmd.set_scissor(scissor);
    cmd.bind_pipeline(m_shadow_pipeline.get_pipeline());

    scene.get_world().each([&](flecs::entity e, const scene::TransformComponent& trans, const scene::MeshRendererComponent& mr) {
        if (!mr.is_visible || !mr.cast_shadows) return;

        std::shared_ptr<Mesh> mesh = mr.mesh_uuid.is_valid() ? get_mesh(mr.mesh_uuid) : get_or_create_primitive(e.name().c_str());
        if (!mesh || !mesh->is_gpu_uploaded() || mesh->get_indices().empty()) return;

        core::Mat4 world_mat = trans.get_local_matrix();
        if (e.has<scene::WorldTransformComponent>()) {
            world_mat = e.get<scene::WorldTransformComponent>().matrix;
        }

        ShadowPushConstants pc{};
        pc.light_view_proj = light_view_proj;
        pc.model = world_mat;

        cmd.push_constants(m_shadow_pipeline.get_layout(),
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(ShadowPushConstants),
                           &pc);

        cmd.bind_vertex_buffer(0, mesh->get_vertex_buffer().get_handle());
        cmd.bind_index_buffer(mesh->get_index_buffer().get_handle());
        cmd.draw_indexed(static_cast<uint32_t>(mesh->get_indices().size()), 1, 0, 0, 0);
    });
}

void SceneRenderer::render_scene(rhi::RhiCommandBuffer& cmd,
                                scene::Scene& scene,
                                const core::Mat4& view_proj,
                                const core::Vec3& camera_pos,
                                const core::Frustum& frustum,
                                bool enable_frustum_culling,
                                const rhi::Viewport& viewport,
                                const rhi::Rect2D& scissor) {
    scene.update_transforms();
    m_stats = SceneRenderStats{};

    cmd.set_viewport(viewport);
    cmd.set_scissor(scissor);
    cmd.bind_pipeline(m_pipeline.get_pipeline());

    // 1. Query Directional Light in Scene
    core::Vec3 light_dir(0.5f, 1.0f, 0.3f);
    float light_intensity = 1.5f;
    core::Vec3 light_color(1.0f, 0.98f, 0.92f);

    scene.get_world().each([&](flecs::entity e, const scene::DirectionalLightComponent& light) {
        light_color = light.color;
        light_intensity = light.intensity;
        if (e.has<scene::TransformComponent>()) {
            const auto& t = e.get<scene::TransformComponent>();
            light_dir = t.rotation.rotate(core::Vec3(0.0f, -1.0f, 0.0f)).normalized();
            if (light_dir.length_sq() < 0.0001f) {
                light_dir = core::Vec3(0.0f, -1.0f, 0.0f);
            }
        }
    });

    // 2. Iterate Entities with TransformComponent + MeshRendererComponent
    scene.get_world().each([&](flecs::entity e, const scene::TransformComponent& trans, const scene::MeshRendererComponent& mr) {
        if (!mr.is_visible) return;

        // Frustum Culling
        if (enable_frustum_culling) {
            float max_scale = std::max({ std::abs(trans.scale.x), std::abs(trans.scale.y), std::abs(trans.scale.z) });
            float bounding_radius = max_scale * 0.866f; // Bounding sphere enclosing unit box
            if (!frustum.intersects_sphere(trans.position, bounding_radius)) {
                m_stats.culled_meshes++;
                return; // Culled!
            }
        }

        // Resolve Mesh
        std::shared_ptr<Mesh> mesh = mr.mesh_uuid.is_valid() ? get_mesh(mr.mesh_uuid) : get_or_create_primitive(e.name().c_str());
        if (!mesh || !mesh->is_gpu_uploaded() || mesh->get_indices().empty()) return;

        // Compute Transform
        core::Mat4 world_mat = trans.get_local_matrix();
        if (e.has<scene::WorldTransformComponent>()) {
            world_mat = e.get<scene::WorldTransformComponent>().matrix;
        }

        // Material Data (PBR)
        core::Vec4 base_color(0.85f, 0.85f, 0.85f, 1.0f);
        float roughness = 0.5f;
        float metallic = 0.0f;
        float emissive_strength = 0.0f;
        if (e.has<scene::MaterialComponent>()) {
            const auto& mat = e.get<scene::MaterialComponent>();
            base_color = mat.base_color;
            roughness = mat.roughness;
            metallic = mat.metallic;
            emissive_strength = mat.emissive_strength;
        }

        // Push Constants
        MeshPushConstants pc{};
        pc.view_proj = view_proj;
        pc.model = world_mat;
        pc.base_color = base_color;
        pc.light_dir_intensity = core::Vec4(light_dir.x, light_dir.y, light_dir.z, light_intensity);
        pc.light_color_ambient = core::Vec4(light_color.x, light_color.y, light_color.z, 0.20f);
        pc.camera_pos_roughness = core::Vec4(camera_pos.x, camera_pos.y, camera_pos.z, roughness);
        pc.material_params = core::Vec4(metallic, emissive_strength, 0.0f, 0.0f);

        cmd.push_constants(m_pipeline.get_layout(),
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(MeshPushConstants),
                           &pc);

        cmd.bind_vertex_buffer(0, mesh->get_vertex_buffer().get_handle());
        cmd.bind_index_buffer(mesh->get_index_buffer().get_handle());
        cmd.draw_indexed(static_cast<uint32_t>(mesh->get_indices().size()), 1, 0, 0, 0);

        m_stats.draw_calls++;
        m_stats.triangles += static_cast<uint32_t>(mesh->get_indices().size() / 3);
        m_stats.visible_meshes++;
    });
}

void SceneRenderer::render_tonemapping(rhi::RhiCommandBuffer& cmd,
                                      const TonemapPushConstants& pc,
                                      const rhi::Viewport& viewport,
                                      const rhi::Rect2D& scissor) {
    cmd.set_viewport(viewport);
    cmd.set_scissor(scissor);
    cmd.bind_pipeline(m_tonemap_pipeline.get_pipeline());

    cmd.push_constants(m_tonemap_pipeline.get_layout(),
                       VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(TonemapPushConstants),
                       &pc);

    // Draw full-screen triangle with 3 vertices
    cmd.draw(3, 1, 0, 0);
}

RGTextureHandle SceneRenderer::setup_render_pipeline(RenderGraph& rg,
                                                    scene::Scene& scene,
                                                    const RenderCamera& camera,
                                                    const GraphicsSettings& settings,
                                                    RGTextureHandle output_sdr_target,
                                                    uint32_t width,
                                                    uint32_t height) {
    // 1. Pass 1: Forward Mesh Pass (Renders directly into target SDR swapchain with Depth)
    RGTextureHandle depth_rg = rg.create_texture(RGTextureDesc{
        .width = width,
        .height = height,
        .format = rhi::Format::D32_SFLOAT,
        .usage = rhi::TextureUsage::DepthAttachment,
        .debug_name = "SceneDepth"
    });

    rg.add_pass(
        "SceneForwardLitPass",
        [&](RenderPassBuilder& builder) {
            builder.set_color_attachment(
                0,
                output_sdr_target,
                VK_ATTACHMENT_LOAD_OP_CLEAR,
                VK_ATTACHMENT_STORE_OP_STORE,
                core::Vec4(0.08f, 0.09f, 0.11f, 1.0f)
            );
            builder.set_depth_attachment(depth_rg, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, 1.0f);
        },
        [this, &scene, camera, settings, width, height](RenderPassContext& ctx) {
            auto& cmd = ctx.get_command_buffer();
            render_scene(
                cmd,
                scene,
                camera.view_proj,
                camera.position,
                camera.frustum,
                settings.enable_frustum_culling,
                rhi::Viewport{
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = static_cast<float>(width),
                    .height = static_cast<float>(height),
                    .min_depth = 0.0f,
                    .max_depth = 1.0f
                },
                rhi::Rect2D{
                    .offset_x = 0,
                    .offset_y = 0,
                    .width = width,
                    .height = height
                }
            );
        }
    );

    return output_sdr_target;
}

} // namespace engine::renderer
