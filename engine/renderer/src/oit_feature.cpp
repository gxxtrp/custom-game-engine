#include "engine/renderer/oit_feature.h"
#include "engine/renderer/embedded_shaders.h"
#include "engine/renderer/scene_mesh_iterator.h"
#include "engine/renderer/scene_renderer.h"
#include "engine/core/log.h"
#include <algorithm>

namespace engine::renderer {

bool OitFeature::ensure_pipelines(const SceneRenderView& view) {
    if (m_initialized) {
        if (m_frame_layout == (view.services && view.services->frame_uniforms
                                   ? view.services->frame_uniforms->set_layout.get_handle()
                                   : VK_NULL_HANDLE)) {
            return true;
        }
        m_accumulate_pipeline.destroy();
        m_resolve_pipeline.destroy();
        m_initialized = false;
    }

    VkDescriptorSetLayout frame_layout = VK_NULL_HANDLE;
    if (view.services && view.services->frame_uniforms && view.services->frame_uniforms->valid) {
        frame_layout = view.services->frame_uniforms->set_layout.get_handle();
    }

    // ---- Accumulation pipeline: 2 attachments (accum RGBA16F, reveal R32F) ----
    if (!m_wboit_vert_shader.init_from_spirv(shaders::WBOIT_VERT_SPV, shaders::WBOIT_VERT_SPV_SIZE, rhi::ShaderStage::Vertex) ||
        !m_wboit_frag_shader.init_from_spirv(shaders::WBOIT_ACCUM_FRAG_SPV, shaders::WBOIT_ACCUM_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment)) {
        LOG_FATAL("WboitOIT", "Failed to create WBOIT shader modules");
        return false;
    }

    rhi::GraphicsPipelineDesc accum_desc{};
    accum_desc.vertex_shader = &m_wboit_vert_shader;
    accum_desc.fragment_shader = &m_wboit_frag_shader;
    accum_desc.color_formats = { rhi::Format::R16G16B16A16_SFLOAT, rhi::Format::R32_SFLOAT };
    accum_desc.depth_format = rhi::Format::D32_SFLOAT;
    accum_desc.vertex_bindings = { MeshVertex::get_binding_description() };
    accum_desc.vertex_attributes = MeshVertex::get_attribute_descriptions();
    if (frame_layout != VK_NULL_HANDLE) {
        accum_desc.descriptor_set_layouts = { frame_layout };
    }

    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc_range.offset = 0;
    pc_range.size = sizeof(MeshPushConstants);
    accum_desc.push_constant_ranges = { pc_range };

    // Attachment 0: accumulation — additive (ONE, ONE)
    rhi::GraphicsPipelineDesc::BlendState accum_blend{};
    accum_blend.enable = true;
    accum_blend.src_color = VK_BLEND_FACTOR_ONE;
    accum_blend.dst_color = VK_BLEND_FACTOR_ONE;
    accum_blend.src_alpha = VK_BLEND_FACTOR_ONE;
    accum_blend.dst_alpha = VK_BLEND_FACTOR_ONE;
    // Attachment 1: revealage — dst attenuated by (1 - src.r); the reveal output
    // is a single-component scalar (R32F) so the source factor must come from
    // COLOR, not ALPHA (McGuire & Bavoil WBOIT).
    rhi::GraphicsPipelineDesc::BlendState reveal_blend{};
    reveal_blend.enable = true;
    reveal_blend.src_color = VK_BLEND_FACTOR_ZERO;
    reveal_blend.dst_color = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    reveal_blend.src_alpha = VK_BLEND_FACTOR_ZERO;
    reveal_blend.dst_alpha = VK_BLEND_FACTOR_ONE;
    accum_desc.blend_states = { accum_blend, reveal_blend };

    accum_desc.depth_test_enable = true;
    accum_desc.depth_write_enable = false; // depth prepass owns depth
    accum_desc.depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    accum_desc.cull_mode = VK_CULL_MODE_NONE;
    accum_desc.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    accum_desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (!m_accumulate_pipeline.init(accum_desc)) {
        LOG_FATAL("WboitOIT", "Failed to create WBOIT accumulation pipeline");
        return false;
    }

    // ---- Resolve pipeline: fullscreen composite ----
    if (!m_resolve_vert_shader.init_from_spirv(shaders::TONEMAP_VERT_SPV, shaders::TONEMAP_VERT_SPV_SIZE, rhi::ShaderStage::Vertex) ||
        !m_resolve_frag_shader.init_from_spirv(shaders::OIT_RESOLVE_FRAG_SPV, shaders::OIT_RESOLVE_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment)) {
        LOG_FATAL("WboitOIT", "Failed to create OIT resolve shader modules");
        return false;
    }

    rhi::SamplerDesc sampler_desc{};
    sampler_desc.min_filter = rhi::SamplerFilter::Linear;
    sampler_desc.mag_filter = rhi::SamplerFilter::Linear;
    sampler_desc.mipmap_mode = rhi::SamplerFilter::Nearest;
    sampler_desc.address_u = rhi::SamplerAddressMode::ClampToEdge;
    sampler_desc.address_v = rhi::SamplerAddressMode::ClampToEdge;
    sampler_desc.address_w = rhi::SamplerAddressMode::ClampToEdge;
    sampler_desc.enable_anisotropy = false;
    sampler_desc.debug_name = "OITResolveSampler";
    if (!m_linear_sampler.init(sampler_desc)) {
        LOG_FATAL("WboitOIT", "Failed to create resolve sampler");
        return false;
    }

    std::vector<rhi::DescriptorBinding> resolve_bindings = {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // opaque color
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // accumulation
        { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // revealage
    };
    if (!m_resolve_set_layout.init(resolve_bindings)) {
        LOG_FATAL("WboitOIT", "Failed to create resolve descriptor set layout");
        return false;
    }
    if (!m_resolve_pool.init(1, { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 } }) ||
        !m_resolve_descriptor.init(m_resolve_pool, m_resolve_set_layout.get_handle())) {
        LOG_FATAL("WboitOIT", "Failed to allocate resolve descriptor set");
        return false;
    }

    rhi::GraphicsPipelineDesc resolve_desc{};
    resolve_desc.vertex_shader = &m_resolve_vert_shader;
    resolve_desc.fragment_shader = &m_resolve_frag_shader;
    resolve_desc.color_formats = { rhi::Format::R16G16B16A16_SFLOAT };
    resolve_desc.depth_format = rhi::Format::Undefined;
    resolve_desc.vertex_bindings = {};
    resolve_desc.vertex_attributes = {};
    resolve_desc.descriptor_set_layouts = { m_resolve_set_layout.get_handle() };
    resolve_desc.depth_test_enable = false;
    resolve_desc.depth_write_enable = false;
    resolve_desc.cull_mode = VK_CULL_MODE_NONE;
    resolve_desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (!m_resolve_pipeline.init(resolve_desc)) {
        LOG_FATAL("WboitOIT", "Failed to create OIT resolve pipeline");
        return false;
    }

    m_frame_layout = frame_layout;
    m_initialized = true;
    return true;
}

void OitFeature::setup(RenderPassBuilder& builder, const SceneRenderView& view) {
    if (!view.services || !view.services->resources) return;
    if (!view.services->resources->scene_color_hdr.is_valid()) return;
    if (!ensure_pipelines(view)) return; // pipelines ready before command recording

    // ---- Pass 1: accumulation into WBOIT targets ----
    view.services->resources->oit_accumulation = builder.create_texture(RGTextureDesc{
        .width = view.viewport_width,
        .height = view.viewport_height,
        .format = rhi::Format::R16G16B16A16_SFLOAT,
        .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled,
        .debug_name = "WBOIT_Accumulation"
    });
    view.services->resources->oit_revealage = builder.create_texture(RGTextureDesc{
        .width = view.viewport_width,
        .height = view.viewport_height,
        .format = rhi::Format::R32_SFLOAT,
        .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled,
        .debug_name = "WBOIT_Revealage"
    });

    builder.set_color_attachment(0, view.services->resources->oit_accumulation,
                                 VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                 core::Vec4(0.0f, 0.0f, 0.0f, 0.0f));
    builder.set_color_attachment(1, view.services->resources->oit_revealage,
                                 VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                 core::Vec4(1.0f, 0.0f, 0.0f, 0.0f));
    builder.set_depth_attachment(view.services->resources->scene_depth,
                                 VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_DONT_CARE, 1.0f);

    // ---- Pass 2: resolve over opaque into composite target ----
    view.services->resources->scene_color_composite = builder.create_texture(RGTextureDesc{
        .width = view.viewport_width,
        .height = view.viewport_height,
        .format = rhi::Format::R16G16B16A16_SFLOAT,
        .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage,
        .debug_name = "SceneColorComposite"
    });

    RenderGraph& rg = builder.get_graph();
    const SceneRenderView* view_ptr = &view;
    rg.add_pass(
        "WboitOIT.Resolve",
        [view_ptr](RenderPassBuilder& resolve_builder) {
            auto& res = *view_ptr->services->resources;
            resolve_builder.set_color_attachment(0, res.scene_color_composite,
                                                 VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                                 core::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            resolve_builder.read_texture(res.scene_color_hdr, RGResourceAccess::ShaderRead);
            resolve_builder.read_texture(res.oit_accumulation, RGResourceAccess::ShaderRead);
            resolve_builder.read_texture(res.oit_revealage, RGResourceAccess::ShaderRead);
        },
        [this, view_ptr](RenderPassContext& resolve_ctx) {
            execute_resolve(resolve_ctx, *view_ptr);
        }
    );
}

void OitFeature::execute(RenderPassContext& ctx, const SceneRenderView& view) {
    if (!view.services || !view.services->resources) return;
    if (!view.services->resources->oit_accumulation.is_valid()) return;
    execute_accumulate(ctx, view);
}

void OitFeature::execute_accumulate(RenderPassContext& ctx, const SceneRenderView& view) {
    auto& cmd = ctx.get_command_buffer();
    if (cmd.get_handle() == VK_NULL_HANDLE) return;

    cmd.set_viewport(rhi::Viewport{ 0.0f, 0.0f, static_cast<float>(view.viewport_width), static_cast<float>(view.viewport_height), 0.0f, 1.0f });
    cmd.set_scissor(rhi::Rect2D{ 0, 0, view.viewport_width, view.viewport_height });
    cmd.bind_pipeline(m_accumulate_pipeline.get_pipeline());

    if (view.services->frame_uniforms && view.services->frame_uniforms->valid) {
        cmd.bind_descriptor_set(VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_accumulate_pipeline.get_layout(),
                                view.services->frame_uniforms->descriptor.get_handle());
    }

    const bool cull = view.services->settings ? view.services->settings->enable_frustum_culling : true;

    for_each_mesh_draw(
        view.scene, *view.services->meshes, view.camera.frustum, cull, view.services->stats,
        [](const scene::MaterialComponent* mat, const scene::MeshRendererComponent& mr) {
            (void)mr;
            return mat && mat->blend_mode == 2; // transparent only
        },
        [&](const MeshDrawRecord& rec) {
            MeshPushConstants pc{};
            pc.model = rec.world;
            pc.base_color = rec.base_color;
            pc.material_params = core::Vec4(rec.metallic, rec.emissive_strength, 0.0f, 0.0f);

            cmd.push_constants(m_accumulate_pipeline.get_layout(),
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(pc), &pc);

            cmd.bind_vertex_buffer(0, rec.mesh->get_vertex_buffer().get_handle());
            cmd.bind_index_buffer(rec.mesh->get_index_buffer().get_handle());
            cmd.draw_indexed(static_cast<uint32_t>(rec.mesh->get_indices().size()), 1, 0, 0, 0);
        });
}

void OitFeature::execute_resolve(RenderPassContext& ctx, const SceneRenderView& view) {
    auto& cmd = ctx.get_command_buffer();
    if (cmd.get_handle() == VK_NULL_HANDLE) return;

    auto& res = *view.services->resources;

    VkImageView opaque_view = ctx.get_texture_view(res.scene_color_hdr);
    VkImageView accum_view = ctx.get_texture_view(res.oit_accumulation);
    VkImageView reveal_view = ctx.get_texture_view(res.oit_revealage);
    if (opaque_view != m_bound_opaque_view) {
        m_resolve_descriptor.update_combined_image_sampler(0, opaque_view, m_linear_sampler.get_handle(),
                                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_bound_opaque_view = opaque_view;
    }
    if (accum_view != m_bound_accum_view) {
        m_resolve_descriptor.update_combined_image_sampler(1, accum_view, m_linear_sampler.get_handle(),
                                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_bound_accum_view = accum_view;
    }
    if (reveal_view != m_bound_reveal_view) {
        m_resolve_descriptor.update_combined_image_sampler(2, reveal_view, m_linear_sampler.get_handle(),
                                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_bound_reveal_view = reveal_view;
    }

    cmd.set_viewport(rhi::Viewport{ 0.0f, 0.0f, static_cast<float>(view.viewport_width), static_cast<float>(view.viewport_height), 0.0f, 1.0f });
    cmd.set_scissor(rhi::Rect2D{ 0, 0, view.viewport_width, view.viewport_height });
    cmd.bind_pipeline(m_resolve_pipeline.get_pipeline());
    cmd.bind_descriptor_set(VK_PIPELINE_BIND_POINT_GRAPHICS, m_resolve_pipeline.get_layout(), m_resolve_descriptor.get_handle());
    cmd.draw(3, 1, 0, 0);
}

} // namespace engine::renderer
