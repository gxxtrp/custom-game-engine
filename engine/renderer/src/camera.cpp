#include "engine/renderer/camera.h"

namespace engine::renderer {

Camera Camera::from_components(const scene::CameraComponent& cam,
                               const scene::WorldTransformComponent& world,
                               float aspect_ratio) {
    Camera out{};
    out.aspect = aspect_ratio;
    out.fov_rad = core::math::deg_to_rad(cam.fov_deg);
    out.near_z = cam.near_z;
    out.far_z = cam.far_z;
    out.proj = cam.get_projection_matrix(aspect_ratio);
    out.position = world.get_world_position();

    core::Vec3 fwd = world.matrix.transform_vector(core::Vec3(0.0f, 0.0f, 1.0f)).normalized();
    core::Vec3 up = world.matrix.transform_vector(core::Vec3(0.0f, 1.0f, 0.0f)).normalized();
    out.view = core::Mat4::look_at(out.position, out.position + fwd, up);
    out.view_proj = out.proj * out.view;
    out.frustum = core::Frustum::from_view_projection(out.view_proj);
    return out;
}

} // namespace engine::renderer
