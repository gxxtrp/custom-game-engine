#include "engine/renderer/volumetrics_feature.h"
#include "engine/renderer/embedded_shaders.h"
#include "engine/renderer/scene_renderer.h"
#include "engine/core/log.h"

namespace engine::renderer {

struct alignas(16) VolumetricFogPushConstants {
    core::Mat4 inv_view_proj;
    core::Vec4 camera_pos;
    core::Vec4 light_dir;
    core::Vec4 fog_params; // x=scattering, y=absorption, z=anisotropy, w=height_falloff
};

bool VolumetricsFeature::ensure_pipeline() {
    if (m_initialized) return true;

    if (!m_vert_shader.init_from_spirv(shaders::TONEMAP_VERT_SPV, shaders::TONEMAP_VERT_SPV_SIZE, rhi::ShaderStage::Vertex) ||
        !m_frag_shader.init_from_spirv(shaders::VOLUMETRIC_FOG_FRAG_SPV, shaders::VOLUMETRIC_FOG_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment)) {
        LOG_FATAL("VolumetricFog", "Failed to create volumetric fog shader modules");
        return false;
    }

    rhi::SamplerDesc sampler_desc{};
    sampler_desc.min_filter = rhi::SamplerFilter::Nearest;
    sampler_desc.mag_filter = rhi::SamplerFilter::Nearest;
    sampler_desc.mipmap_mode = rhi::SamplerFilter::Nearest;
    sampler_desc.address_u = rhi::SamplerAddressMode::ClampToEdge;
    sampler_desc.address_v = rhi::SamplerAddressMode::ClampToEdge;
    sampler_desc.address_w = rhi::SamplerAddressMode::ClampToEdge;
    sampler_desc.enable_anisotropy = false;
    sampler_desc.debug_name = "VolumetricDepthSampler";
    if (!m_depth_sampler.init(sampler_desc)) {
        LOG_FATAL("VolumetricFog", "Failed to create depth sampler");
        return false;
    }

    std::vector<rhi::DescriptorBinding> bindings = {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    };
    if (!m_set_layout.init(bindings)) {
        LOG_FATAL("VolumetricFog", "Failed to create fog descriptor set layout");
        return false;
    }
    if (!m_pool.init(1, { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } }) ||
        !m_descriptor.init(m_pool, m_set_layout.get_handle())) {
        LOG_FATAL("VolumetricFog", "Failed to allocate fog descriptor set");
        return false;
    }

    rhi::GraphicsPipelineDesc desc{};
    desc.vertex_shader = &m_vert_shader;
    desc.fragment_shader = &m_frag_shader;
    desc.color_formats = { rhi::Format::R16G16B16A16_SFLOAT };
    desc.depth_format = rhi::Format::Undefined;
    desc.vertex_bindings = {};
    desc.vertex_attributes = {};
    desc.descriptor_set_layouts = { m_set_layout.get_handle() };

    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pc_range.offset = 0;
    pc_range.size = sizeof(VolumetricFogPushConstants);
    desc.push_constant_ranges = { pc_range };

    desc.depth_test_enable = false;
    desc.depth_write_enable = false;
    desc.cull_mode = VK_CULL_MODE_NONE;
    desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    if (!m_pipeline.init(desc)) {
        LOG_FATAL("VolumetricFog", "Failed to create volumetric fog pipeline");
        return false;
    }

    m_initialized = true;
    return true;
}

void VolumetricsFeature::setup(RenderPassBuilder& builder, const SceneRenderView& view) {
    if (!view.services || !view.services->resources) return;
    const GraphicsSettings* settings = view.services->settings;
    if (settings && !settings->enable_volumetric_fog) return;
    if (!view.services->resources->scene_depth.is_valid()) return;

    const uint32_t lut_w = std::max(1u, view.viewport_width / 2);
    const uint32_t lut_h = std::max(1u, view.viewport_height / 2);

    view.services->resources->volumetric_fog_lut = builder.create_texture(RGTextureDesc{
        .width = lut_w,
        .height = lut_h,
        .format = rhi::Format::R16G16B16A16_SFLOAT,
        .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled,
        .debug_name = "VolumetricFogLUT"
    });

    builder.set_color_attachment(0, view.services->resources->volumetric_fog_lut,
                                 VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                 core::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    builder.read_texture(view.services->resources->scene_depth, RGResourceAccess::ShaderRead);
}

void VolumetricsFeature::execute(RenderPassContext& ctx, const SceneRenderView& view) {
    if (!view.services || !view.services->resources) return;
    const GraphicsSettings* settings = view.services->settings;
    if (settings && !settings->enable_volumetric_fog) return;
    if (!view.services->resources->volumetric_fog_lut.is_valid()) return;
    if (!ensure_pipeline()) return;

    auto& cmd = ctx.get_command_buffer();
    if (cmd.get_handle() == VK_NULL_HANDLE) return;

    const uint32_t lut_w = std::max(1u, view.viewport_width / 2);
    const uint32_t lut_h = std::max(1u, view.viewport_height / 2);

    // Bind depth view (re-bind only when the physical view changes).
    VkImageView depth_view = ctx.get_texture_view(view.services->resources->scene_depth);
    if (depth_view != VK_NULL_HANDLE && depth_view != m_bound_depth_view) {
        m_descriptor.update_combined_image_sampler(0, depth_view, m_depth_sampler.get_handle(),
                                                   VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        m_bound_depth_view = depth_view;
    }

    cmd.set_viewport(rhi::Viewport{ 0.0f, 0.0f, static_cast<float>(lut_w), static_cast<float>(lut_h), 0.0f, 1.0f });
    cmd.set_scissor(rhi::Rect2D{ 0, 0, lut_w, lut_h });
    cmd.bind_pipeline(m_pipeline.get_pipeline());
    cmd.bind_descriptor_set(VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.get_layout(), m_descriptor.get_handle());

    VolumetricFogPushConstants pc{};
    pc.inv_view_proj = view.camera.view_proj.inverted();
    pc.camera_pos = core::Vec4(view.camera.position.x, view.camera.position.y, view.camera.position.z, view.camera.near_z);
    const auto& dir = view.services->lights ? view.services->lights->direction : core::Vec3(0.5f, 1.0f, 0.3f);
    pc.light_dir = core::Vec4(dir.x, dir.y, dir.z, 0.0f);
    pc.fog_params = core::Vec4(0.02f, 0.005f, 0.7f, 0.1f);

    cmd.push_constants(m_pipeline.get_layout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
    cmd.draw(3, 1, 0, 0);
}

} // namespace engine::renderer
