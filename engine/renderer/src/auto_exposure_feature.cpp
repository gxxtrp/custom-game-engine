#include "engine/renderer/auto_exposure_feature.h"
#include "engine/renderer/embedded_shaders.h"
#include "engine/renderer/scene_renderer.h"
#include "engine/core/log.h"
#include <algorithm>
#include <cmath>

namespace engine::renderer {

bool AutoExposureFeature::ensure_pipeline() {
    if (m_initialized) return true;

    if (!m_system.init()) {
        LOG_FATAL("AutoExposure", "Failed to initialize auto exposure system");
        return false;
    }

    if (!m_compute_shader.init_from_spirv(shaders::AUTO_EXPOSURE_COMP_SPV, shaders::AUTO_EXPOSURE_COMP_SPV_SIZE, rhi::ShaderStage::Compute)) {
        LOG_FATAL("AutoExposure", "Failed to create auto exposure compute shader");
        return false;
    }

    std::vector<rhi::DescriptorBinding> bindings = {
        { 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT },  // scene color
        { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT }, // histogram
    };
    if (!m_set_layout.init(bindings)) {
        LOG_FATAL("AutoExposure", "Failed to create auto exposure descriptor set layout");
        return false;
    }
    if (!m_pool.init(1, { { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } }) ||
        !m_descriptor.init(m_pool, m_set_layout.get_handle())) {
        LOG_FATAL("AutoExposure", "Failed to allocate auto exposure descriptor set");
        return false;
    }

    rhi::ComputePipelineDesc desc{};
    desc.compute_shader = &m_compute_shader;
    desc.descriptor_set_layouts = { m_set_layout.get_handle() };
    if (!m_pipeline.init(desc)) {
        LOG_FATAL("AutoExposure", "Failed to create auto exposure compute pipeline");
        return false;
    }

    m_initialized = true;
    return true;
}

void AutoExposureFeature::setup(RenderPassBuilder& builder, const SceneRenderView& view) {
    if (!view.services || !view.services->resources) return;
    const GraphicsSettings* settings = view.services->settings;
    if (settings && !settings->enable_auto_exposure) return;

    // Runs before TAA in the post stack; measures the OIT-resolved composite.
    RGTextureHandle scene_color = view.services->resources->scene_color_composite;
    if (!scene_color.is_valid()) return;

    // Read-only histogram pass: sampled access needs no GENERAL layout transition.
    builder.read_texture(scene_color, RGResourceAccess::ShaderRead);
}

void AutoExposureFeature::execute(RenderPassContext& ctx, const SceneRenderView& view) {
    if (!view.services || !view.services->resources) return;
    const GraphicsSettings* settings = view.services->settings;
    if (settings && !settings->enable_auto_exposure) return;
    if (!ensure_pipeline()) return;

    RGTextureHandle scene_color = view.services->resources->scene_color_composite;
    if (!scene_color.is_valid()) return;

    auto& cmd = ctx.get_command_buffer();
    if (cmd.get_handle() == VK_NULL_HANDLE) return;

    VkImageView scene_view = ctx.get_texture_view(scene_color);
    if (scene_view == VK_NULL_HANDLE) return;

    if (scene_view != m_bound_view) {
        m_descriptor.update_sampled_image(0, scene_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_descriptor.update_storage_buffer(1, m_system.get_histogram_buffer().get_handle(),
                                           AutoExposureSystem::HISTOGRAM_BINS * sizeof(uint32_t));
        m_bound_view = scene_view;
    }

    const uint32_t group_x = std::max(1u, (view.viewport_width + 15) / 16);
    const uint32_t group_y = std::max(1u, (view.viewport_height + 15) / 16);

    cmd.fill_buffer(m_system.get_histogram_buffer().get_handle(),
                    AutoExposureSystem::HISTOGRAM_BINS * sizeof(uint32_t), 0);
    cmd.bind_pipeline(m_pipeline.get_pipeline(), VK_PIPELINE_BIND_POINT_COMPUTE);
    cmd.bind_descriptor_set(VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline.get_layout(), m_descriptor.get_handle());
    cmd.dispatch(group_x, group_y, 1);
    cmd.copy_buffer(m_system.get_histogram_buffer().get_handle(),
                    m_system.get_readback_buffer().get_handle(),
                    AutoExposureSystem::HISTOGRAM_BINS * sizeof(uint32_t));

    m_histogram_written = true;
}

void AutoExposureFeature::post_frame(const SceneRenderView& view) {
    if (!view.services || !m_histogram_written) return;
    const GraphicsSettings* settings = view.services->settings;
    if (settings && !settings->enable_auto_exposure) return;

    std::vector<uint32_t> bins;
    if (!m_system.readback_histogram(bins)) return;

    float avg_luma = AutoExposureSystem::compute_average_luminance(bins);
    m_system.set_target_luminance(avg_luma);
    m_system.update(view.delta_time > 0.0f ? view.delta_time : 1.0f / 60.0f);
    view.services->adapted_exposure = m_system.get_exposure();
}

} // namespace engine::renderer
