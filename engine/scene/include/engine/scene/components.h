#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
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

} // namespace engine::scene
