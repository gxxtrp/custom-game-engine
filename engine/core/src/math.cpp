#include "engine/core/math.h"

namespace engine::core::math {

const Vec3 Vec3::ZERO    = {0.0f, 0.0f, 0.0f};
const Vec3 Vec3::ONE     = {1.0f, 1.0f, 1.0f};
const Vec3 Vec3::UP      = {0.0f, 1.0f, 0.0f};
const Vec3 Vec3::DOWN    = {0.0f, -1.0f, 0.0f};
const Vec3 Vec3::LEFT    = {-1.0f, 0.0f, 0.0f};
const Vec3 Vec3::RIGHT   = {1.0f, 0.0f, 0.0f};
const Vec3 Vec3::FORWARD = {0.0f, 0.0f, -1.0f};
const Vec3 Vec3::BACK    = {0.0f, 0.0f, 1.0f};

Mat4 Mat4::transposed() const {
    Mat4 res = *this;
    _MM_TRANSPOSE4_PS(res.cols[0].simd, res.cols[1].simd, res.cols[2].simd, res.cols[3].simd);
    return res;
}

Mat4 Mat4::inverted() const {
    // Standard 4x4 matrix inversion using Kramer's rule
    float m[16];
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            m[col * 4 + row] = cols[col][row];
        }
    }

    float inv[16];

    inv[0] = m[5]  * m[10] * m[15] - 
             m[5]  * m[11] * m[14] - 
             m[9]  * m[6]  * m[15] + 
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] - 
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] + 
              m[4]  * m[11] * m[14] + 
              m[8]  * m[6]  * m[15] - 
              m[8]  * m[7]  * m[14] - 
              m[12] * m[6]  * m[11] + 
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] - 
             m[4]  * m[11] * m[13] - 
             m[8]  * m[5] * m[15] + 
             m[8]  * m[7] * m[13] + 
             m[12] * m[5] * m[11] - 
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] + 
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] - 
               m[8]  * m[6] * m[13] - 
               m[12] * m[5] * m[10] + 
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] + 
              m[1]  * m[11] * m[14] + 
              m[9]  * m[2] * m[15] - 
              m[9]  * m[3] * m[14] - 
              m[13] * m[2] * m[11] + 
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] - 
             m[0]  * m[11] * m[14] - 
             m[8]  * m[2] * m[15] + 
             m[8]  * m[3] * m[14] + 
             m[12] * m[2] * m[11] - 
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] + 
              m[0]  * m[11] * m[13] + 
              m[8]  * m[1] * m[15] - 
              m[8]  * m[3] * m[13] - 
              m[12] * m[1] * m[11] + 
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] - 
              m[0]  * m[10] * m[13] - 
              m[8]  * m[1] * m[14] + 
              m[8]  * m[2] * m[13] + 
              m[12] * m[1] * m[10] - 
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] - 
             m[1]  * m[7] * m[14] - 
             m[5]  * m[2] * m[15] + 
             m[5]  * m[3] * m[14] + 
             m[13] * m[2] * m[7] - 
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] + 
              m[0]  * m[7] * m[14] + 
              m[4]  * m[2] * m[15] - 
              m[4]  * m[3] * m[14] - 
              m[12] * m[2] * m[7] + 
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] - 
              m[0]  * m[7] * m[13] - 
              m[4]  * m[1] * m[15] + 
              m[4]  * m[3] * m[13] + 
              m[12] * m[1] * m[7] - 
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] + 
               m[0]  * m[6] * m[13] + 
               m[4]  * m[1] * m[14] - 
               m[4]  * m[2] * m[13] - 
               m[12] * m[1] * m[6] + 
               m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + 
              m[1] * m[7] * m[10] + 
              m[5] * m[2] * m[11] - 
              m[5] * m[3] * m[10] - 
              m[9] * m[2] * m[7] + 
              m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - 
             m[0] * m[7] * m[10] - 
             m[4] * m[2] * m[11] + 
             m[4] * m[3] * m[10] + 
             m[8] * m[2] * m[7] - 
             m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + 
               m[0] * m[7] * m[9] + 
               m[4] * m[1] * m[11] - 
               m[4] * m[3] * m[9] - 
               m[8] * m[1] * m[7] + 
               m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - 
              m[0] * m[6] * m[9] - 
              m[4] * m[1] * m[10] + 
              m[4] * m[2] * m[9] + 
              m[8] * m[1] * m[6] - 
              m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::abs(det) < EPSILON) return identity();

    float inv_det = 1.0f / det;
    Mat4 res;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            res.cols[col][row] = inv[col * 4 + row] * inv_det;
        }
    }
    return res;
}

Mat4 Mat4::translation(const Vec3& pos) {
    Mat4 m;
    m.cols[3] = Vec4(pos, 1.0f);
    return m;
}

Mat4 Mat4::scaling(const Vec3& scale) {
    Mat4 m;
    m.cols[0].x = scale.x;
    m.cols[1].y = scale.y;
    m.cols[2].z = scale.z;
    return m;
}

Mat4 Mat4::rotation_x(float rad) {
    float c = std::cos(rad);
    float s = std::sin(rad);
    Mat4 m;
    m.cols[1] = Vec4(0, c, s, 0);
    m.cols[2] = Vec4(0, -s, c, 0);
    return m;
}

Mat4 Mat4::rotation_y(float rad) {
    float c = std::cos(rad);
    float s = std::sin(rad);
    Mat4 m;
    m.cols[0] = Vec4(c, 0, -s, 0);
    m.cols[2] = Vec4(s, 0, c, 0);
    return m;
}

Mat4 Mat4::rotation_z(float rad) {
    float c = std::cos(rad);
    float s = std::sin(rad);
    Mat4 m;
    m.cols[0] = Vec4(c, s, 0, 0);
    m.cols[1] = Vec4(-s, c, 0, 0);
    return m;
}

Mat4 Mat4::rotation_axis(const Vec3& axis, float rad) {
    Vec3 a = axis.normalized();
    float c = std::cos(rad);
    float s = std::sin(rad);
    float t = 1.0f - c;

    Mat4 m;
    m.cols[0] = Vec4(t * a.x * a.x + c,       t * a.x * a.y + s * a.z, t * a.x * a.z - s * a.y, 0.0f);
    m.cols[1] = Vec4(t * a.x * a.y - s * a.z, t * a.y * a.y + c,       t * a.y * a.z + s * a.x, 0.0f);
    m.cols[2] = Vec4(t * a.x * a.z + s * a.y, t * a.y * a.z - s * a.x, t * a.z * a.z + c,       0.0f);
    return m;
}

// Vulkan Perspective (Depth [0, 1], Right-handed, Y flipped for Vulkan viewport coordinate system)
Mat4 Mat4::perspective_vk(float fov_rad, float aspect, float near_z, float far_z) {
    float tan_half_fov = std::tan(fov_rad * 0.5f);
    Mat4 m;
    m.cols[0] = Vec4(1.0f / (aspect * tan_half_fov), 0.0f, 0.0f, 0.0f);
    m.cols[1] = Vec4(0.0f, -1.0f / tan_half_fov, 0.0f, 0.0f); // Invert Y for Vulkan
    m.cols[2] = Vec4(0.0f, 0.0f, far_z / (near_z - far_z), -1.0f);
    m.cols[3] = Vec4(0.0f, 0.0f, (near_z * far_z) / (near_z - far_z), 0.0f);
    return m;
}

Mat4 Mat4::look_at(const Vec3& eye, const Vec3& target, const Vec3& up) {
    Vec3 f = (target - eye).normalized();
    Vec3 s = f.cross(up).normalized();
    Vec3 u = s.cross(f);

    Mat4 m;
    m.cols[0] = Vec4(s.x, u.x, -f.x, 0.0f);
    m.cols[1] = Vec4(s.y, u.y, -f.y, 0.0f);
    m.cols[2] = Vec4(s.z, u.z, -f.z, 0.0f);
    m.cols[3] = Vec4(-s.dot(eye), -u.dot(eye), f.dot(eye), 1.0f);
    return m;
}

Mat4 Mat4::orthographic(float left, float right, float bottom, float top, float near_z, float far_z) {
    Mat4 m;
    m.cols[0] = Vec4(2.0f / (right - left), 0.0f, 0.0f, 0.0f);
    m.cols[1] = Vec4(0.0f, 2.0f / (top - bottom), 0.0f, 0.0f);
    m.cols[2] = Vec4(0.0f, 0.0f, -1.0f / (far_z - near_z), 0.0f);
    m.cols[3] = Vec4(-(right + left) / (right - left), -(top + bottom) / (top - bottom), -near_z / (far_z - near_z), 1.0f);
    return m;
}

// Quaternion implementations
Quat Quat::from_axis_angle(const Vec3& axis, float rad) {
    float half = rad * 0.5f;
    float s = std::sin(half);
    Vec3 a = axis.normalized();
    return Quat(a.x * s, a.y * s, a.z * s, std::cos(half));
}

Quat Quat::from_euler(float pitch_rad, float yaw_rad, float roll_rad) {
    float cp = std::cos(pitch_rad * 0.5f);
    float sp = std::sin(pitch_rad * 0.5f);
    float cy = std::cos(yaw_rad * 0.5f);
    float sy = std::sin(yaw_rad * 0.5f);
    float cr = std::cos(roll_rad * 0.5f);
    float sr = std::sin(roll_rad * 0.5f);

    return Quat(
        sp * cy * cr - cp * sy * sr,
        cp * sy * cr + sp * cy * sr,
        cp * cy * sr - sp * sy * cr,
        cp * cy * cr + sp * sy * sr
    );
}

Vec3 Quat::rotate(const Vec3& v) const {
    Vec3 qv(x, y, z);
    Vec3 uv = qv.cross(v);
    Vec3 uuv = qv.cross(uv);
    return v + (uv * w + uuv) * 2.0f;
}

Mat4 Quat::to_mat4() const {
    Mat4 m;
    float xx = x * x, yy = y * y, zz = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;

    m.cols[0] = Vec4(1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy), 0.0f);
    m.cols[1] = Vec4(2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx), 0.0f);
    m.cols[2] = Vec4(2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy), 0.0f);
    m.cols[3] = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    return m;
}

Quat Quat::slerp(const Quat& a, const Quat& b, float t) {
    float cos_half_theta = a.dot(b);
    Quat end = b;
    if (cos_half_theta < 0.0f) {
        end = Quat(-b.x, -b.y, -b.z, -b.w);
        cos_half_theta = -cos_half_theta;
    }

    if (cos_half_theta >= 0.999f) {
        return Quat(
            lerp(a.x, end.x, t),
            lerp(a.y, end.y, t),
            lerp(a.z, end.z, t),
            lerp(a.w, end.w, t)
        ).normalized();
    }

    float half_theta = std::acos(cos_half_theta);
    float sin_half_theta = std::sqrt(1.0f - cos_half_theta * cos_half_theta);

    float ratio_a = std::sin((1.0f - t) * half_theta) / sin_half_theta;
    float ratio_b = std::sin(t * half_theta) / sin_half_theta;

    return Quat(
        a.x * ratio_a + end.x * ratio_b,
        a.y * ratio_a + end.y * ratio_b,
        a.z * ratio_a + end.z * ratio_b,
        a.w * ratio_a + end.w * ratio_b
    );
}

// AABB
AABB AABB::transformed(const Mat4& transform) const {
    Vec3 corners[8] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {min.x, max.y, min.z},
        {max.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {min.x, max.y, max.z},
        {max.x, max.y, max.z}
    };

    AABB result;
    for (int i = 0; i < 8; ++i) {
        result.expand_by_point(transform.transform_point(corners[i]));
    }
    return result;
}

// Ray
bool Ray::intersects_aabb(const AABB& aabb, float& out_t) const {
    float tmin = (aabb.min.x - origin.x) / (direction.x == 0.0f ? 1e-6f : direction.x);
    float tmax = (aabb.max.x - origin.x) / (direction.x == 0.0f ? 1e-6f : direction.x);
    if (tmin > tmax) std::swap(tmin, tmax);

    float tymin = (aabb.min.y - origin.y) / (direction.y == 0.0f ? 1e-6f : direction.y);
    float tymax = (aabb.max.y - origin.y) / (direction.y == 0.0f ? 1e-6f : direction.y);
    if (tymin > tymax) std::swap(tymin, tymax);

    if ((tmin > tymax) || (tymin > tmax)) return false;
    if (tymin > tmin) tmin = tymin;
    if (tymax < tmax) tmax = tymax;

    float tzmin = (aabb.min.z - origin.z) / (direction.z == 0.0f ? 1e-6f : direction.z);
    float tzmax = (aabb.max.z - origin.z) / (direction.z == 0.0f ? 1e-6f : direction.z);
    if (tzmin > tzmax) std::swap(tzmin, tzmax);

    if ((tmin > tzmax) || (tzmin > tmax)) return false;
    if (tzmin > tmin) tmin = tzmin;
    if (tzmax < tmax) tmax = tzmax;

    if (tmax < 0.0f) return false;
    out_t = (tmin < 0.0f) ? tmax : tmin;
    return true;
}

// Frustum
Frustum Frustum::from_view_projection(const Mat4& vp) {
    Frustum f;
    // Extract planes from column-major matrix vp
    // Left:   col3 + col0
    // Right:  col3 - col0
    // Bottom: col3 + col1
    // Top:    col3 - col1
    // Near:   col3 + col2 (or col2 for [0, 1] range)
    // Far:    col3 - col2

    auto make_plane = [](float a, float b, float c, float d) {
        Plane p;
        float len = std::sqrt(a * a + b * b + c * c);
        float inv = (len > EPSILON) ? (1.0f / len) : 1.0f;
        p.normal = {a * inv, b * inv, c * inv};
        p.distance = d * inv;
        return p;
    };

    // Left
    f.planes[0] = make_plane(
        vp[0].w + vp[0].x,
        vp[1].w + vp[1].x,
        vp[2].w + vp[2].x,
        vp[3].w + vp[3].x
    );
    // Right
    f.planes[1] = make_plane(
        vp[0].w - vp[0].x,
        vp[1].w - vp[1].x,
        vp[2].w - vp[2].x,
        vp[3].w - vp[3].x
    );
    // Bottom
    f.planes[2] = make_plane(
        vp[0].w + vp[0].y,
        vp[1].w + vp[1].y,
        vp[2].w + vp[2].y,
        vp[3].w + vp[3].y
    );
    // Top
    f.planes[3] = make_plane(
        vp[0].w - vp[0].y,
        vp[1].w - vp[1].y,
        vp[2].w - vp[2].y,
        vp[3].w - vp[3].y
    );
    // Near
    f.planes[4] = make_plane(
        vp[0].z,
        vp[1].z,
        vp[2].z,
        vp[3].z
    );
    // Far
    f.planes[5] = make_plane(
        vp[0].w - vp[0].z,
        vp[1].w - vp[1].z,
        vp[2].w - vp[2].z,
        vp[3].w - vp[3].z
    );

    return f;
}

bool Frustum::contains_point(const Vec3& point) const {
    for (int i = 0; i < 6; ++i) {
        if (planes[i].signed_distance(point) < 0.0f) {
            return false;
        }
    }
    return true;
}

bool Frustum::intersects_aabb(const AABB& aabb) const {
    for (int i = 0; i < 6; ++i) {
        // Find p-vertex and n-vertex
        Vec3 p = aabb.min;
        if (planes[i].normal.x >= 0.0f) p.x = aabb.max.x;
        if (planes[i].normal.y >= 0.0f) p.y = aabb.max.y;
        if (planes[i].normal.z >= 0.0f) p.z = aabb.max.z;

        if (planes[i].signed_distance(p) < 0.0f) {
            return false;
        }
    }
    return true;
}

bool Frustum::intersects_sphere(const Vec3& center, float radius) const {
    for (int i = 0; i < 6; ++i) {
        if (planes[i].signed_distance(center) < -radius) {
            return false;
        }
    }
    return true;
}

} // namespace engine::core::math
