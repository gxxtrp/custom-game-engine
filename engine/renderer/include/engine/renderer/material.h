#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/assets/uuid.h"
#include "engine/rhi/rhi_buffer.h"
#include "engine/importer/importer.h"
#include <string>
#include <memory>

namespace engine::renderer {

enum class BlendMode : uint8_t {
    Opaque = 0,
    Masked,
    Transparent // OIT path
};

struct alignas(16) MaterialUniformData {
    core::Vec4 base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
    core::Vec3 emissive_factor{0.0f, 0.0f, 0.0f};
    float metallic_factor{1.0f};

    float roughness_factor{1.0f};
    float emissive_strength{1.0f};
    float transmission_factor{0.0f};
    float ior{1.5f};

    float clearcoat_factor{0.0f};
    float sheen_factor{0.0f};
    uint32_t albedo_tex_idx{UINT32_MAX};
    uint32_t normal_tex_idx{UINT32_MAX};

    uint32_t metallic_roughness_tex_idx{UINT32_MAX};
    uint32_t emissive_tex_idx{UINT32_MAX};
    uint32_t padding[2]{0, 0};
};

class MaterialInstance {
public:
    MaterialInstance() = default;
    ~MaterialInstance();

    bool init(std::string_view name = "MaterialInstance");
    void destroy();

    void update_gpu_buffer();

    void set_base_color(const core::Vec4& color) { m_data.base_color_factor = color; m_is_dirty = true; }
    void set_metallic(float metallic) { m_data.metallic_factor = metallic; m_is_dirty = true; }
    void set_roughness(float roughness) { m_data.roughness_factor = roughness; m_is_dirty = true; }
    void set_emissive(const core::Vec3& emissive, float strength = 1.0f) {
        m_data.emissive_factor = emissive;
        m_data.emissive_strength = strength;
        m_is_dirty = true;
    }
    void set_transmission(float transmission) { m_data.transmission_factor = transmission; m_is_dirty = true; }
    void set_ior(float ior) { m_data.ior = ior; m_is_dirty = true; }

    void set_albedo_texture_index(uint32_t idx) { m_data.albedo_tex_idx = idx; m_is_dirty = true; }
    void set_normal_texture_index(uint32_t idx) { m_data.normal_tex_idx = idx; m_is_dirty = true; }
    void set_metallic_roughness_texture_index(uint32_t idx) { m_data.metallic_roughness_tex_idx = idx; m_is_dirty = true; }
    void set_emissive_texture_index(uint32_t idx) { m_data.emissive_tex_idx = idx; m_is_dirty = true; }

    const MaterialUniformData& get_data() const { return m_data; }
    const rhi::RhiBuffer& get_uniform_buffer() const { return m_uniform_buffer; }
    BlendMode get_blend_mode() const { return m_blend_mode; }
    void set_blend_mode(BlendMode mode) { m_blend_mode = mode; }

    static std::shared_ptr<MaterialInstance> from_imported_material(const importer::ImportedMaterial& imported_mat);

    assets::UUID uuid{assets::UUID::generate()};
    std::string name{"MaterialInstance"};

private:
    MaterialUniformData m_data{};
    rhi::RhiBuffer m_uniform_buffer;
    BlendMode m_blend_mode{BlendMode::Opaque};
    bool m_is_dirty{true};
};

} // namespace engine::renderer
