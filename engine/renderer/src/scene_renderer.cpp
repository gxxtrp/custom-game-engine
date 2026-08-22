#include "engine/renderer/scene_renderer.h"
#include "engine/renderer/camera.h"
#include "engine/renderer/taa.h"
#include "engine/core/log.h"
#include "engine/rhi/rhi_context.h"
#include "engine/scene/components.h"
#include <algorithm>
#include <numeric>

// Built-in feature adapters (registered on the GPU path).
#include "engine/renderer/depth_pre_pass_feature.h"
#include "engine/renderer/forward_opaque_feature.h"
#include "engine/renderer/cascaded_shadow_feature.h"
#include "engine/renderer/clustered_lighting_feature.h"
#include "engine/renderer/volumetrics_feature.h"
#include "engine/renderer/oit_feature.h"
#include "engine/renderer/auto_exposure_feature.h"
#include "engine/renderer/taa_feature.h"
#include "engine/renderer/bloom_feature.h"
#include "engine/renderer/post_process_feature.h"

namespace engine::renderer {

SceneRenderer::SceneRenderer(rhi::RhiContext* rhi) : m_rhi(rhi) {
    LOG_INFO("SceneRenderer", "SceneRenderer constructed (RHI context: {})",
             m_rhi != nullptr ? "present" : "none (CPU/test mode)");
}

SceneRenderer::~SceneRenderer() {
    // Reverse order teardown
    m_features_by_stage.clear();
    m_features_by_name.clear();
    m_mesh_resolver.clear();

    if (m_gpu_ready) {
        m_render_graph.destroy();
        m_cmd_buffer.destroy(m_cmd_pool.get_handle());
        m_cmd_pool.destroy();
        m_frame_fence.destroy();
        for (auto& sem : m_render_finished_semaphores) sem.destroy();
        m_frame_uniforms.descriptor.destroy(m_frame_uniforms.pool);
        m_frame_uniforms.pool.destroy();
        m_frame_uniforms.set_layout.destroy();
        m_frame_uniforms.shadow_sampler.destroy();
        m_frame_uniforms.ubo.destroy();
    }
    LOG_INFO("SceneRenderer", "SceneRenderer destroyed cleanly");
}

// ==========================================
// Feature registration seam
// ==========================================

void SceneRenderer::register_feature(std::shared_ptr<IRenderFeature> feature) {
    if (!feature) return;

    // Replace semantics: a name maps to at most one active feature.
    unregister_feature(feature->get_name());

    m_features_by_stage[feature->get_stage()].push_back(feature);
    m_features_by_name.emplace(std::string(feature->get_name()), std::move(feature));
}

void SceneRenderer::unregister_feature(std::string_view feature_name) {
    auto it = m_features_by_name.find(std::string(feature_name));
    if (it == m_features_by_name.end()) return;

    auto feature = it->second;
    m_features_by_name.erase(it);

    auto stage_it = m_features_by_stage.find(feature->get_stage());
    if (stage_it != m_features_by_stage.end()) {
        auto& vec = stage_it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), feature), vec.end());
    }
}

IRenderFeature* SceneRenderer::get_feature(std::string_view feature_name) const {
    auto it = m_features_by_name.find(std::string(feature_name));
    return (it != m_features_by_name.end()) ? it->second.get() : nullptr;
}

void SceneRenderer::register_mesh(const assets::UUID& uuid, std::shared_ptr<Mesh> mesh) {
    m_mesh_resolver.register_mesh(uuid, std::move(mesh));
}

std::shared_ptr<Mesh> SceneRenderer::get_mesh(const assets::UUID& uuid) const {
    return m_mesh_resolver.get_mesh(uuid);
}

VkSemaphore SceneRenderer::get_render_finished_semaphore() const {
    if (!m_gpu_ready || m_render_finished_semaphores.empty()) return VK_NULL_HANDLE;
    return m_render_finished_semaphores[m_render_finished_index % m_render_finished_semaphores.size()].get_handle();
}

void SceneRenderer::set_frames_in_flight(uint32_t count) {
    const uint32_t n = count > 0 ? count : 1;
    if (n == m_frames_in_flight && !m_render_finished_semaphores.empty()) return;
    m_frames_in_flight = n;
    if (m_gpu_ready) {
        // Caller guarantees the queue is idle, so no present can still be
        // waiting on any of these slots.
        for (auto& sem : m_render_finished_semaphores) sem.destroy();
        m_render_finished_semaphores.resize(n);
        for (auto& sem : m_render_finished_semaphores) {
            if (!sem.init(false)) {
                LOG_FATAL("SceneRenderer", "Failed to recreate render-finished semaphore");
            }
        }
        m_render_finished_index = 0;
    }
}

// ==========================================
// Frame state extraction
// ==========================================

void SceneRenderer::extract_frame_lights(const scene::Scene& scene) {
    m_lights = FrameLightState{};

    const_cast<flecs::world&>(scene.get_world())
        .query_builder<const scene::DirectionalLightComponent>()
        .build()
        .each([&](flecs::entity e, const scene::DirectionalLightComponent& light) {
            if (m_lights.has_directional) return;
            m_lights.has_directional = true;
            m_lights.color = light.color;
            m_lights.intensity = light.intensity;
            m_lights.cast_shadows = light.cast_shadows;
            m_lights.cascade_count = light.cascade_count;

            const scene::TransformComponent* t = e.has<scene::TransformComponent>() ? &e.get<scene::TransformComponent>() : nullptr;
            if (t) {
                core::Vec3 dir = t->rotation.rotate(core::Vec3(0.0f, -1.0f, 0.0f)).normalized();
                if (dir.length_sq() > 0.0001f) {
                    m_lights.direction = dir;
                }
            }
        });
}

void SceneRenderer::extract_point_and_spot_lights(const scene::Scene& scene) {
    m_lights.point_lights.clear();
    m_lights.spot_lights.clear();

    const_cast<flecs::world&>(scene.get_world())
        .query_builder<const scene::PointLightComponent, const scene::TransformComponent>()
        .build()
        .each([&](flecs::entity, const scene::PointLightComponent& light, const scene::TransformComponent& t) {
            GPUPointLight gpu{};
            gpu.position = t.position;
            gpu.color = light.color;
            gpu.intensity = light.intensity;
            gpu.radius = light.radius;
            gpu.falloff = light.falloff;
            m_lights.point_lights.push_back(gpu);
        });

    const_cast<flecs::world&>(scene.get_world())
        .query_builder<const scene::SpotLightComponent, const scene::TransformComponent>()
        .build()
        .each([&](flecs::entity, const scene::SpotLightComponent& light, const scene::TransformComponent& t) {
            GPUSpotLight gpu{};
            gpu.position = t.position;
            gpu.direction = t.rotation.rotate(core::Vec3(0.0f, -1.0f, 0.0f)).normalized();
            gpu.color = light.color;
            gpu.intensity = light.intensity;
            gpu.range = light.range;
            gpu.cos_inner = std::cos(core::math::deg_to_rad(light.inner_cone_angle_deg));
            gpu.cos_outer = std::cos(core::math::deg_to_rad(light.outer_cone_angle_deg));
            m_lights.spot_lights.push_back(gpu);
        });
}

void SceneRenderer::reset_frame_resources() {
    m_frame_resources = FrameResources{};
}

// ==========================================
// GPU initialization (lazy, first render)
// ==========================================

bool SceneRenderer::ensure_gpu_ready() {
    if (m_gpu_ready) return true;
    if (!m_rhi || !m_rhi->is_initialized()) return false;

    // 1. Command pool + buffer
    if (!m_cmd_pool.init(m_rhi->get_queue_families().graphics_family)) {
        LOG_FATAL("SceneRenderer", "Failed to create command pool");
        return false;
    }
    if (!m_cmd_buffer.init(m_cmd_pool.get_handle())) {
        LOG_FATAL("SceneRenderer", "Failed to allocate frame command buffer");
        return false;
    }

    // 2. Synchronization primitives
    if (!m_frame_fence.init(true)) {
        LOG_FATAL("SceneRenderer", "Failed to create frame fence");
        return false;
    }
    uint32_t slot_count = m_frames_in_flight > 0 ? m_frames_in_flight : 1;
    m_render_finished_semaphores.resize(slot_count);
    for (auto& sem : m_render_finished_semaphores) {
        if (!sem.init(false)) {
            LOG_FATAL("SceneRenderer", "Failed to create render-finished semaphore");
            return false;
        }
    }

    // 3. Shared frame uniforms (UBO + CSM shadow sampler + descriptor set)
    rhi::BufferDesc ubo_desc{};
    ubo_desc.size = sizeof(FrameUniforms);
    ubo_desc.usage = rhi::BufferUsage::Uniform;
    ubo_desc.memory_usage = rhi::MemoryUsage::CpuToGpu;
    ubo_desc.debug_name = "FrameUniforms";
    if (!m_frame_uniforms.ubo.init(ubo_desc)) {
        LOG_FATAL("SceneRenderer", "Failed to create frame uniform buffer");
        return false;
    }

    rhi::SamplerDesc shadow_sampler_desc{};
    shadow_sampler_desc.min_filter = rhi::SamplerFilter::Linear;
    shadow_sampler_desc.mag_filter = rhi::SamplerFilter::Linear;
    shadow_sampler_desc.mipmap_mode = rhi::SamplerFilter::Nearest;
    shadow_sampler_desc.address_u = rhi::SamplerAddressMode::ClampToBorder;
    shadow_sampler_desc.address_v = rhi::SamplerAddressMode::ClampToBorder;
    shadow_sampler_desc.address_w = rhi::SamplerAddressMode::ClampToBorder;
    shadow_sampler_desc.enable_anisotropy = false;
    shadow_sampler_desc.compare_enable = true;
    shadow_sampler_desc.compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
    shadow_sampler_desc.debug_name = "CSM_ShadowSampler";
    if (!m_frame_uniforms.shadow_sampler.init(shadow_sampler_desc)) {
        LOG_FATAL("SceneRenderer", "Failed to create shadow comparison sampler");
        return false;
    }

    std::vector<rhi::DescriptorBinding> bindings = {
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    };
    if (!m_frame_uniforms.set_layout.init(bindings)) {
        LOG_FATAL("SceneRenderer", "Failed to create frame descriptor set layout");
        return false;
    }

    std::vector<std::pair<VkDescriptorType, uint32_t>> pool_sizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    };
    if (!m_frame_uniforms.pool.init(1, pool_sizes) ||
        !m_frame_uniforms.descriptor.init(m_frame_uniforms.pool, m_frame_uniforms.set_layout.get_handle())) {
        LOG_FATAL("SceneRenderer", "Failed to allocate frame descriptor set");
        return false;
    }
    m_frame_uniforms.descriptor.update_uniform_buffer(0, m_frame_uniforms.ubo.get_handle(), sizeof(FrameUniforms));
    m_frame_uniforms.valid = true;

    // 4. Procedural primitives
    m_mesh_resolver.register_primitive("Cube", Mesh::create_cube(core::Vec3(1.0f, 1.0f, 1.0f)));
    m_mesh_resolver.register_primitive("Sphere", Mesh::create_sphere(0.5f, 32, 16));
    m_mesh_resolver.register_primitive("Plane", Mesh::create_plane(1.0f, 1.0f, 1));
    m_mesh_resolver.register_primitive("GroundPlane", Mesh::create_cube(core::Vec3(1.0f, 1.0f, 1.0f)));

    // 5. Default built-in feature set
    register_default_features();

    m_gpu_ready = true;
    LOG_INFO("SceneRenderer", "GPU execution state initialized with {} built-in features", m_features_by_name.size());
    return true;
}

void SceneRenderer::register_default_features() {
    // Stage ordering note: this renderer is forward-shaded, so cascade shadow
    // evaluation must precede opaque rasterization. CascadedShadowFeature is
    // registered in the DepthPrePass stage with negative priority so it executes
    // first. Remaining features map directly to their spec stages.
    register_feature(std::make_shared<CascadedShadowFeature>());
    register_feature(std::make_shared<DepthPrePassFeature>());
    register_feature(std::make_shared<ForwardOpaqueFeature>());
    register_feature(std::make_shared<ClusteredLightingFeature>());
    register_feature(std::make_shared<VolumetricsFeature>());
    register_feature(std::make_shared<OitFeature>());
    register_feature(std::make_shared<AutoExposureFeature>());
    register_feature(std::make_shared<TaaFeature>());
    register_feature(std::make_shared<BloomFeature>());
    register_feature(std::make_shared<PostProcessCompositeFeature>());
}

// ==========================================
// Frame uniform upload
// ==========================================

void SceneRenderer::upload_frame_uniforms(const SceneRenderView& view) {
    FrameUniforms& u = m_frame_uniforms.cpu_data;
    u = FrameUniforms{};

    u.view_proj = view.camera.view_proj;
    u.prev_view_proj = m_prev_view_proj;
    u.view = view.camera.view;
    u.camera_pos = core::Vec4(view.camera.position.x, view.camera.position.y, view.camera.position.z, 0.0f);

    if (m_lights.has_directional) {
        u.dir_light_dir_intensity = core::Vec4(m_lights.direction.x, m_lights.direction.y, m_lights.direction.z, m_lights.intensity);
        u.dir_light_color_ambient = core::Vec4(m_lights.color.x, m_lights.color.y, m_lights.color.z, 0.20f);
    } else {
        u.dir_light_dir_intensity = core::Vec4(0.5f, 1.0f, 0.3f, 1.5f);
        u.dir_light_color_ambient = core::Vec4(1.0f, 0.98f, 0.92f, 0.20f);
    }

    u.cascade_splits = m_lights.cascade_splits;
    if (m_lights.cascades_valid) {
        for (uint32_t i = 0; i < 4; ++i) {
            u.cascade_view_proj[i] = m_lights.cascade_view_proj[i];
        }
    }
    u.shadow_params = core::Vec4(m_settings.shadow_bias, m_settings.enable_shadows ? 1.0f : 0.0f, 0.0f, 0.0f);

    if (m_frame_uniforms.ubo.is_valid()) {
        m_frame_uniforms.ubo.upload_data(&u, sizeof(FrameUniforms));
    }
}

// ==========================================
// Graph compilation (stage-ordered)
// ==========================================

void SceneRenderer::compile_and_execute_graph(RenderGraph& rg, const SceneRenderView& view) {
    // Import final target into the graph so features can attach to it.
    if (view.final_target.is_valid() && view.services) {
        view.services->resources->final_target_rg = rg.import_texture(
            "FinalTarget",
            view.final_target.image,
            view.final_target.image_view,
            view.final_target.width,
            view.final_target.height,
            view.final_target.format,
            view.final_target.initial_layout
        );
    }

    // Compile features stage-by-stage in strict RenderStage order.
    for (uint8_t s = 0; s < RENDER_STAGE_COUNT; ++s) {
        RenderStage stage = static_cast<RenderStage>(s);
        auto it = m_features_by_stage.find(stage);
        if (it == m_features_by_stage.end()) continue;

        auto& features = it->second;
        std::vector<size_t> order(features.size());
        std::iota(order.begin(), order.end(), 0);
        // Stable: registration order preserved within equal priorities.
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
            return features[a]->get_priority() < features[b]->get_priority();
        });

        for (size_t idx : order) {
            auto& feature = features[idx];
            rg.add_pass(
                feature->get_name(),
                [feature, &view](RenderPassBuilder& builder) {
                    feature->setup(builder, view);
                },
                [feature, &view](RenderPassContext& ctx) {
                    feature->execute(ctx, view);
                }
            );
        }
    }

    // Final transition to present layout.
    if (view.final_target.is_valid() && view.services) {
        RGTextureHandle final_rg = view.services->resources->final_target_rg;
        rg.add_pass(
            "PresentTransitionPass",
            [final_rg](RenderPassBuilder& builder) {
                builder.read_texture(final_rg, RGResourceAccess::Present);
            },
            [](RenderPassContext&) {}
        );
    }
}

// ==========================================
// Main render entry point
// ==========================================

void SceneRenderer::render(const scene::Scene& scene, const Camera& camera, rhi::RHIImageHandle output_target) {
    const bool gpu_path = (m_rhi != nullptr) && m_rhi->is_initialized();
    if (gpu_path && !ensure_gpu_ready()) {
        LOG_ERROR("SceneRenderer", "GPU initialization failed; frame skipped");
        return;
    }

    // 1. Frame bookkeeping
    m_stats = SceneRenderStats{};
    reset_frame_resources();
    extract_frame_lights(scene);
    extract_point_and_spot_lights(scene);

    const uint32_t width = output_target.get_width_or(1280);
    const uint32_t height = output_target.get_height_or(720);

    // 2. Working camera with TAA projection jitter (applied before graph build so
    //    every stage renders against the same jittered projection).
    m_working_camera = camera;
    if (m_settings.enable_taa && get_feature("TaaResolve") != nullptr) {
        core::Vec2 jitter = TaaSystem::get_jitter(m_frame_index);
        m_working_camera.proj = TaaSystem::apply_jitter(camera.proj, jitter, width, height);
        m_working_camera.update_view_proj();
    }

    // 3. Build stage-ordered feature graph
    m_render_graph.reset();

    RenderFeatureServices services{};
    services.meshes = &m_mesh_resolver;
    services.stats = &m_stats;
    services.settings = &m_settings;
    services.lights = &m_lights;
    services.resources = &m_frame_resources;
    services.prev_view_proj = m_prev_view_proj;
    services.frame_index = m_frame_index;
    services.adapted_exposure = m_adapted_exposure;
    services.frame_uniforms = m_frame_uniforms.valid ? &m_frame_uniforms : nullptr;

    SceneRenderView view{ scene, m_working_camera, output_target, width, height, m_delta_time, &services };
    compile_and_execute_graph(m_render_graph, view);

    // 4. Upload shared frame uniforms (cascade data finalized during setup).
    upload_frame_uniforms(view);

    if (gpu_path) {
        // 5a. GPU: record + submit
        m_frame_fence.wait();
        m_frame_fence.reset();

        if (!m_render_graph.compile()) {
            LOG_ERROR("SceneRenderer", "RenderGraph compile failed; frame skipped");
            return;
        }

        if (!m_cmd_buffer.begin()) {
            LOG_ERROR("SceneRenderer", "Command buffer begin failed");
            return;
        }
        m_render_graph.execute(m_cmd_buffer);
        if (!m_cmd_buffer.end()) {
            LOG_ERROR("SceneRenderer", "Command buffer end failed");
            return;
        }

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphore = output_target.acquire_semaphore;
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        if (wait_semaphore != VK_NULL_HANDLE) {
            submit_info.waitSemaphoreCount = 1;
            submit_info.pWaitSemaphores = &wait_semaphore;
            submit_info.pWaitDstStageMask = &wait_stage;
        }

        VkCommandBuffer raw_cmd = m_cmd_buffer.get_handle();
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &raw_cmd;

        // Alternate render-finished semaphores per frame slot so a present wait
        // on frame N is never re-signaled before the swapchain retires it.
        const size_t slot = m_frame_index % m_render_finished_semaphores.size();
        VkSemaphore signal_semaphore = m_render_finished_semaphores[slot].get_handle();
        m_render_finished_index = static_cast<uint32_t>(slot);
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = &signal_semaphore;

        if (vkQueueSubmit(m_rhi->get_graphics_queue(), 1, &submit_info, m_frame_fence.get_handle()) != VK_SUCCESS) {
            LOG_ERROR("SceneRenderer", "Frame submission failed");
            return;
        }

        // Wait for GPU completion before post-frame work (readbacks, history swap).
        m_frame_fence.wait();
    } else {
        // 5b. CPU-only path (tests, tools): compile + execute pass callbacks with a
        //     dummy command buffer. Features declaring no transient resources make
        //     this path touch zero Vulkan calls.
        m_render_graph.compile();
        rhi::RhiCommandBuffer dummy_cmd;
        m_render_graph.execute(dummy_cmd);
    }

    // 6. Post-frame callbacks (stage order preserved).
    for (uint8_t s = 0; s < RENDER_STAGE_COUNT; ++s) {
        RenderStage stage = static_cast<RenderStage>(s);
        auto it = m_features_by_stage.find(stage);
        if (it == m_features_by_stage.end()) continue;
        for (auto& feature : it->second) {
            feature->post_frame(view);
        }
    }
    m_adapted_exposure = services.adapted_exposure;

    // 7. Frame advancement
    m_prev_view_proj = m_working_camera.view_proj;
    m_frame_index++;
}

// ==========================================
// Test / tool seam
// ==========================================

void SceneRenderer::build_feature_graph(RenderGraph& rg,
                                        const scene::Scene& scene,
                                        const Camera& camera,
                                        rhi::RHIImageHandle output_target) {
    reset_frame_resources();
    extract_frame_lights(scene);
    extract_point_and_spot_lights(scene);

    const uint32_t width = output_target.get_width_or(1280);
    const uint32_t height = output_target.get_height_or(720);

    RenderFeatureServices services{};
    services.meshes = &m_mesh_resolver;
    services.stats = &m_stats;
    services.settings = &m_settings;
    services.lights = &m_lights;
    services.resources = &m_frame_resources;
    services.prev_view_proj = m_prev_view_proj;
    services.frame_index = m_frame_index;
    services.adapted_exposure = m_adapted_exposure;
    services.frame_uniforms = m_frame_uniforms.valid ? &m_frame_uniforms : nullptr;

    SceneRenderView view{ scene, camera, output_target, width, height, m_delta_time, &services };
    compile_and_execute_graph(rg, view);
}

} // namespace engine::renderer
