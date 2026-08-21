#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/core/reflection.h"
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

REFLECT_STRUCT_BEGIN(engine::physics::RigidBodyComponent)
    REFLECT_FIELD(motion_type, "Motion Type", "Static, Kinematic, or Dynamic body motion")
    REFLECT_FIELD(mass, "Mass", "Rigid body mass in kg")
    REFLECT_FIELD(friction, "Friction", "Surface friction coefficient [0.0 - 1.0]")
    REFLECT_FIELD(restitution, "Restitution", "Bounciness factor [0.0 - 1.0]")
    REFLECT_FIELD(linear_damping, "Linear Damping", "Linear velocity drag")
    REFLECT_FIELD(angular_damping, "Angular Damping", "Angular velocity drag")
    REFLECT_FIELD(gravity_factor, "Gravity Factor", "Gravity multiplier")
    REFLECT_FIELD(is_sensor, "Is Sensor", "Trigger volume mode without physical collisions")
    REFLECT_FIELD(allow_sleeping, "Allow Sleeping", "Allows physics body to enter low-power sleep")
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(engine::physics::ColliderComponent)
    REFLECT_FIELD(shape_type, "Shape Type", "Geometric collision primitive shape")
    REFLECT_FIELD(box_half_extents, "Box Half Extents", "Box half-extents in X, Y, Z")
    REFLECT_FIELD(radius, "Radius", "Sphere or capsule radius")
    REFLECT_FIELD(half_height, "Half Height", "Capsule cylinder half-height")
    REFLECT_FIELD(offset, "Offset", "Local offset relative to entity position")
    REFLECT_FIELD(rotation, "Rotation", "Local orientation relative to entity rotation")
REFLECT_STRUCT_END()
