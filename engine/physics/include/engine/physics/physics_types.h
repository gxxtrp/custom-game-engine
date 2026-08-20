#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include <cstdint>

// Jolt includes
#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

namespace engine::physics {

namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer TRIGGER = 2;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 3;
}

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr uint32_t NUM_LAYERS = 2;
}

enum class BodyMotionType : uint8_t {
    Static = 0,
    Kinematic,
    Dynamic
};

enum class ColliderShapeType : uint8_t {
    Box = 0,
    Sphere,
    Capsule,
    Cylinder,
    Mesh
};

struct RaycastHit {
    bool has_hit{false};
    float fraction{1.0f};
    core::Vec3 position{0.0f, 0.0f, 0.0f};
    core::Vec3 normal{0.0f, 1.0f, 0.0f};
    JPH::BodyID body_id;
};

// Math conversion helpers between Engine math and Jolt math
inline JPH::Vec3 to_jolt(const core::Vec3& v) {
    return JPH::Vec3(v.x, v.y, v.z);
}

inline core::Vec3 to_engine(const JPH::Vec3& v) {
    return core::Vec3(v.GetX(), v.GetY(), v.GetZ());
}

inline JPH::Quat to_jolt(const core::Quat& q) {
    return JPH::Quat(q.x, q.y, q.z, q.w);
}

inline core::Quat to_engine(const JPH::Quat& q) {
    return core::Quat(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
}

} // namespace engine::physics
