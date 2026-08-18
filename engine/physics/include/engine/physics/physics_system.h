#pragma once

#include "engine/core/config.h"
#include "engine/core/math.h"
#include "engine/physics/physics_types.h"
#include "engine/physics/physics_components.h"
#include "engine/scene/scene.h"
#include <memory>

namespace engine::physics {

class BPLayerInterfaceImpl;
class ObjectVsBroadPhaseLayerFilterImpl;
class ObjectLayerPairFilterImpl;

class PhysicsSystem {
public:
    static PhysicsSystem& instance();

    bool init(uint32_t max_bodies = 10240, 
              uint32_t num_body_mutexes = 0, 
              uint32_t max_body_pairs = 10240, 
              uint32_t max_contact_constraints = 10240);
    void shutdown();

    void update(float dt, int collision_steps = 1);

    // Body Management
    JPH::BodyID create_body(const core::Transform& transform, 
                            RigidBodyComponent& rb, 
                            const ColliderComponent& col);
    void destroy_body(JPH::BodyID body_id);

    // Physics Queries
    bool raycast(const core::Vec3& origin, 
                 const core::Vec3& direction, 
                 float max_distance, 
                 RaycastHit& out_hit);

    // Velocity & Forces
    void set_linear_velocity(JPH::BodyID id, const core::Vec3& velocity);
    core::Vec3 get_linear_velocity(JPH::BodyID id) const;
    void add_force(JPH::BodyID id, const core::Vec3& force);
    void add_impulse(JPH::BodyID id, const core::Vec3& impulse);

    core::Vec3 get_body_position(JPH::BodyID id) const;
    core::Quat get_body_rotation(JPH::BodyID id) const;

    // Flecs ECS Synchronization
    void register_scene(scene::Scene& scene);
    void sync_to_physics(scene::Scene& scene);
    void sync_from_physics(scene::Scene& scene);

    JPH::PhysicsSystem* get_jolt_system() { return m_physics_system.get(); }
    JPH::BodyInterface& get_body_interface();

private:
    PhysicsSystem() = default;
    ~PhysicsSystem();

    std::unique_ptr<JPH::TempAllocatorImpl> m_temp_allocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_job_system;
    std::unique_ptr<BPLayerInterfaceImpl> m_broad_phase_layer_interface;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_object_vs_broadphase_layer_filter;
    std::unique_ptr<ObjectLayerPairFilterImpl> m_object_layer_pair_filter;
    std::unique_ptr<JPH::PhysicsSystem> m_physics_system;

    bool m_initialized{false};
};

} // namespace engine::physics
