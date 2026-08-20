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

bool SceneRenderer::init(rhi::Format color_format, rhi::Format depth_format) {
    if (m_initialized) return true;

    // 1. Compile Shader Modules
    if (!m_vert_shader.init_from_spirv(shaders::MESH_VERT_SPV, shaders::MESH_VERT_SPV_SIZE, rhi::ShaderStage::Vertex)) {
        LOG_FATAL("SceneRenderer", "Failed to create mesh vertex shader module!");
        return false;
    }

    if (!m_frag_shader.init_from_spirv(shaders::MESH_FRAG_SPV, shaders::MESH_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment)) {
        LOG_FATAL("SceneRenderer", "Failed to create mesh fragment shader module!");
        return false;
    }

    // 2. Vertex & Push Constant Layout
    rhi::GraphicsPipelineDesc pipeline_desc{};
    pipeline_desc.vertex_shader = &m_vert_shader;
    pipeline_desc.fragment_shader = &m_frag_shader;
    pipeline_desc.color_formats = { color_format };
    pipeline_desc.depth_format = depth_format;

    pipeline_desc.vertex_bindings = { MeshVertex::get_binding_description() };
    pipeline_desc.vertex_attributes = MeshVertex::get_attribute_descriptions();

    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc_range.offset = 0;
    pc_range.size = sizeof(MeshPushConstants);
    pipeline_desc.push_constant_ranges = { pc_range };

    pipeline_desc.depth_test_enable = (depth_format != rhi::Format::Undefined);
    pipeline_desc.depth_write_enable = (depth_format != rhi::Format::Undefined);
    pipeline_desc.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipeline_desc.cull_mode = VK_CULL_MODE_NONE; // Two-sided for generic primitives
    pipeline_desc.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipeline_desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (!m_pipeline.init(pipeline_desc)) {
        LOG_FATAL("SceneRenderer", "Failed to create 3D Scene Mesh Graphics Pipeline!");
        return false;
    }

    // 3. Pre-upload procedural primitive meshes
    m_primitive_meshes["Cube"] = Mesh::create_cube(core::Vec3(1.0f, 1.0f, 1.0f));
    m_primitive_meshes["Sphere"] = Mesh::create_sphere(0.5f, 32, 16);
    m_primitive_meshes["Plane"] = Mesh::create_plane(1.0f, 1.0f, 1);
    m_primitive_meshes["GroundPlane"] = Mesh::create_cube(core::Vec3(1.0f, 1.0f, 1.0f));
    m_primitive_meshes["DynamicPhysicsBox"] = Mesh::create_cube(core::Vec3(1.0f, 1.0f, 1.0f));
    m_primitive_meshes["PlayerController"] = Mesh::create_cube(core::Vec3(1.0f, 1.0f, 1.0f));

    m_initialized = true;
    LOG_INFO("SceneRenderer", "Initialized 3D Forward Mesh Scene Renderer successfully");
    return true;
}

void SceneRenderer::shutdown() {
    if (!m_initialized) return;

    m_primitive_meshes.clear();
    m_mesh_registry.clear();

    m_pipeline.destroy();
    m_vert_shader.destroy();
    m_frag_shader.destroy();

    m_initialized = false;
    LOG_INFO("SceneRenderer", "SceneRenderer shutdown cleanly");
}

std::shared_ptr<Mesh> SceneRenderer::get_or_create_primitive(std::string_view name) {
    auto it = m_primitive_meshes.find(std::string(name));
    if (it != m_primitive_meshes.end()) {
        return it->second;
    }
    // Fallback to Cube
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

void SceneRenderer::render_scene(rhi::RhiCommandBuffer& cmd,
                                scene::Scene& scene,
                                const core::Mat4& view_proj,
                                const core::Vec3& camera_pos,
                                const rhi::Viewport& viewport,
                                const rhi::Rect2D& scissor) {
    if (!m_initialized) return;

    // Synchronize scene transforms to ensure world matrices are updated for rendering
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
            // Use quaternion rotation to calculate light direction (default down vector rotated by orientation)
            light_dir = t.rotation.rotate(core::Vec3(0.0f, -1.0f, 0.0f)).normalized();
            if (light_dir.length_sq() < 0.0001f) {
                light_dir = core::Vec3(0.0f, -1.0f, 0.0f);
            }
        }
    });

    // 2. Iterate Entities with TransformComponent + MeshRendererComponent
    scene.get_world().each([&](flecs::entity e, const scene::TransformComponent& trans, const scene::MeshRendererComponent& mr) {
        if (!mr.is_visible) return;

        // Resolve Mesh
        std::shared_ptr<Mesh> mesh = nullptr;
        if (mr.mesh_uuid.is_valid()) {
            mesh = get_mesh(mr.mesh_uuid);
        }

        if (!mesh) {
            std::string entity_name = e.name().c_str();
            mesh = get_or_create_primitive(entity_name);
        }

        if (!mesh || !mesh->is_gpu_uploaded() || mesh->get_indices().empty()) {
            return;
        }

        // Compute Transform
        core::Mat4 world_mat = trans.get_local_matrix();
        if (e.has<scene::WorldTransformComponent>()) {
            world_mat = e.get<scene::WorldTransformComponent>().matrix;
        }

        // Data-driven material properties (no entity name string heuristics)
        core::Vec4 base_color(0.85f, 0.85f, 0.85f, 1.0f);
        float roughness = 0.5f;
        if (e.has<scene::MaterialComponent>()) {
            const auto& mat = e.get<scene::MaterialComponent>();
            base_color = mat.base_color;
            roughness = mat.roughness;
        }

        // Push Constants
        MeshPushConstants pc{};
        pc.view_proj = view_proj;
        pc.model = world_mat;
        pc.base_color = base_color;
        pc.light_dir_intensity = core::Vec4(light_dir.x, light_dir.y, light_dir.z, light_intensity);
        pc.light_color_ambient = core::Vec4(light_color.x, light_color.y, light_color.z, 0.20f);
        pc.camera_pos_roughness = core::Vec4(camera_pos.x, camera_pos.y, camera_pos.z, roughness);

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

} // namespace engine::renderer
