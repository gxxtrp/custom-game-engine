#include "engine/renderer/material.h"
#include "engine/core/log.h"

namespace engine::renderer {

MaterialInstance::~MaterialInstance() {
    destroy();
}

bool MaterialInstance::init(std::string_view mat_name) {
    name = std::string(mat_name);

    rhi::BufferDesc ubo_desc{
        .size = sizeof(MaterialUniformData),
        .usage = rhi::BufferUsage::Uniform | rhi::BufferUsage::Storage | rhi::BufferUsage::TransferDst | rhi::BufferUsage::ShaderDeviceAddress,
        .memory_usage = rhi::MemoryUsage::CpuToGpu,
        .debug_name = name + "_UBO"
    };

    if (!m_uniform_buffer.init(ubo_desc)) {
        LOG_ERROR("Material", "Failed to create uniform buffer for '{}'", name);
        return false;
    }

    update_gpu_buffer();
    return true;
}

void MaterialInstance::destroy() {
    m_uniform_buffer.destroy();
}

void MaterialInstance::update_gpu_buffer() {
    if (!m_uniform_buffer.is_valid()) {
        init(name);
    }
    if (m_is_dirty && m_uniform_buffer.is_valid()) {
        m_uniform_buffer.upload_data(&m_data, sizeof(MaterialUniformData));
        m_is_dirty = false;
    }
}

std::shared_ptr<MaterialInstance> MaterialInstance::from_imported_material(const importer::ImportedMaterial& imported_mat) {
    auto mat = std::make_shared<MaterialInstance>();
    mat->init(imported_mat.name);
    mat->set_base_color(imported_mat.base_color_factor);
    mat->set_metallic(imported_mat.metallic_factor);
    mat->set_roughness(imported_mat.roughness_factor);
    mat->set_emissive(imported_mat.emissive_factor, imported_mat.emissive_strength);
    mat->set_transmission(imported_mat.transmission_factor);
    mat->set_ior(imported_mat.ior);

    if (imported_mat.transmission_factor > 0.0f) {
        mat->set_blend_mode(BlendMode::Transparent);
    }

    mat->update_gpu_buffer();
    return mat;
}

} // namespace engine::renderer
