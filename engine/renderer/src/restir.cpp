#include "engine/renderer/restir.h"
#include "engine/core/log.h"

namespace engine::renderer {

ReSTIRSystem::~ReSTIRSystem() {
    destroy();
}

bool ReSTIRSystem::init(uint32_t width, uint32_t height) {
    destroy();
    m_width = width;
    m_height = height;

    uint32_t pixel_count = width * height;

    // 1. Normal + Roughness Guide (RGBA16F)
    rhi::TextureDesc norm_desc{
        .width = width,
        .height = height,
        .format = rhi::Format::R16G16B16A16_SFLOAT,
        .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage,
        .debug_name = "NRD_NormalRoughnessGuide"
    };
    if (!m_normal_roughness_guide.init(norm_desc)) return false;

    // 2. Motion Vectors Guide (RG16F)
    rhi::TextureDesc mv_desc{
        .width = width,
        .height = height,
        .format = rhi::Format::R16G16_SFLOAT,
        .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage,
        .debug_name = "NRD_MotionVectorsGuide"
    };
    if (!m_motion_vectors_guide.init(mv_desc)) return false;

    // 3. Radiance Output (RGBA16F)
    rhi::TextureDesc rad_desc{
        .width = width,
        .height = height,
        .format = rhi::Format::R16G16B16A16_SFLOAT,
        .usage = rhi::TextureUsage::ColorAttachment | rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage,
        .debug_name = "ReSTIR_RadianceOutput"
    };
    if (!m_radiance_output.init(rad_desc)) return false;

    // 4. Temporal & Spatial Reservoir Buffers
    rhi::BufferDesc res_desc{
        .size = pixel_count * sizeof(Reservoir),
        .usage = rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::GpuOnly,
        .debug_name = "ReSTIR_TemporalReservoirs"
    };
    m_temporal_reservoir_buffer.init(res_desc);

    res_desc.debug_name = "ReSTIR_SpatialReservoirs";
    m_spatial_reservoir_buffer.init(res_desc);

    LOG_INFO("ReSTIR", "Initialized ReSTIR & Denoising Interface ({}x{}, {} pixels)", width, height, pixel_count);
    return true;
}

void ReSTIRSystem::resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;
    init(width, height);
}

void ReSTIRSystem::destroy() {
    m_normal_roughness_guide.destroy();
    m_motion_vectors_guide.destroy();
    m_radiance_output.destroy();
    m_temporal_reservoir_buffer.destroy();
    m_spatial_reservoir_buffer.destroy();
}

} // namespace engine::renderer
