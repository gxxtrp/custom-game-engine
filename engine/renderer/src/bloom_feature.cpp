#include "engine/renderer/bloom_feature.h"
#include "engine/renderer/embedded_shaders.h"
#include "engine/renderer/scene_renderer.h"
#include "engine/core/log.h"
#include <algorithm>

namespace engine::renderer {

struct alignas(16) BloomPushConstants {
    core::Vec4 params;   // x=threshold, y=soft knee, z=intensity, w=filter radius
    core::Vec2 texel;    // 1/width, 1/height
    core::Vec2 pad;
};

bool BloomFeature::ensure_pipelines() {
    if (m_initialized) return true;

    if (!m_vert_shader.init_from_spirv(shaders::TONEMAP_VERT_SPV, shaders::TONEMAP_VERT_SPV_SIZE, rhi::ShaderStage::Vertex) ||
        !m_prefilter_frag.init_from_spirv(shaders::BLOOM_PREFILTER_FRAG_SPV, shaders::BLOOM_PREFILTER_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment) ||
        !m_downsample_frag.init_from_spirv(shaders::BLOOM_DOWNSAMPLE_FRAG_SPV, shaders::BLOOM_DOWNSAMPLE_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment) ||
        !m_upsample_frag.init_from_spirv(shaders::BLOOM_UPSAMPLE_FRAG_SPV, shaders::BLOOM_UPSAMPLE_FRAG_SPV_SIZE, rhi::ShaderStage::Fragment)) {
        LOG_FATAL("Bloom", "Failed to create bloom shader modules");
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
    sampler_desc.debug_name = "BloomLinearSampler";
    if (!m_linear_sampler.init(sampler_desc)) {
        LOG_FATAL("Bloom", "Failed to create bloom sampler");
        return false;
    }

    std::vector<rhi::DescriptorBinding> bindings = {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    };
    if (!m_set_layout.init(bindings)) {
        LOG_FATAL("Bloom", "Failed to create bloom descriptor set layout");
        return false;
    }
    // 11 persistent sets: 1 prefilter + 5 downsample + 5 upsample (b1 only used
    // by upsample). Allocated once — reallocating per frame would exhaust the pool.
    if (!m_pool.init(MAX_BLOOM_PASSES, { { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 24 } })) {
        LOG_FATAL("Bloom", "Failed to create bloom descriptor pool");
        return false;
    }
    for (auto& set : m_pass_sets) {
        if (!set.init(m_pool, m_set_layout.get_handle())) {
            LOG_FATAL("Bloom", "Failed to allocate bloom descriptor set");
            return false;
        }
    }

    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pc_range.offset = 0;
    pc_range.size = sizeof(BloomPushConstants);

    auto make_desc = [&](rhi::RhiShaderModule* frag) {
        rhi::GraphicsPipelineDesc desc{};
        desc.vertex_shader = &m_vert_shader;
        desc.fragment_shader = frag;
        desc.color_formats = { rhi::Format::R16G16B16A16_SFLOAT };
        desc.depth_format = rhi::Format::Undefined;
        desc.vertex_bindings = {};
        desc.vertex_attributes = {};
        desc.descriptor_set_layouts = { m_set_layout.get_handle() };
        desc.push_constant_ranges = { pc_range };
        desc.depth_test_enable = false;
        desc.depth_write_enable = false;
        desc.cull_mode = VK_CULL_MODE_NONE;
        desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        return desc;
    };

    if (!m_prefilter_pipeline.init(make_desc(&m_prefilter_frag)) ||
        !m_downsample_pipeline.init(make_desc(&m_downsample_frag)) ||
        !m_upsample_pipeline.init(make_desc(&m_upsample_frag))) {
        LOG_FATAL("Bloom", "Failed to create bloom pipelines");
        return false;
    }

    m_initialized = true;
    return true;
}

void BloomFeature::setup(RenderPassBuilder& builder, const SceneRenderView& view) {
    if (!view.services || !view.services->resources) return;
    const GraphicsSettings* settings = view.services->settings;
    if (settings && !settings->enable_bloom) return;

    // Input: TAA-resolved color when available, otherwise the OIT composite.
    RGTextureHandle scene_color = view.services->resources->scene_color_taa.is_valid()
        ? view.services->resources->scene_color_taa
        : view.services->resources->scene_color_composite;
    if (!scene_color.is_valid()) return;

    m_passes.clear();
    m_passes.reserve(11);
    m_mips.clear();
    m_mips.reserve(6);

    // ---- Build the mip chain (6 levels, half-resolution each) ----
    for (uint32_t i = 0; i < 6; ++i) {
        m_mips.push_back(builder.create_texture(RGTextureDesc{
            .width = std::max(1u, view.viewport_width >> (i + 1)),
            .height = std::max(1u, view.viewport_height >> (i + 1)),
            .format = rhi::Format::R16G16B16A16_SFLOAT,
            .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled,
            .debug_name = "BloomMip" + std::to_string(i)
        }));
    }

    auto make_pass = [&](int32_t type, RGTextureHandle in_a, RGTextureHandle in_b, RGTextureHandle out, uint32_t w, uint32_t h) {
        BloomPassState state{};
        state.type = type;
        state.input_a = in_a;
        state.input_b = in_b;
        state.output = out;
        state.width = w;
        state.height = h;
        state.descriptor = (m_passes.size() < m_pass_sets.size()) ? &m_pass_sets[m_passes.size()] : nullptr;
        m_passes.push_back(std::move(state));
    };

    // Prefilter: scene color -> mip0
    make_pass(0, scene_color, RGTextureHandle{}, m_mips[0],
              std::max(1u, view.viewport_width >> 1),
              std::max(1u, view.viewport_height >> 1));
    // Downsample: mip[i] -> mip[i+1]
    for (uint32_t i = 0; i < 5; ++i) {
        make_pass(1, m_mips[i], RGTextureHandle{}, m_mips[i + 1],
                  std::max(1u, view.viewport_width >> (i + 2)),
                  std::max(1u, view.viewport_height >> (i + 2)));
    }
    // Upsample with temporary chain to avoid read-modify-write hazards:
    // temps[i] size == mips[i] size; result reuses mips[0].
    std::vector<RGTextureHandle> temps(5);
    for (uint32_t i = 0; i < 5; ++i) {
        temps[i] = builder.create_texture(RGTextureDesc{
            .width = std::max(1u, view.viewport_width >> (i + 1)),
            .height = std::max(1u, view.viewport_height >> (i + 1)),
            .format = rhi::Format::R16G16B16A16_SFLOAT,
            .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled,
            .debug_name = "BloomUpTemp" + std::to_string(i)
        });
    }
    make_pass(2, m_mips[5], m_mips[4], temps[4],
              std::max(1u, view.viewport_width >> 5), std::max(1u, view.viewport_height >> 5));
    for (int32_t i = 3; i >= 1; --i) {
        make_pass(2, temps[i + 1], m_mips[i], temps[i],
                  std::max(1u, view.viewport_width >> (i + 1)), std::max(1u, view.viewport_height >> (i + 1)));
    }
    // Final: temps[1] + mips[1] -> mips[0] (bloom result at half resolution)
    view.services->resources->bloom_result = m_mips[0];
    make_pass(2, temps[1], m_mips[1], m_mips[0],
              std::max(1u, view.viewport_width >> 1), std::max(1u, view.viewport_height >> 1));

    // ---- Register sub-passes ----
    RenderGraph& rg = builder.get_graph();
    const SceneRenderView* view_ptr = &view;
    for (size_t i = 0; i < m_passes.size(); ++i) {
        const size_t pass_index = i;
        rg.add_pass(
            "Bloom.SubPass" + std::to_string(i),
            [this, pass_index](RenderPassBuilder& sub_builder) {
                const auto& pass = m_passes[pass_index];
                sub_builder.set_color_attachment(0, pass.output,
                                                 VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                                 core::Vec4(0.0f, 0.0f, 0.0f, 0.0f));
                sub_builder.read_texture(pass.input_a, RGResourceAccess::ShaderRead);
                if (pass.input_b.is_valid()) {
                    sub_builder.read_texture(pass.input_b, RGResourceAccess::ShaderRead);
                }
            },
            [this, pass_index, view_ptr](RenderPassContext& sub_ctx) {
                execute_pass(sub_ctx, *view_ptr, m_passes[pass_index]);
            }
        );
    }
}

void BloomFeature::execute(RenderPassContext& ctx, const SceneRenderView& view) {
    (void)ctx;
    (void)view;
    // All bloom work runs in the sub-passes registered during setup().
}

void BloomFeature::execute_pass(RenderPassContext& ctx, const SceneRenderView& view, BloomPassState& pass) {
    if (!ensure_pipelines()) return;
    if (!pass.descriptor) return;

    auto& cmd = ctx.get_command_buffer();
    if (cmd.get_handle() == VK_NULL_HANDLE) return;

    VkImageView view_a = ctx.get_texture_view(pass.input_a);
    if (view_a == VK_NULL_HANDLE) return;
    if (view_a != pass.bound_a) {
        pass.descriptor->update_combined_image_sampler(0, view_a, m_linear_sampler.get_handle(),
                                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        pass.bound_a = view_a;
    }
    if (pass.input_b.is_valid()) {
        VkImageView view_b = ctx.get_texture_view(pass.input_b);
        if (view_b != pass.bound_b && view_b != VK_NULL_HANDLE) {
            pass.descriptor->update_combined_image_sampler(1, view_b, m_linear_sampler.get_handle(),
                                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            pass.bound_b = view_b;
        }
    }

    rhi::RhiGraphicsPipeline* pipeline = nullptr;
    switch (pass.type) {
        case 0: pipeline = &m_prefilter_pipeline; break;
        case 1: pipeline = &m_downsample_pipeline; break;
        default: pipeline = &m_upsample_pipeline; break;
    }

    cmd.set_viewport(rhi::Viewport{ 0.0f, 0.0f, static_cast<float>(pass.width), static_cast<float>(pass.height), 0.0f, 1.0f });
    cmd.set_scissor(rhi::Rect2D{ 0, 0, pass.width, pass.height });
    cmd.bind_pipeline(pipeline->get_pipeline());
    cmd.bind_descriptor_set(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->get_layout(), pass.descriptor->get_handle());

    BloomPushConstants pc{};
    const BloomParams& p = m_params;
    pc.params = core::Vec4(p.threshold, p.soft_knee, p.intensity, p.filter_radius);
    pc.texel = core::Vec2(1.0f / static_cast<float>(std::max(1u, pass.width)),
                          1.0f / static_cast<float>(std::max(1u, pass.height)));
    cmd.push_constants(pipeline->get_layout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
    cmd.draw(3, 1, 0, 0);
}

} // namespace engine::renderer
