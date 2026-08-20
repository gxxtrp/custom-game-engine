#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"

namespace engine::ui {

class EditorCamera {
public:
    EditorCamera(float fov = 45.0f, float near_clip = 0.1f, float far_clip = 1000.0f);

    void on_update(float dt, const core::Vec2& mouse_delta, bool is_orbiting, bool is_panning, float scroll);

    core::Mat4 get_view_matrix() const;
    core::Mat4 get_projection_matrix(float aspect) const;

    core::Ray screen_pos_to_world_ray(const core::Vec2& screen_pos, const core::Vec2& viewport_size) const;

    const core::Vec3& get_position() const { return m_position; }
    void set_position(const core::Vec3& pos) { m_position = pos; update_view(); }

    const core::Vec3& get_focal_point() const { return m_focal_point; }
    void set_focal_point(const core::Vec3& fp) { m_focal_point = fp; update_view(); }

    float get_distance() const { return m_distance; }
    void set_distance(float dist) { m_distance = dist; update_view(); }

    core::Vec3 get_forward() const {
        return (m_focal_point - m_position).normalized();
    }
    core::Vec3 get_right() const {
        return core::Vec3(m_view_matrix.cols[0].x, m_view_matrix.cols[1].x, m_view_matrix.cols[2].x);
    }
    core::Vec3 get_up() const {
        return core::Vec3(m_view_matrix.cols[0].y, m_view_matrix.cols[1].y, m_view_matrix.cols[2].y);
    }

private:
    void update_view();
    void mouse_pan(const core::Vec2& delta);
    void mouse_rotate(const core::Vec2& delta);
    void mouse_zoom(float delta);

    float m_fov{45.0f};
    float m_near_clip{0.1f};
    float m_far_clip{1000.0f};

    core::Vec3 m_position{0.0f, 5.0f, -12.0f};
    core::Vec3 m_focal_point{0.0f, 0.0f, 0.0f};
    float m_distance{12.0f};
    float m_pitch{0.35f};
    float m_yaw{0.0f};

    core::Mat4 m_view_matrix{core::Mat4::identity()};
};

} // namespace engine::ui
