#include "engine/renderer/post_process_feature.h"
#include "engine/renderer/embedded_shaders.h"
#include "engine/renderer/scene_renderer.h"
#include "engine/core/log.h"

namespace engine::renderer {

struct alignas(16) CompositePushConstants {
    float exposure{1.0f};
    int32_t tone_mapper{0};          // 0=ACES, 1=AgX, 2=Neutral, 3=Reinhard, 4=Linear
    float bloom_intensity{0.05f};
    float vignette_intensity{0.25f};
    float saturation{1.0f};
    float contrast{1.0f};
    float grain{0.02f};
    float fog_enabled{1.0f};
};

bool PostProcessCompositeFeature::ensure_pipeline(rhi::Format target_format) {
    if (m_initialized && m_pipeline_format == target_format) return true;

    if (m_initialized) {
        m_pipeline.destroy();
        m_initialized = false;
    }

    if (!m_vert_shader.init_from_spirv(shaders::TONEMAP_VERT_SPV, shaders::TONEMAP_VERT_SPV_SIZE, rhi::ShaderStage::Vertex) ||
        !m_frag_shader.init_from_spirv(shaders::COMPOSITE_FRAG_SPV, shaders::COMPOSITE_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment)) {
        LOG_FATAL("PostProcessComposite", "Failed to create composite shader modules");
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
    sampler_desc.debug_name = "CompositeSampler";
    if (!m_linear_sampler.init(sampler_desc)) {
        LOG_FATAL("PostProcessComposite", "Failed to create composite sampler");
        return false;
    }

    // Dummy textures keep all three bindings validly bound when a pass is disabled.
    rhi::TextureDesc black_desc{};
    black_desc.width = 1;
    black_desc.height = 1;
    black_desc.format = rhi::Format::R16G16B16A16_SFLOAT;
    black_desc.usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled;
    black_desc.debug_name = "CompositeDummyBlack";
    if (!m_dummy_black.init(black_desc)) {
        LOG_FATAL("PostProcessComposite", "Failed to create dummy black texture");
        return false;
    }
    rhi::TextureDesc transparent_desc = black_desc;
    transparent_desc.debug_name = "CompositeDummyTransparent";
    if (!m_dummy_transparent.init(transparent_desc)) {
        LOG_FATAL("PostProcessComposite", "Failed to create dummy transparent texture");
        return false;
    }

    std::vector<rhi::DescriptorBinding> bindings = {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // scene color
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // bloom
        { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // volumetric fog
    };
    if (!m_set_layout.init(bindings)) {
        LOG_FATAL("PostProcessComposite", "Failed to create composite descriptor set layout");
        return false;
    }
    if (!m_pool.init(1, { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 } }) ||
        !m_descriptor.init(m_pool, m_set_layout.get_handle())) {
        LOG_FATAL("PostProcessComposite", "Failed to allocate composite descriptor set");
        return false;
    }

    rhi::GraphicsPipelineDesc desc{};
    desc.vertex_shader = &m_vert_shader;
    desc.fragment_shader = &m_frag_shader;
    desc.color_formats = { target_format };
    desc.depth_format = rhi::Format::Undefined;
    desc.vertex_bindings = {};
    desc.vertex_attributes = {};
    desc.descriptor_set_layouts = { m_set_layout.get_handle() };

    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pc_range.offset = 0;
    pc_range.size = sizeof(CompositePushConstants);
    desc.push_constant_ranges = { pc_range };

    desc.depth_test_enable = false;
    desc.depth_write_enable = false;
    desc.cull_mode = VK_CULL_MODE_NONE;
    desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (!m_pipeline.init(desc)) {
        LOG_FATAL("PostProcessComposite", "Failed to create composite pipeline");
        return false;
    }

    m_pipeline_format = target_format;
    m_initialized = true;
    return true;
}

void PostProcessCompositeFeature::setup(RenderPassBuilder& builder, const SceneRenderView& view) {
    if (!view.services || !view.services->resources) return;
    if (!view.final_target.is_valid()) return;

    RGTextureHandle scene_color = view.services->resources->scene_color_taa.is_valid()
        ? view.services->resources->scene_color_taa
        : view.services->resources->scene_color_composite;
    if (!scene_color.is_valid()) return;

    builder.set_color_attachment(0, view.services->resources->final_target_rg,
                                 VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                 core::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    builder.read_texture(scene_color, RGResourceAccess::ShaderRead);
    if (view.services->resources->bloom_result.is_valid()) {
        builder.read_texture(view.services->resources->bloom_result, RGResourceAccess::ShaderRead);
    }
    if (view.services->resources->volumetric_fog_lut.is_valid()) {
        builder.read_texture(view.services->resources->volumetric_fog_lut, RGResourceAccess::ShaderRead);
    }
}

void PostProcessCompositeFeature::execute(RenderPassContext& ctx, const SceneRenderView& view) {
    if (!view.services || !view.services->resources) return;
    if (!view.final_target.is_valid()) return;

    auto& cmd = ctx.get_command_buffer();
    if (cmd.get_handle() == VK_NULL_HANDLE) return;

    const GraphicsSettings& settings = view.services->settings ? *view.services->settings : GraphicsSettings{};
    if (!settings.enable_post_process) return;
    if (!ensure_pipeline(view.final_target.format)) return;

    auto& res = *view.services->resources;

    RGTextureHandle scene_color = res.scene_color_taa.is_valid() ? res.scene_color_taa : res.scene_color_composite;

    VkImageView scene_view = ctx.get_texture_view(scene_color);
    VkImageView bloom_view = res.bloom_result.is_valid() ? ctx.get_texture_view(res.bloom_result) : m_dummy_black.get_view();
    VkImageView fog_view = res.volumetric_fog_lut.is_valid() ? ctx.get_texture_view(res.volumetric_fog_lut) : m_dummy_transparent.get_view();

    if (scene_view != m_bound_scene) {
        m_descriptor.update_combined_image_sampler(0, scene_view, m_linear_sampler.get_handle(),
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_bound_scene = scene_view;
    }
    if (bloom_view != m_bound_bloom) {
        m_descriptor.update_combined_image_sampler(1, bloom_view, m_linear_sampler.get_handle(),
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_bound_bloom = bloom_view;
    }
    if (fog_view != m_bound_fog) {
        m_descriptor.update_combined_image_sampler(2, fog_view, m_linear_sampler.get_handle(),
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_bound_fog = fog_view;
    }

    cmd.set_viewport(rhi::Viewport{ 0.0f, 0.0f, static_cast<float>(view.viewport_width), static_cast<float>(view.viewport_height), 0.0f, 1.0f });
    cmd.set_scissor(rhi::Rect2D{ 0, 0, view.viewport_width, view.viewport_height });
    cmd.bind_pipeline(m_pipeline.get_pipeline());
    cmd.bind_descriptor_set(VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.get_layout(), m_descriptor.get_handle());

    CompositePushConstants pc{};
    pc.exposure = settings.enable_auto_exposure ? view.services->adapted_exposure : settings.exposure;
    pc.tone_mapper = static_cast<int32_t>(settings.tone_mapper);
    pc.bloom_intensity = settings.enable_bloom ? settings.bloom_intensity : 0.0f;
    pc.vignette_intensity = settings.enable_vignette ? settings.vignette_intensity : 0.0f;
    pc.saturation = 1.0f;
    pc.contrast = 1.0f;
    pc.grain = 0.02f;
    pc.fog_enabled = (settings.enable_volumetric_fog && res.volumetric_fog_lut.is_valid()) ? 1.0f : 0.0f;

    cmd.push_constants(m_pipeline.get_layout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
    cmd.draw(3, 1, 0, 0);
}

} // namespace engine::renderer
