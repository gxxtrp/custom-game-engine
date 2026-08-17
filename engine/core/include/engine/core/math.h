#pragma once

#include "engine/core/config.h"
#include <cmath>
#include <immintrin.h>
#include <algorithm>
#include <string>
#include <format>

namespace engine::core::math {

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 6.28318530717958647692f;
constexpr float HALF_PI = 1.57079632679489661923f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;
constexpr float EPSILON = 1e-6f;

ENGINE_FORCE_INLINE constexpr float deg_to_rad(float deg) { return deg * DEG_TO_RAD; }
ENGINE_FORCE_INLINE constexpr float rad_to_deg(float rad) { return rad * RAD_TO_DEG; }
ENGINE_FORCE_INLINE constexpr float clamp(float v, float min_v, float max_v) { return std::clamp(v, min_v, max_v); }
ENGINE_FORCE_INLINE constexpr float lerp(float a, float b, float t) { return a + t * (b - a); }
ENGINE_FORCE_INLINE bool is_nearly_equal(float a, float b, float eps = EPSILON) { return std::abs(a - b) <= eps; }
ENGINE_FORCE_INLINE bool is_nearly_zero(float v, float eps = EPSILON) { return std::abs(v) <= eps; }

// Forward declarations
struct Vec2;
struct Vec3;
struct alignas(16) Vec4;
struct Mat3;
struct alignas(16) Mat4;
struct alignas(16) Quat;
struct AABB;
struct Ray;
struct Plane;
struct Frustum;

// Vector 2
struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}
    explicit constexpr Vec2(float scalar) : x(scalar), y(scalar) {}

    ENGINE_FORCE_INLINE float& operator[](size_t index) { return (&x)[index]; }
    ENGINE_FORCE_INLINE const float& operator[](size_t index) const { return (&x)[index]; }

    ENGINE_FORCE_INLINE Vec2 operator+(const Vec2& rhs) const { return {x + rhs.x, y + rhs.y}; }
    ENGINE_FORCE_INLINE Vec2 operator-(const Vec2& rhs) const { return {x - rhs.x, y - rhs.y}; }
    ENGINE_FORCE_INLINE Vec2 operator*(float scalar) const { return {x * scalar, y * scalar}; }
    ENGINE_FORCE_INLINE Vec2 operator*(const Vec2& rhs) const { return {x * rhs.x, y * rhs.y}; }
    ENGINE_FORCE_INLINE Vec2 operator/(float scalar) const { float inv = 1.0f / scalar; return {x * inv, y * inv}; }
    ENGINE_FORCE_INLINE Vec2 operator-() const { return {-x, -y}; }

    ENGINE_FORCE_INLINE Vec2& operator+=(const Vec2& rhs) { x += rhs.x; y += rhs.y; return *this; }
    ENGINE_FORCE_INLINE Vec2& operator-=(const Vec2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
    ENGINE_FORCE_INLINE Vec2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
    ENGINE_FORCE_INLINE Vec2& operator/=(float scalar) { float inv = 1.0f / scalar; x *= inv; y *= inv; return *this; }

    ENGINE_FORCE_INLINE float dot(const Vec2& rhs) const { return x * rhs.x + y * rhs.y; }
    ENGINE_FORCE_INLINE float length_sq() const { return dot(*this); }
    ENGINE_FORCE_INLINE float length() const { return std::sqrt(length_sq()); }
    ENGINE_FORCE_INLINE Vec2 normalized() const {
        float len = length();
        return len > EPSILON ? (*this / len) : Vec2(0.0f);
    }
};

// Vector 3
struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    explicit constexpr Vec3(float scalar) : x(scalar), y(scalar), z(scalar) {}
    constexpr Vec3(const Vec2& xy, float z_) : x(xy.x), y(xy.y), z(z_) {}

    ENGINE_FORCE_INLINE float& operator[](size_t index) { return (&x)[index]; }
    ENGINE_FORCE_INLINE const float& operator[](size_t index) const { return (&x)[index]; }

    ENGINE_FORCE_INLINE Vec3 operator+(const Vec3& rhs) const { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
    ENGINE_FORCE_INLINE Vec3 operator-(const Vec3& rhs) const { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
    ENGINE_FORCE_INLINE Vec3 operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar}; }
    ENGINE_FORCE_INLINE Vec3 operator*(const Vec3& rhs) const { return {x * rhs.x, y * rhs.y, z * rhs.z}; }
    ENGINE_FORCE_INLINE Vec3 operator/(float scalar) const { float inv = 1.0f / scalar; return {x * inv, y * inv, z * inv}; }
    ENGINE_FORCE_INLINE Vec3 operator-() const { return {-x, -y, -z}; }

    ENGINE_FORCE_INLINE Vec3& operator+=(const Vec3& rhs) { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
    ENGINE_FORCE_INLINE Vec3& operator-=(const Vec3& rhs) { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
    ENGINE_FORCE_INLINE Vec3& operator*=(float scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
    ENGINE_FORCE_INLINE Vec3& operator/=(float scalar) { float inv = 1.0f / scalar; x *= inv; y *= inv; z *= inv; return *this; }

    ENGINE_FORCE_INLINE float dot(const Vec3& rhs) const { return x * rhs.x + y * rhs.y + z * rhs.z; }
    ENGINE_FORCE_INLINE Vec3 cross(const Vec3& rhs) const {
        return {
            y * rhs.z - z * rhs.y,
            z * rhs.x - x * rhs.z,
            x * rhs.y - y * rhs.x
        };
    }
    ENGINE_FORCE_INLINE float length_sq() const { return dot(*this); }
    ENGINE_FORCE_INLINE float length() const { return std::sqrt(length_sq()); }
    ENGINE_FORCE_INLINE Vec3 normalized() const {
        float len = length();
        return len > EPSILON ? (*this / len) : Vec3(0.0f);
    }

    static const Vec3 ZERO;
    static const Vec3 ONE;
    static const Vec3 UP;
    static const Vec3 DOWN;
    static const Vec3 LEFT;
    static const Vec3 RIGHT;
    static const Vec3 FORWARD;
    static const Vec3 BACK;
};

// Vector 4 (SIMD-accelerated)
struct alignas(16) Vec4 {
    union {
        struct { float x, y, z, w; };
        __m128 simd;
    };

    ENGINE_FORCE_INLINE Vec4() : simd(_mm_setzero_ps()) {}
    ENGINE_FORCE_INLINE Vec4(float x_, float y_, float z_, float w_) : simd(_mm_setr_ps(x_, y_, z_, w_)) {}
    ENGINE_FORCE_INLINE explicit Vec4(float scalar) : simd(_mm_set1_ps(scalar)) {}
    ENGINE_FORCE_INLINE Vec4(const Vec3& xyz, float w_) : simd(_mm_setr_ps(xyz.x, xyz.y, xyz.z, w_)) {}
    ENGINE_FORCE_INLINE Vec4(__m128 s) : simd(s) {}

    ENGINE_FORCE_INLINE float& operator[](size_t index) { return (&x)[index]; }
    ENGINE_FORCE_INLINE const float& operator[](size_t index) const { return (&x)[index]; }

    ENGINE_FORCE_INLINE Vec4 operator+(const Vec4& rhs) const { return Vec4(_mm_add_ps(simd, rhs.simd)); }
    ENGINE_FORCE_INLINE Vec4 operator-(const Vec4& rhs) const { return Vec4(_mm_sub_ps(simd, rhs.simd)); }
    ENGINE_FORCE_INLINE Vec4 operator*(float scalar) const { return Vec4(_mm_mul_ps(simd, _mm_set1_ps(scalar))); }
    ENGINE_FORCE_INLINE Vec4 operator*(const Vec4& rhs) const { return Vec4(_mm_mul_ps(simd, rhs.simd)); }
    ENGINE_FORCE_INLINE Vec4 operator/(float scalar) const { return Vec4(_mm_div_ps(simd, _mm_set1_ps(scalar))); }
    ENGINE_FORCE_INLINE Vec4 operator-() const { return Vec4(_mm_xor_ps(simd, _mm_set1_ps(-0.0f))); }

    ENGINE_FORCE_INLINE Vec4& operator+=(const Vec4& rhs) { simd = _mm_add_ps(simd, rhs.simd); return *this; }
    ENGINE_FORCE_INLINE Vec4& operator-=(const Vec4& rhs) { simd = _mm_sub_ps(simd, rhs.simd); return *this; }
    ENGINE_FORCE_INLINE Vec4& operator*=(float scalar) { simd = _mm_mul_ps(simd, _mm_set1_ps(scalar)); return *this; }

    ENGINE_FORCE_INLINE float dot(const Vec4& rhs) const {
        __m128 d = _mm_dp_ps(simd, rhs.simd, 0xF1);
        return _mm_cvtss_f32(d);
    }
    ENGINE_FORCE_INLINE float length_sq() const { return dot(*this); }
    ENGINE_FORCE_INLINE float length() const { return std::sqrt(length_sq()); }
    ENGINE_FORCE_INLINE Vec4 normalized() const {
        float len = length();
        return len > EPSILON ? (*this / len) : Vec4(0.0f);
    }

    ENGINE_FORCE_INLINE Vec3 xyz() const { return Vec3(x, y, z); }
};

// 4x4 Matrix (Column-major, SIMD accelerated)
struct alignas(16) Mat4 {
    Vec4 cols[4];

    ENGINE_FORCE_INLINE Mat4() {
        cols[0] = Vec4(1, 0, 0, 0);
        cols[1] = Vec4(0, 1, 0, 0);
        cols[2] = Vec4(0, 0, 1, 0);
        cols[3] = Vec4(0, 0, 0, 1);
    }

    ENGINE_FORCE_INLINE Mat4(const Vec4& c0, const Vec4& c1, const Vec4& c2, const Vec4& c3) {
        cols[0] = c0; cols[1] = c1; cols[2] = c2; cols[3] = c3;
    }

    static Mat4 identity() { return Mat4(); }

    ENGINE_FORCE_INLINE Vec4& operator[](size_t col) { return cols[col]; }
    ENGINE_FORCE_INLINE const Vec4& operator[](size_t col) const { return cols[col]; }

    ENGINE_FORCE_INLINE Mat4 operator*(const Mat4& rhs) const {
        Mat4 result;
        for (int i = 0; i < 4; ++i) {
            __m128 r = _mm_mul_ps(cols[0].simd, _mm_set1_ps(rhs.cols[i].x));
            r = _mm_add_ps(r, _mm_mul_ps(cols[1].simd, _mm_set1_ps(rhs.cols[i].y)));
            r = _mm_add_ps(r, _mm_mul_ps(cols[2].simd, _mm_set1_ps(rhs.cols[i].z)));
            r = _mm_add_ps(r, _mm_mul_ps(cols[3].simd, _mm_set1_ps(rhs.cols[i].w)));
            result.cols[i].simd = r;
        }
        return result;
    }

    ENGINE_FORCE_INLINE Vec4 operator*(const Vec4& v) const {
        __m128 r = _mm_mul_ps(cols[0].simd, _mm_set1_ps(v.x));
        r = _mm_add_ps(r, _mm_mul_ps(cols[1].simd, _mm_set1_ps(v.y)));
        r = _mm_add_ps(r, _mm_mul_ps(cols[2].simd, _mm_set1_ps(v.z)));
        r = _mm_add_ps(r, _mm_mul_ps(cols[3].simd, _mm_set1_ps(v.w)));
        return Vec4(r);
    }

    ENGINE_FORCE_INLINE Vec3 transform_point(const Vec3& p) const {
        Vec4 v = (*this) * Vec4(p, 1.0f);
        float inv_w = 1.0f / v.w;
        return Vec3(v.x * inv_w, v.y * inv_w, v.z * inv_w);
    }

    ENGINE_FORCE_INLINE Vec3 transform_vector(const Vec3& v) const {
        Vec4 res = (*this) * Vec4(v, 0.0f);
        return res.xyz();
    }

    Mat4 transposed() const;
    Mat4 inverted() const;

    static Mat4 translation(const Vec3& pos);
    static Mat4 scaling(const Vec3& scale);
    static Mat4 rotation_x(float rad);
    static Mat4 rotation_y(float rad);
    static Mat4 rotation_z(float rad);
    static Mat4 rotation_axis(const Vec3& axis, float rad);

    // Vulkan Perspective (Depth [0, 1], Right-handed, Y-down or Y-up)
    static Mat4 perspective_vk(float fov_rad, float aspect, float near_z, float far_z);
    static Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up);
    static Mat4 orthographic(float left, float right, float bottom, float top, float near_z, float far_z);
};

// Quaternion (SIMD-accelerated)
struct alignas(16) Quat {
    union {
        struct { float x, y, z, w; };
        __m128 simd;
    };

    ENGINE_FORCE_INLINE Quat() : simd(_mm_setr_ps(0, 0, 0, 1)) {}
    ENGINE_FORCE_INLINE Quat(float x_, float y_, float z_, float w_) : simd(_mm_setr_ps(x_, y_, z_, w_)) {}
    ENGINE_FORCE_INLINE Quat(__m128 s) : simd(s) {}

    static Quat identity() { return Quat(0, 0, 0, 1); }
    static Quat from_axis_angle(const Vec3& axis, float rad);
    static Quat from_euler(float pitch_rad, float yaw_rad, float roll_rad);

    ENGINE_FORCE_INLINE Quat operator*(const Quat& rhs) const {
        return Quat(
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
            w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z
        );
    }

    ENGINE_FORCE_INLINE Quat conjugate() const { return Quat(-x, -y, -z, w); }
    ENGINE_FORCE_INLINE float dot(const Quat& rhs) const {
        return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
    }
    ENGINE_FORCE_INLINE float length() const { return std::sqrt(dot(*this)); }
    ENGINE_FORCE_INLINE Quat normalized() const {
        float len = length();
        if (len <= EPSILON) return identity();
        float inv = 1.0f / len;
        return Quat(x * inv, y * inv, z * inv, w * inv);
    }

    Vec3 rotate(const Vec3& v) const;
    Mat4 to_mat4() const;
    static Quat slerp(const Quat& a, const Quat& b, float t);
};

// Axis-Aligned Bounding Box (AABB)
struct AABB {
    Vec3 min{1e30f, 1e30f, 1e30f};
    Vec3 max{-1e30f, -1e30f, -1e30f};

    AABB() = default;
    AABB(const Vec3& min_, const Vec3& max_) : min(min_), max(max_) {}

    ENGINE_FORCE_INLINE Vec3 center() const { return (min + max) * 0.5f; }
    ENGINE_FORCE_INLINE Vec3 extents() const { return (max - min) * 0.5f; }
    ENGINE_FORCE_INLINE Vec3 size() const { return max - min; }

    ENGINE_FORCE_INLINE void expand_by_point(const Vec3& p) {
        min.x = std::min(min.x, p.x);
        min.y = std::min(min.y, p.y);
        min.z = std::min(min.z, p.z);
        max.x = std::max(max.x, p.x);
        max.y = std::max(max.y, p.y);
        max.z = std::max(max.z, p.z);
    }

    ENGINE_FORCE_INLINE bool intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }

    AABB transformed(const Mat4& transform) const;
};

// Ray
struct Ray {
    Vec3 origin{0.0f};
    Vec3 direction{0.0f, 0.0f, -1.0f};

    Ray() = default;
    Ray(const Vec3& orig, const Vec3& dir) : origin(orig), direction(dir.normalized()) {}

    ENGINE_FORCE_INLINE Vec3 point_at(float t) const { return origin + direction * t; }
    bool intersects_aabb(const AABB& aabb, float& out_t) const;
};

// Plane
struct Plane {
    Vec3 normal{0.0f, 1.0f, 0.0f};
    float distance{0.0f}; // dot(normal, point) + distance = 0

    Plane() = default;
    Plane(const Vec3& n, float d) : normal(n.normalized()), distance(d) {}
    Plane(const Vec3& p0, const Vec3& p1, const Vec3& p2) {
        normal = (p1 - p0).cross(p2 - p0).normalized();
        distance = -normal.dot(p0);
    }

    ENGINE_FORCE_INLINE float signed_distance(const Vec3& point) const {
        return normal.dot(point) + distance;
    }
};

// Frustum (6 Planes)
struct Frustum {
    Plane planes[6]; // Left, Right, Bottom, Top, Near, Far

    static Frustum from_view_projection(const Mat4& vp);
    bool contains_point(const Vec3& point) const;
    bool intersects_aabb(const AABB& aabb) const;
    bool intersects_sphere(const Vec3& center, float radius) const;
};

// Transform
struct Transform {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Quat rotation{0.0f, 0.0f, 0.0f, 1.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    Mat4 to_mat4() const {
        return Mat4::translation(position) * rotation.to_mat4() * Mat4::scaling(scale);
    }
};

} // namespace engine::core::math

namespace engine::core {
    using Vec2 = math::Vec2;
    using Vec3 = math::Vec3;
    using Vec4 = math::Vec4;
    using Mat4 = math::Mat4;
    using Quat = math::Quat;
    using AABB = math::AABB;
    using Ray = math::Ray;
    using Plane = math::Plane;
    using Frustum = math::Frustum;
    using Transform = math::Transform;
}
