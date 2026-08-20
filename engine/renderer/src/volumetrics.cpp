#include "engine/renderer/volumetrics.h"
#include "engine/core/log.h"
#include <cmath>
#include <numbers>

namespace engine::renderer {

VolumetricFog::~VolumetricFog() {
    destroy();
}

bool VolumetricFog::init(uint32_t dim_x, uint32_t dim_y, uint32_t dim_z) {
    destroy();
    m_uniforms.grid_dim_x = dim_x;
    m_uniforms.grid_dim_y = dim_y;
    m_uniforms.grid_dim_z = dim_z;

    // 1. 3D Texture for Volumetric Fog Grid
    rhi::TextureDesc tex_desc{
        .width = dim_x,
        .height = dim_y,
        .depth = dim_z,
        .mip_levels = 1,
        .array_layers = 1,
        .format = rhi::Format::R16G16B16A16_SFLOAT,
        .dimension = rhi::TextureDimension::Texture3D,
        .usage = rhi::TextureUsage::Storage | rhi::TextureUsage::Sampled,
        .debug_name = "VolumetricFog_3DGrid"
    };

    if (!m_volumetric_texture.init(tex_desc)) {
        LOG_ERROR("Volumetrics", "Failed to create 3D volumetric fog texture ({}x{}x{})", dim_x, dim_y, dim_z);
        return false;
    }

    // 2. Uniform Buffer
    rhi::BufferDesc ubo_desc{
        .size = sizeof(VolumetricFogUniforms),
        .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::CpuToGpu,
        .debug_name = "VolumetricFog_UBO"
    };

    if (!m_uniform_buffer.init(ubo_desc)) {
        LOG_ERROR("Volumetrics", "Failed to create volumetric fog uniform buffer");
        return false;
    }

    update_gpu_buffer();
    LOG_INFO("Volumetrics", "Initialized Volumetric Fog System ({}x{}x{} 3D Froxel Grid)", dim_x, dim_y, dim_z);
    return true;
}

void VolumetricFog::destroy() {
    m_volumetric_texture.destroy();
    m_uniform_buffer.destroy();
}

void VolumetricFog::update_gpu_buffer() {
    if (m_dirty && m_uniform_buffer.is_valid()) {
        m_uniform_buffer.upload_data(&m_uniforms, sizeof(VolumetricFogUniforms));
        m_dirty = false;
    }
}

float VolumetricFog::henyey_greenstein_phase(float cos_theta, float g) {
    float g2 = g * g;
    float denom = 1.0f + g2 - 2.0f * g * cos_theta;
    return (1.0f - g2) / (4.0f * std::numbers::pi_v<float> * std::pow(std::max(denom, 1e-4f), 1.5f));
}

float VolumetricFog::rayleigh_phase(float cos_theta) {
    return (3.0f / (16.0f * std::numbers::pi_v<float>)) * (1.0f + cos_theta * cos_theta);
}

} // namespace engine::renderer
