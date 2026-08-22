#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/scene/components.h"

namespace engine::renderer {

// View camera state consumed by render features. Built by the host from the
// active scene camera each frame (or directly by tools/plugins).
class Camera {
public:
    core::Mat4 view{core::Mat4::identity()};
    core::Mat4 proj{core::Mat4::identity()};
    core::Mat4 view_proj{core::Mat4::identity()};
    core::Vec3 position{0.0f, 0.0f, 0.0f};
    core::Frustum frustum{};
    float fov_rad{core::math::deg_to_rad(60.0f)};
    float aspect{16.0f / 9.0f};
    float near_z{0.1f};
    float far_z{1000.0f};

    void update_view_proj() {
        view_proj = proj * view;
    }

    // Builds a Camera from ECS camera components (primary camera extraction path).
    static Camera from_components(const scene::CameraComponent& cam,
                                  const scene::WorldTransformComponent& world,
                                  float aspect_ratio);
};

} // namespace engine::renderer
