#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/core/reflection.h"
#include "engine/assets/uuid.h"
#include <string>
#include <vector>

namespace engine::scene {

struct TagComponent {
    std::string name{"Entity"};
};

struct UUIDComponent {
    assets::UUID uuid{assets::UUID::generate()};
};

struct TransformComponent {
    core::Vec3 position{0.0f, 0.0f, 0.0f};
    core::Quat rotation{0.0f, 0.0f, 0.0f, 1.0f};
    core::Vec3 scale{1.0f, 1.0f, 1.0f};
    bool is_dirty{true};

    core::Mat4 get_local_matrix() const {
        return core::Mat4::translation(position) * rotation.to_mat4() * core::Mat4::scaling(scale);
    }
};

struct WorldTransformComponent {
    core::Mat4 matrix{core::Mat4::identity()};

    core::Vec3 get_world_position() const {
        return core::Vec3(matrix.cols[3].x, matrix.cols[3].y, matrix.cols[3].z);
    }
};

struct MeshRendererComponent {
    assets::UUID mesh_uuid;
    uint32_t submesh_index{0};
    assets::UUID material_uuid;
    bool cast_shadows{true};
    bool receive_shadows{true};
    bool is_visible{true};
};

struct DirectionalLightComponent {
    core::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
    bool cast_shadows{true};
    uint32_t cascade_count{4};
};

struct PointLightComponent {
    core::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
    float radius{10.0f};
    float falloff{1.0f};
    bool cast_shadows{false};
};

struct SpotLightComponent {
    core::Vec3 color{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
    float range{15.0f};
    float inner_cone_angle_deg{25.0f};
    float outer_cone_angle_deg{35.0f};
    bool cast_shadows{false};
};

struct CameraComponent {
    float fov_deg{60.0f};
    float near_z{0.1f};
    float far_z{1000.0f};
    bool is_orthographic{false};
    float ortho_size{10.0f};
    bool is_primary{true};

    core::Mat4 get_projection_matrix(float aspect_ratio) const {
        if (is_orthographic) {
            float half_w = ortho_size * aspect_ratio * 0.5f;
            float half_h = ortho_size * 0.5f;
            return core::Mat4::orthographic(-half_w, half_w, -half_h, half_h, near_z, far_z);
        } else {
            return core::Mat4::perspective_vk(core::math::deg_to_rad(fov_deg), aspect_ratio, near_z, far_z);
        }
    }
};

struct MaterialComponent {
    core::Vec4 base_color{0.8f, 0.8f, 0.8f, 1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    core::Vec3 emissive{0.0f, 0.0f, 0.0f};
    float emissive_strength{1.0f};
    assets::UUID albedo_texture_uuid;
    assets::UUID normal_texture_uuid;
    assets::UUID metallic_roughness_texture_uuid;
    // 0 = Opaque, 1 = Masked, 2 = Transparent (OIT path)
    uint32_t blend_mode{0};
};

} // namespace engine::scene

// ==========================================
// Component Reflection Registrations
// ==========================================

REFLECT_STRUCT_BEGIN(engine::scene::TagComponent)
    REFLECT_FIELD(name, "Name", "Entity tag name")
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(engine::scene::UUIDComponent)
    REFLECT_FIELD(uuid, "UUID", "Unique 128-bit entity identifier")
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(engine::scene::TransformComponent)
    REFLECT_FIELD(position, "Position", "Local translation")
    REFLECT_FIELD(rotation, "Rotation", "Local orientation quaternion")
    REFLECT_FIELD(scale, "Scale", "Local 3D scaling vector")
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(engine::scene::MeshRendererComponent)
    REFLECT_FIELD(mesh_uuid, "Mesh UUID", "Referenced glTF mesh asset")
    REFLECT_FIELD(submesh_index, "Submesh Index", "Submesh primitive slot index")
    REFLECT_FIELD(material_uuid, "Material UUID", "Material definition asset UUID")
    REFLECT_FIELD(cast_shadows, "Cast Shadows", "Casts shadows into cascade shadow maps")
    REFLECT_FIELD(receive_shadows, "Receive Shadows", "Receives dynamic cascaded shadows")
    REFLECT_FIELD(is_visible, "Is Visible", "Toggles geometry rendering")
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(engine::scene::DirectionalLightComponent)
    REFLECT_FIELD(color, "Color", "Sun / directional light RGB color")
    REFLECT_FIELD(intensity, "Intensity", "Illuminance in Lux")
    REFLECT_FIELD(cast_shadows, "Cast Shadows", "Enables 4-split cascaded shadow mapping")
    REFLECT_FIELD(cascade_count, "Cascade Count", "Number of active shadow cascades")
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(engine::scene::PointLightComponent)
    REFLECT_FIELD(color, "Color", "Omni-directional light RGB color")
    REFLECT_FIELD(intensity, "Intensity", "Luminous intensity in Candela")
    REFLECT_FIELD(radius, "Radius", "Attenuation influence radius")
    REFLECT_FIELD(falloff, "Falloff", "Distance attenuation exponent")
    REFLECT_FIELD(cast_shadows, "Cast Shadows", "Enables point light cubemap shadows")
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(engine::scene::SpotLightComponent)
    REFLECT_FIELD(color, "Color", "Conical spot light RGB color")
    REFLECT_FIELD(intensity, "Intensity", "Luminous intensity in Candela")
    REFLECT_FIELD(range, "Range", "Maximum projection distance")
    REFLECT_FIELD(inner_cone_angle_deg, "Inner Cone Angle", "Inner full-intensity cone angle in degrees")
    REFLECT_FIELD(outer_cone_angle_deg, "Outer Cone Angle", "Outer cutoff cone angle in degrees")
    REFLECT_FIELD(cast_shadows, "Cast Shadows", "Enables spot light perspective shadows")
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(engine::scene::CameraComponent)
    REFLECT_FIELD(fov_deg, "Field of View", "Vertical field of view in degrees")
    REFLECT_FIELD(near_z, "Near Plane", "Near clipping distance")
    REFLECT_FIELD(far_z, "Far Plane", "Far clipping distance")
    REFLECT_FIELD(is_orthographic, "Is Orthographic", "Toggles orthographic projection mode")
    REFLECT_FIELD(ortho_size, "Ortho Size", "Orthographic vertical view height")
    REFLECT_FIELD(is_primary, "Is Primary", "Designates main viewport render camera")
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(engine::scene::MaterialComponent)
    REFLECT_FIELD(base_color, "Base Color", "PBR albedo RGBA tint")
    REFLECT_FIELD(metallic, "Metallic", "Metallic factor [0.0 - 1.0]")
    REFLECT_FIELD(roughness, "Roughness", "Roughness factor [0.0 - 1.0]")
    REFLECT_FIELD(emissive, "Emissive", "Emissive RGB radiance vector")
    REFLECT_FIELD(emissive_strength, "Emissive Strength", "Emissive multiplier")
    REFLECT_FIELD(albedo_texture_uuid, "Albedo Texture", "Albedo texture UUID")
    REFLECT_FIELD(normal_texture_uuid, "Normal Map", "Normal texture UUID")
    REFLECT_FIELD(metallic_roughness_texture_uuid, "Metallic Roughness Map", "Metallic/Roughness texture UUID")
    REFLECT_FIELD(blend_mode, "Blend Mode", "0=Opaque, 1=Masked, 2=Transparent")
REFLECT_STRUCT_END()
