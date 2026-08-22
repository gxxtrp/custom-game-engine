#include "engine/renderer/cascaded_shadow_feature.h"
#include "engine/renderer/embedded_shaders.h"
#include "engine/renderer/scene_mesh_iterator.h"
#include "engine/renderer/scene_renderer.h"
#include "engine/core/log.h"

namespace engine::renderer {

struct alignas(16) ShadowPushConstants {
    core::Mat4 light_view_proj;
    core::Mat4 model;
};

CascadedShadowFeature::~CascadedShadowFeature() = default;

bool CascadedShadowFeature::ensure_initialized(const SceneRenderView& view) {
    if (m_initialized) return true;
    if (!view.services) return false;

    const GraphicsSettings* settings = view.services->settings;
    const uint32_t resolution = settings ? settings->shadow_resolution : 2048;
    if (!m_csm.init(resolution)) {
        LOG_FATAL("CascadedShadow", "Failed to initialize cascaded shadow map");
        return false;
    }

    if (!m_vert_shader.init_from_spirv(shaders::SHADOW_VERT_SPV, shaders::SHADOW_VERT_SPV_SIZE, rhi::ShaderStage::Vertex) ||
        !m_frag_shader.init_from_spirv(shaders::SHADOW_FRAG_SPV, shaders::SHADOW_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment)) {
        LOG_FATAL("CascadedShadow", "Failed to create shadow shader modules");
        return false;
    }

    rhi::GraphicsPipelineDesc desc{};
    desc.vertex_shader = &m_vert_shader;
    desc.fragment_shader = &m_frag_shader;
    desc.color_formats = {};
    desc.depth_format = rhi::Format::D32_SFLOAT;

    desc.vertex_bindings = { MeshVertex::get_binding_description() };
    desc.vertex_attributes = { MeshVertex::get_attribute_descriptions()[0] };

    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pc_range.offset = 0;
    pc_range.size = sizeof(ShadowPushConstants);
    desc.push_constant_ranges = { pc_range };

    desc.depth_test_enable = true;
    desc.depth_write_enable = true;
    desc.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    desc.cull_mode = VK_CULL_MODE_NONE;
    desc.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (!m_pipeline.init(desc)) {
        LOG_FATAL("CascadedShadow", "Failed to create shadow pipeline");
        return false;
    }

    // 1x1 dummy depth texture (value 0.0 = fully lit) bound when shadows are
    // disabled, so the frame descriptor's b1 sampler stays validly bound.
    rhi::TextureDesc dummy_desc{};
    dummy_desc.width = 1;
    dummy_desc.height = 1;
    dummy_desc.format = rhi::Format::D32_SFLOAT;
    dummy_desc.usage = rhi::TextureUsage::DepthAttachment | rhi::TextureUsage::Sampled;
    dummy_desc.debug_name = "CSM_DummyDepth";
    if (!m_dummy_shadow_texture.init(dummy_desc)) {
        LOG_FATAL("CascadedShadow", "Failed to create dummy shadow texture");
        return false;
    }

    m_initialized = true;
    return true;
}

void CascadedShadowFeature::setup(RenderPassBuilder& builder, const SceneRenderView& view) {
    if (!view.services || !view.services->resources || !view.services->lights) return;
    if (!ensure_initialized(view)) return;

    const GraphicsSettings* settings = view.services->settings;
    FrameLightState& lights = *view.services->lights;

    // 1. Compute cascades from the current camera + directional light.
    if (settings && settings->enable_shadows && lights.has_directional && lights.cast_shadows) {
        m_csm.update_cascades(lights.direction, view.camera.view,
                              view.camera.fov_rad, view.camera.aspect,
                              view.camera.near_z, view.camera.far_z);
        m_active_cascades = std::min<uint32_t>(lights.cascade_count, CascadedShadowMap::CASCADE_COUNT);
        if (settings->shadow_cascade_count > 0) {
            m_active_cascades = std::min(m_active_cascades, settings->shadow_cascade_count);
        }

        lights.cascade_splits = m_csm.get_cascade_splits();
        for (uint32_t i = 0; i < CascadedShadowMap::CASCADE_COUNT; ++i) {
            lights.cascade_view_proj[i] = m_csm.get_cascades()[i].view_proj;
        }
        lights.cascades_valid = true;
    } else {
        m_active_cascades = 0;
        lights.cascades_valid = false;
    }

    // 2. Import the CSM depth array so later passes can declare read access and
    //    the graph handles layout transitions automatically.
    if (!view.services->resources->shadow_map.is_valid() && m_csm.get_shadow_texture().is_valid()) {
        const auto& tex = m_csm.get_shadow_texture();
        view.services->resources->shadow_map = builder.get_graph().import_texture(
            "CSM_DepthArray", tex.get_handle(), tex.get_view(),
            m_csm.get_resolution(), m_csm.get_resolution(),
            rhi::Format::D32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED);
    }
    if (view.services->resources->shadow_map.is_valid()) {
        builder.write_texture(view.services->resources->shadow_map, RGResourceAccess::DepthAttachmentWrite);
    }

    // 3. Keep the shared frame descriptor's shadow sampler binding valid.
    if (view.services->frame_uniforms && view.services->frame_uniforms->valid) {
        VkImageView shadow_view = (m_active_cascades > 0)
            ? m_csm.get_shadow_texture().get_view()
            : m_dummy_shadow_texture.get_view();
        view.services->frame_uniforms->descriptor.update_combined_image_sampler(
            1, shadow_view, view.services->frame_uniforms->shadow_sampler.get_handle(),
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    }
}

void CascadedShadowFeature::render_cascade(rhi::RhiCommandBuffer& cmd, uint32_t cascade, const SceneRenderView& view) {
    const auto& cascades = m_csm.get_cascades();

    rhi::DepthAttachmentDesc depth_att{};
    depth_att.image_view = m_csm.get_shadow_texture().get_or_create_layer_view(cascade);
    depth_att.image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_att.load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_att.store_op = VK_ATTACHMENT_STORE_OP_STORE;
    depth_att.clear_depth = 1.0f;

    rhi::RenderingDesc rendering{};
    rendering.render_area = rhi::Rect2D{ 0, 0, m_csm.get_resolution(), m_csm.get_resolution() };
    rendering.depth_attachment = depth_att;
    rendering.has_depth = true;
    cmd.begin_rendering(rendering);

    cmd.set_viewport(rhi::Viewport{ 0.0f, 0.0f, static_cast<float>(m_csm.get_resolution()), static_cast<float>(m_csm.get_resolution()), 0.0f, 1.0f });
    cmd.set_scissor(rhi::Rect2D{ 0, 0, m_csm.get_resolution(), m_csm.get_resolution() });
    cmd.bind_pipeline(m_pipeline.get_pipeline());

    for_each_mesh_draw(
        view.scene, *view.services->meshes, view.camera.frustum,
        false, nullptr,
        [](const scene::MaterialComponent* mat, const scene::MeshRendererComponent& mr) {
            (void)mat;
            return mr.cast_shadows;
        },
        [&](const MeshDrawRecord& rec) {
            ShadowPushConstants pc{};
            pc.light_view_proj = cascades[cascade].view_proj;
            pc.model = rec.world;
            cmd.push_constants(m_pipeline.get_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

            cmd.bind_vertex_buffer(0, rec.mesh->get_vertex_buffer().get_handle());
            cmd.bind_index_buffer(rec.mesh->get_index_buffer().get_handle());
            cmd.draw_indexed(static_cast<uint32_t>(rec.mesh->get_indices().size()), 1, 0, 0, 0);
        });

    cmd.end_rendering();
}

void CascadedShadowFeature::execute(RenderPassContext& ctx, const SceneRenderView& view) {
    if (!view.services || !view.services->meshes) return;
    if (m_active_cascades == 0) return;

    auto& cmd = ctx.get_command_buffer();
    if (cmd.get_handle() == VK_NULL_HANDLE) return;

    // The graph pass owns no attachments; each cascade is a manual dynamic
    // rendering block targeting its array slice.
    for (uint32_t c = 0; c < m_active_cascades; ++c) {
        render_cascade(cmd, c, view);
    }
}

} // namespace engine::renderer
