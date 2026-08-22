#include "engine/renderer/depth_pre_pass_feature.h"
#include "engine/renderer/embedded_shaders.h"
#include "engine/renderer/scene_mesh_iterator.h"
#include "engine/renderer/scene_renderer.h"
#include "engine/core/log.h"

namespace engine::renderer {

struct alignas(16) DepthPrePassPushConstants {
    core::Mat4 model;
};

bool DepthPrePassFeature::ensure_pipeline(VkDescriptorSetLayout frame_set_layout) {
    if (m_initialized) {
        if (m_bound_layout == frame_set_layout) return true;
        m_pipeline.destroy();
        m_initialized = false;
    }

    if (!m_vert_shader.init_from_spirv(shaders::DEPTH_PREPASS_VERT_SPV, shaders::DEPTH_PREPASS_VERT_SPV_SIZE, rhi::ShaderStage::Vertex) ||
        !m_frag_shader.init_from_spirv(shaders::DEPTH_PREPASS_FRAG_SPV, shaders::DEPTH_PREPASS_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment)) {
        LOG_FATAL("DepthPrePass", "Failed to create depth prepass shader modules");
        return false;
    }

    rhi::GraphicsPipelineDesc desc{};
    desc.vertex_shader = &m_vert_shader;
    desc.fragment_shader = &m_frag_shader;
    desc.color_formats = { rhi::Format::R32G32_SFLOAT }; // velocity
    desc.depth_format = rhi::Format::D32_SFLOAT;

    desc.vertex_bindings = { MeshVertex::get_binding_description() };
    desc.vertex_attributes = { MeshVertex::get_attribute_descriptions()[0] };

    // Set 0: FrameUniforms UBO (view_proj / prev_view_proj) — shared frame layout.
    if (frame_set_layout != VK_NULL_HANDLE) {
        desc.descriptor_set_layouts = { frame_set_layout };
    }

    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc_range.offset = 0;
    pc_range.size = sizeof(DepthPrePassPushConstants);
    desc.push_constant_ranges = { pc_range };

    desc.depth_test_enable = true;
    desc.depth_write_enable = true;
    desc.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    desc.cull_mode = VK_CULL_MODE_NONE;
    desc.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (!m_pipeline.init(desc)) {
        LOG_FATAL("DepthPrePass", "Failed to create depth prepass pipeline");
        return false;
    }

    m_bound_layout = frame_set_layout;
    m_initialized = true;
    return true;
}

void DepthPrePassFeature::setup(RenderPassBuilder& builder, const SceneRenderView& view) {
    if (!view.services || !view.services->resources) return;

    // Velocity target
    if (!view.services->resources->velocity_buffer.is_valid()) {
        view.services->resources->velocity_buffer = builder.create_texture(RGTextureDesc{
            .width = view.viewport_width,
            .height = view.viewport_height,
            .format = rhi::Format::R32G32_SFLOAT,
            .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled,
            .debug_name = "VelocityBuffer"
        });
    }

    // Scene depth (shared with forward opaque, volumetrics, OIT)
    if (!view.services->resources->scene_depth.is_valid()) {
        view.services->resources->scene_depth = builder.create_texture(RGTextureDesc{
            .width = view.viewport_width,
            .height = view.viewport_height,
            .format = rhi::Format::D32_SFLOAT,
            .usage = rhi::TextureUsage::DepthAttachment | rhi::TextureUsage::Sampled,
            .debug_name = "SceneDepth"
        });
    }

    builder.set_color_attachment(0, view.services->resources->velocity_buffer,
                                 VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                 core::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    builder.set_depth_attachment(view.services->resources->scene_depth,
                                 VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, 1.0f);
}

void DepthPrePassFeature::execute(RenderPassContext& ctx, const SceneRenderView& view) {
    if (!view.services || !view.services->meshes) return;

    auto& cmd = ctx.get_command_buffer();
    if (cmd.get_handle() == VK_NULL_HANDLE) return; // CPU-only execution

    VkDescriptorSetLayout frame_layout = VK_NULL_HANDLE;
    if (view.services->frame_uniforms && view.services->frame_uniforms->valid) {
        frame_layout = view.services->frame_uniforms->set_layout.get_handle();
    }
    if (!ensure_pipeline(frame_layout)) return;

    cmd.set_viewport(rhi::Viewport{ 0.0f, 0.0f, static_cast<float>(view.viewport_width), static_cast<float>(view.viewport_height), 0.0f, 1.0f });
    cmd.set_scissor(rhi::Rect2D{ 0, 0, view.viewport_width, view.viewport_height });
    cmd.bind_pipeline(m_pipeline.get_pipeline());

    if (view.services->frame_uniforms && view.services->frame_uniforms->valid) {
        cmd.bind_descriptor_set(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipeline.get_layout(),
                                view.services->frame_uniforms->descriptor.get_handle());
    }

    const bool cull = view.services->settings ? view.services->settings->enable_frustum_culling : true;

    for_each_mesh_draw(
        view.scene, *view.services->meshes, view.camera.frustum, cull, view.services->stats,
        [](const scene::MaterialComponent* mat, const scene::MeshRendererComponent& mr) {
            (void)mat; (void)mr; // depth prepass covers all visible geometry
            return true;
        },
        [&](const MeshDrawRecord& rec) {
            DepthPrePassPushConstants pc{};
            pc.model = rec.world;
            cmd.push_constants(m_pipeline.get_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

            cmd.bind_vertex_buffer(0, rec.mesh->get_vertex_buffer().get_handle());
            cmd.bind_index_buffer(rec.mesh->get_index_buffer().get_handle());
            cmd.draw_indexed(static_cast<uint32_t>(rec.mesh->get_indices().size()), 1, 0, 0, 0);
        });
}

} // namespace engine::renderer
