#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/physics/physics_types.h"

namespace engine::physics {

struct RigidBodyComponent {
    BodyMotionType motion_type{BodyMotionType::Dynamic};
    float mass{1.0f};
    float friction{0.2f};
    float restitution{0.0f}; // Bounciness
    float linear_damping{0.05f};
    float angular_damping{0.05f};
    float gravity_factor{1.0f};
    bool is_sensor{false};
    bool allow_sleeping{true};

    JPH::BodyID body_id;
    bool is_registered{false};
};

struct ColliderComponent {
    ColliderShapeType shape_type{ColliderShapeType::Box};

    // Box dimensions
    core::Vec3 box_half_extents{0.5f, 0.5f, 0.5f};

    // Sphere & Capsule dimensions
    float radius{0.5f};
    float half_height{0.5f};

    // Offset relative to entity transform
    core::Vec3 offset{0.0f, 0.0f, 0.0f};
    core::Quat rotation{0.0f, 0.0f, 0.0f, 1.0f};
};

} // namespace engine::physics
