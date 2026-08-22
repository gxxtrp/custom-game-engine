#include "engine/renderer/taa_feature.h"
#include "engine/renderer/embedded_shaders.h"
#include "engine/renderer/scene_renderer.h"
#include "engine/core/log.h"

namespace engine::renderer {

struct alignas(16) TaaPushConstants {
    core::Vec2 jitter;   // applied projection jitter (UV space)
    core::Vec2 texel;    // 1/size
};

bool TaaFeature::ensure_pipeline() {
    if (m_initialized) return true;

    if (!m_vert_shader.init_from_spirv(shaders::TONEMAP_VERT_SPV, shaders::TONEMAP_VERT_SPV_SIZE, rhi::ShaderStage::Vertex) ||
        !m_frag_shader.init_from_spirv(shaders::TAA_RESOLVE_FRAG_SPV, shaders::TAA_RESOLVE_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment)) {
        LOG_FATAL("TaaResolve", "Failed to create TAA shader modules");
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
    sampler_desc.debug_name = "TaaLinearSampler";
    if (!m_linear_sampler.init(sampler_desc)) {
        LOG_FATAL("TaaResolve", "Failed to create TAA sampler");
        return false;
    }

    std::vector<rhi::DescriptorBinding> bindings = {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // current color
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // history
        { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // velocity
    };
    if (!m_set_layout.init(bindings)) {
        LOG_FATAL("TaaResolve", "Failed to create TAA descriptor set layout");
        return false;
    }
    if (!m_pool.init(1, { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 } }) ||
        !m_descriptor.init(m_pool, m_set_layout.get_handle())) {
        LOG_FATAL("TaaResolve", "Failed to allocate TAA descriptor set");
        return false;
    }

    rhi::GraphicsPipelineDesc desc{};
    desc.vertex_shader = &m_vert_shader;
    desc.fragment_shader = &m_frag_shader;
    desc.color_formats = { rhi::Format::R16G16B16A16_SFLOAT, rhi::Format::R16G16B16A16_SFLOAT }; // resolved + history write
    desc.depth_format = rhi::Format::Undefined;
    desc.vertex_bindings = {};
    desc.vertex_attributes = {};
    desc.descriptor_set_layouts = { m_set_layout.get_handle() };

    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pc_range.offset = 0;
    pc_range.size = sizeof(TaaPushConstants);
    desc.push_constant_ranges = { pc_range };

    desc.depth_test_enable = false;
    desc.depth_write_enable = false;
    desc.cull_mode = VK_CULL_MODE_NONE;
    desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (!m_pipeline.init(desc)) {
        LOG_FATAL("TaaResolve", "Failed to create TAA resolve pipeline");
        return false;
    }

    m_initialized = true;
    return true;
}

void TaaFeature::setup(RenderPassBuilder& builder, const SceneRenderView& view) {
    if (!view.services || !view.services->resources) return;
    const GraphicsSettings* settings = view.services->settings;
    if (settings && !settings->enable_taa) return;
    if (!view.services->resources->scene_color_composite.is_valid()) return;
    if (!view.services->resources->velocity_buffer.is_valid()) return;
    if (!ensure_pipeline()) return; // pipelines ready before command recording

    // Persistent history (owned by TaaSystem, imported as external).
    if (!m_system.get_history_texture(0).is_valid()) {
        m_system.init(view.viewport_width, view.viewport_height);
    } else if (m_system.get_history_texture(0).get_width() != view.viewport_width ||
               m_system.get_history_texture(0).get_height() != view.viewport_height) {
        m_system.resize(view.viewport_width, view.viewport_height);
    }

    RenderGraph& rg = builder.get_graph();
    const uint32_t read_idx = m_system.get_current_history_index();
    const uint32_t write_idx = (read_idx + 1) % 2;

    const auto& read_tex = m_system.get_history_texture(read_idx);
    const auto& write_tex = m_system.get_history_texture(write_idx);

    view.services->resources->taa_history_read = rg.import_texture(
        "TaaHistoryRead", read_tex.get_handle(), read_tex.get_view(),
        read_tex.get_width(), read_tex.get_height(), rhi::Format::R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED);
    view.services->resources->taa_history_write = rg.import_texture(
        "TaaHistoryWrite", write_tex.get_handle(), write_tex.get_view(),
        write_tex.get_width(), write_tex.get_height(), rhi::Format::R16G16B16A16_SFLOAT,
        VK_IMAGE_LAYOUT_UNDEFINED);

    // Resolved output.
    view.services->resources->scene_color_taa = builder.create_texture(RGTextureDesc{
        .width = view.viewport_width,
        .height = view.viewport_height,
        .format = rhi::Format::R16G16B16A16_SFLOAT,
        .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled,
        .debug_name = "SceneColorTAA"
    });

    builder.set_color_attachment(0, view.services->resources->scene_color_taa,
                                 VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                 core::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    builder.set_color_attachment(1, view.services->resources->taa_history_write,
                                 VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                 core::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    builder.read_texture(view.services->resources->scene_color_composite, RGResourceAccess::ShaderRead);
    builder.read_texture(view.services->resources->taa_history_read, RGResourceAccess::ShaderRead);
    builder.read_texture(view.services->resources->velocity_buffer, RGResourceAccess::ShaderRead);
}

void TaaFeature::execute(RenderPassContext& ctx, const SceneRenderView& view) {
    if (!view.services || !view.services->resources) return;
    const GraphicsSettings* settings = view.services->settings;
    if (settings && !settings->enable_taa) return;
    if (!view.services->resources->scene_color_taa.is_valid()) return;

    auto& cmd = ctx.get_command_buffer();
    if (cmd.get_handle() == VK_NULL_HANDLE) return;

    auto& res = *view.services->resources;

    VkImageView current_view = ctx.get_texture_view(res.scene_color_composite);
    VkImageView history_view = ctx.get_texture_view(res.taa_history_read);
    VkImageView velocity_view = ctx.get_texture_view(res.velocity_buffer);

    if (current_view != m_bound_current) {
        m_descriptor.update_combined_image_sampler(0, current_view, m_linear_sampler.get_handle(),
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_bound_current = current_view;
    }
    if (history_view != m_bound_history) {
        m_descriptor.update_combined_image_sampler(1, history_view, m_linear_sampler.get_handle(),
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_bound_history = history_view;
    }
    if (velocity_view != m_bound_velocity) {
        m_descriptor.update_combined_image_sampler(2, velocity_view, m_linear_sampler.get_handle(),
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_bound_velocity = velocity_view;
    }

    cmd.set_viewport(rhi::Viewport{ 0.0f, 0.0f, static_cast<float>(view.viewport_width), static_cast<float>(view.viewport_height), 0.0f, 1.0f });
    cmd.set_scissor(rhi::Rect2D{ 0, 0, view.viewport_width, view.viewport_height });
    cmd.bind_pipeline(m_pipeline.get_pipeline());
    cmd.bind_descriptor_set(VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.get_layout(), m_descriptor.get_handle());

    TaaPushConstants pc{};
    pc.jitter = TaaSystem::get_jitter(view.services->frame_index);
    pc.texel = core::Vec2(1.0f / static_cast<float>(view.viewport_width),
                          1.0f / static_cast<float>(view.viewport_height));
    cmd.push_constants(m_pipeline.get_layout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
    cmd.draw(3, 1, 0, 0);
}

void TaaFeature::post_frame(const SceneRenderView& view) {
    const GraphicsSettings* settings = view.services ? view.services->settings : nullptr;
    if (settings && !settings->enable_taa) return;
    m_system.advance_frame();
}

} // namespace engine::renderer
