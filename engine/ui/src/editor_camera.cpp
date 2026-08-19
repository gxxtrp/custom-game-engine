#include "engine/ui/editor_camera.h"
#include <cmath>
#include <algorithm>

namespace engine::ui {

EditorCamera::EditorCamera(float fov, float near_clip, float far_clip)
    : m_fov(fov), m_near_clip(near_clip), m_far_clip(far_clip) {
    update_view();
}

void EditorCamera::on_update(float /*dt*/, const core::Vec2& mouse_delta, bool is_orbiting, bool is_panning, float scroll) {
    if (is_panning) {
        mouse_pan(mouse_delta);
    } else if (is_orbiting) {
        mouse_rotate(mouse_delta);
    }

    if (std::abs(scroll) > 0.0f) {
        mouse_zoom(scroll);
    }

    update_view();
}

void EditorCamera::update_view() {
    float cos_pitch = std::cos(m_pitch);
    float sin_pitch = std::sin(m_pitch);
    float cos_yaw = std::cos(m_yaw);
    float sin_yaw = std::sin(m_yaw);

    core::Vec3 offset(
        m_distance * sin_yaw * cos_pitch,
        m_distance * sin_pitch,
        m_distance * cos_yaw * cos_pitch
    );

    m_position = m_focal_point - offset;
    m_view_matrix = core::Mat4::look_at(m_position, m_focal_point, core::Vec3(0.0f, 1.0f, 0.0f));
}

void EditorCamera::mouse_pan(const core::Vec2& delta) {
    float speed = m_distance * 0.003f;
    core::Vec3 right(m_view_matrix.cols[0].x, m_view_matrix.cols[1].x, m_view_matrix.cols[2].x);
    core::Vec3 up(m_view_matrix.cols[0].y, m_view_matrix.cols[1].y, m_view_matrix.cols[2].y);

    m_focal_point -= right * (delta.x * speed);
    m_focal_point += up * (delta.y * speed);
}

void EditorCamera::mouse_rotate(const core::Vec2& delta) {
    m_yaw -= delta.x * 0.005f;
    m_pitch -= delta.y * 0.005f;
    m_pitch = std::clamp(m_pitch, -1.55f, 1.55f);
}

void EditorCamera::mouse_zoom(float delta) {
    m_distance -= delta * (m_distance * 0.1f);
    m_distance = std::max(0.1f, m_distance);
}

core::Mat4 EditorCamera::get_view_matrix() const {
    return m_view_matrix;
}

core::Mat4 EditorCamera::get_projection_matrix(float aspect) const {
    return core::Mat4::perspective_vk(m_fov * (3.14159265359f / 180.0f), aspect, m_near_clip, m_far_clip);
}

core::Ray EditorCamera::screen_pos_to_world_ray(const core::Vec2& screen_pos, const core::Vec2& viewport_size) const {
    if (viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
        return core::Ray(m_position, core::Vec3(0.0f, 0.0f, 1.0f));
    }

    float ndc_x = (2.0f * screen_pos.x / viewport_size.x) - 1.0f;
    float ndc_y = (2.0f * screen_pos.y / viewport_size.y) - 1.0f;

    core::Mat4 inv_vp = (get_projection_matrix(viewport_size.x / viewport_size.y) * m_view_matrix).inverted();

    core::Vec4 near_point_ndc(ndc_x, ndc_y, 0.0f, 1.0f);
    core::Vec4 far_point_ndc(ndc_x, ndc_y, 1.0f, 1.0f);

    core::Vec4 near_world = inv_vp * near_point_ndc;
    core::Vec4 far_world = inv_vp * far_point_ndc;

    if (std::abs(near_world.w) > 1e-6f) near_world = near_world * (1.0f / near_world.w);
    if (std::abs(far_world.w) > 1e-6f) far_world = far_world * (1.0f / far_world.w);

    core::Vec3 ray_origin(near_world.x, near_world.y, near_world.z);
    core::Vec3 ray_target(far_world.x, far_world.y, far_world.z);
    core::Vec3 ray_dir = (ray_target - ray_origin).normalized();

    return core::Ray(ray_origin, ray_dir);
}

} // namespace engine::ui
