#include "engine/physics/physics_system.h"
#include "engine/scene/scene_subsystem.h"
#include "engine/core/log.h"
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>

namespace engine::physics {

// BroadPhaseLayerInterface implementation
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        m_object_to_broad_phase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        m_object_to_broad_phase[Layers::MOVING] = BroadPhaseLayers::MOVING;
        m_object_to_broad_phase[Layers::TRIGGER] = BroadPhaseLayers::MOVING;
    }

    uint32_t GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return m_object_to_broad_phase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
            default: return "INVALID";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer m_object_to_broad_phase[Layers::NUM_LAYERS];
};

// ObjectVsBroadPhaseLayerFilter implementation
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
            case Layers::TRIGGER:
                return true;
            default:
                return false;
        }
    }
};

// ObjectLayerPairFilter implementation
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        switch (inObject1) {
            case Layers::NON_MOVING:
                return inObject2 == Layers::MOVING || inObject2 == Layers::TRIGGER;
            case Layers::MOVING:
                return true;
            case Layers::TRIGGER:
                return inObject2 == Layers::MOVING;
            default:
                return false;
        }
    }
};

PhysicsSystem::PhysicsSystem() = default;

PhysicsSystem& PhysicsSystem::instance() {
    static PhysicsSystem s_instance;
    return s_instance;
}

PhysicsSystem::~PhysicsSystem() {
    shutdown();
}

void PhysicsSystem::declare_dependencies(core::SubsystemDependencyBuilder& builder) {
    (void)builder;
}

bool PhysicsSystem::initialize(core::EngineContext& context) {
    if (!init()) {
        return false;
    }
    context.register_service<PhysicsSystem>(this);
    if (auto* scene_sub = context.try_get<scene::SceneSubsystem>()) {
        register_scene(scene_sub->get_active_scene());
    }
    return true;
}

void PhysicsSystem::tick(core::EngineContext& context, core::ExecutionPhase phase, float dt) {
    if (phase == core::ExecutionPhase::Simulation) {
        if (auto* scene_sub = context.try_get<scene::SceneSubsystem>()) {
            sync_to_physics(scene_sub->get_active_scene());
        }
        update(dt);
        if (auto* scene_sub = context.try_get<scene::SceneSubsystem>()) {
            sync_from_physics(scene_sub->get_active_scene());
        }
    }
}

void PhysicsSystem::shutdown(core::EngineContext& context) {
    context.unregister_service<PhysicsSystem>();
    shutdown();
}

bool PhysicsSystem::init(uint32_t max_bodies, 
                         uint32_t num_body_mutexes, 
                         uint32_t max_body_pairs, 
                         uint32_t max_contact_constraints) {
    if (m_initialized) return true;

    // 1. Initialize Jolt Allocator & Factory
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // 2. Allocate memory & worker threads
    m_temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
    m_job_system = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 
                                                              std::max(1u, std::thread::hardware_concurrency() - 1));

    // 3. Broadphase & Layer filters
    m_broad_phase_layer_interface = std::make_unique<BPLayerInterfaceImpl>();
    m_object_vs_broadphase_layer_filter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    m_object_layer_pair_filter = std::make_unique<ObjectLayerPairFilterImpl>();

    // 4. Create Jolt Physics System
    m_physics_system = std::make_unique<JPH::PhysicsSystem>();
    m_physics_system->Init(max_bodies, 
                           num_body_mutexes, 
                           max_body_pairs, 
                           max_contact_constraints, 
                           *m_broad_phase_layer_interface, 
                           *m_object_vs_broadphase_layer_filter, 
                           *m_object_layer_pair_filter);

    m_initialized = true;
    LOG_INFO("Physics", "Initialized Jolt Physics 3D Engine (Max Bodies: {})", max_bodies);
    return true;
}

void PhysicsSystem::shutdown() {
    if (!m_initialized) return;

    m_physics_system.reset();
    m_object_layer_pair_filter.reset();
    m_object_vs_broadphase_layer_filter.reset();
    m_broad_phase_layer_interface.reset();
    m_job_system.reset();
    m_temp_allocator.reset();

    if (JPH::Factory::sInstance) {
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    m_initialized = false;
    LOG_INFO("Physics", "Jolt Physics System shutdown cleanly");
}

void PhysicsSystem::update(float dt, int collision_steps) {
    if (!m_initialized || !m_physics_system) return;
    m_physics_system->Update(dt, collision_steps, m_temp_allocator.get(), m_job_system.get());
}

JPH::BodyInterface& PhysicsSystem::get_body_interface() {
    return m_physics_system->GetBodyInterface();
}

JPH::BodyID PhysicsSystem::create_body(const core::Transform& transform, 
                                      RigidBodyComponent& rb, 
                                      const ColliderComponent& col) {
    if (!m_initialized) return JPH::BodyID();

    // 1. Create Collision Shape with Transform Scale factored in
    core::Vec3 scale = transform.scale;
    core::Vec3 scaled_extents = core::Vec3(
        std::max(0.01f, std::abs(col.box_half_extents.x * scale.x)),
        std::max(0.01f, std::abs(col.box_half_extents.y * scale.y)),
        std::max(0.01f, std::abs(col.box_half_extents.z * scale.z))
    );
    float max_radial_scale = std::max({std::abs(scale.x), std::abs(scale.z), 0.01f});
    float max_uniform_scale = std::max({std::abs(scale.x), std::abs(scale.y), std::abs(scale.z), 0.01f});
    float scaled_radius = std::max(0.01f, col.radius * max_radial_scale);
    float scaled_sphere_radius = std::max(0.01f, col.radius * max_uniform_scale);
    float scaled_half_height = std::max(0.01f, col.half_height * std::max(0.01f, std::abs(scale.y)));

    JPH::Ref<JPH::Shape> shape;
    switch (col.shape_type) {
        case ColliderShapeType::Box:
            shape = new JPH::BoxShape(to_jolt(scaled_extents));
            break;
        case ColliderShapeType::Sphere:
            shape = new JPH::SphereShape(scaled_sphere_radius);
            break;
        case ColliderShapeType::Capsule:
            shape = new JPH::CapsuleShape(scaled_half_height, scaled_radius);
            break;
        case ColliderShapeType::Cylinder:
            shape = new JPH::CylinderShape(scaled_half_height, scaled_radius);
            break;
        default:
            shape = new JPH::BoxShape(to_jolt(scaled_extents));
            break;
    }

    if (col.offset.length() > 0.0f || col.rotation != core::Quat::identity()) {
        shape = new JPH::RotatedTranslatedShape(to_jolt(col.offset), to_jolt(col.rotation), shape);
    }

    // 2. Motion Type & Layer
    JPH::EMotionType motion_type = JPH::EMotionType::Dynamic;
    JPH::ObjectLayer layer = Layers::MOVING;

    if (rb.motion_type == BodyMotionType::Static) {
        motion_type = JPH::EMotionType::Static;
        layer = Layers::NON_MOVING;
    } else if (rb.motion_type == BodyMotionType::Kinematic) {
        motion_type = JPH::EMotionType::Kinematic;
        layer = Layers::MOVING;
    }

    if (rb.is_sensor) {
        layer = Layers::TRIGGER;
    }

    // 3. Creation Settings
    JPH::BodyCreationSettings settings(
        shape,
        to_jolt(transform.position),
        to_jolt(transform.rotation),
        motion_type,
        layer
    );

    settings.mFriction = rb.friction;
    settings.mRestitution = rb.restitution;
    settings.mLinearDamping = rb.linear_damping;
    settings.mAngularDamping = rb.angular_damping;
    settings.mGravityFactor = rb.gravity_factor;
    settings.mIsSensor = rb.is_sensor;
    settings.mAllowSleeping = rb.allow_sleeping;

    if (rb.mass > 0.0f && motion_type == JPH::EMotionType::Dynamic) {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = rb.mass;
    }

    JPH::Body* body = m_physics_system->GetBodyInterface().CreateBody(settings);
    if (!body) {
        LOG_ERROR("Physics", "Failed to create Jolt body!");
        return JPH::BodyID();
    }

    JPH::BodyID body_id = body->GetID();
    m_physics_system->GetBodyInterface().AddBody(body_id, JPH::EActivation::Activate);

    rb.body_id = body_id;
    rb.is_registered = true;

    return body_id;
}

void PhysicsSystem::destroy_body(JPH::BodyID body_id) {
    if (!m_initialized || body_id.IsInvalid()) return;
    auto& bi = m_physics_system->GetBodyInterface();
    bi.RemoveBody(body_id);
    bi.DestroyBody(body_id);
}

bool PhysicsSystem::raycast(const core::Vec3& origin, 
                           const core::Vec3& direction, 
                           float max_distance, 
                           RaycastHit& out_hit) {
    if (!m_initialized) return false;

    core::Vec3 dir_norm = direction.normalized();
    JPH::RRayCast ray(to_jolt(origin), to_jolt(dir_norm * max_distance));
    JPH::RayCastResult result;

    if (m_physics_system->GetNarrowPhaseQuery().CastRay(ray, result)) {
        out_hit.has_hit = true;
        out_hit.fraction = result.mFraction;
        out_hit.body_id = result.mBodyID;
        out_hit.position = origin + dir_norm * (max_distance * result.mFraction);

        // Get surface normal
        JPH::BodyLockRead lock(m_physics_system->GetBodyLockInterface(), result.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Body& body = lock.GetBody();
            JPH::Vec3 jolt_norm = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ray.GetPointOnRay(result.mFraction));
            out_hit.normal = to_engine(jolt_norm);
        }
        return true;
    }

    out_hit.has_hit = false;
    return false;
}

void PhysicsSystem::set_linear_velocity(JPH::BodyID id, const core::Vec3& velocity) {
    if (m_initialized && !id.IsInvalid()) {
        m_physics_system->GetBodyInterface().SetLinearVelocity(id, to_jolt(velocity));
    }
}

core::Vec3 PhysicsSystem::get_linear_velocity(JPH::BodyID id) const {
    if (m_initialized && !id.IsInvalid()) {
        return to_engine(m_physics_system->GetBodyInterface().GetLinearVelocity(id));
    }
    return core::Vec3(0.0f, 0.0f, 0.0f);
}

void PhysicsSystem::add_force(JPH::BodyID id, const core::Vec3& force) {
    if (m_initialized && !id.IsInvalid()) {
        m_physics_system->GetBodyInterface().AddForce(id, to_jolt(force));
    }
}

void PhysicsSystem::add_impulse(JPH::BodyID id, const core::Vec3& impulse) {
    if (m_initialized && !id.IsInvalid()) {
        m_physics_system->GetBodyInterface().AddImpulse(id, to_jolt(impulse));
    }
}

core::Vec3 PhysicsSystem::get_body_position(JPH::BodyID id) const {
    if (m_initialized && !id.IsInvalid()) {
        return to_engine(m_physics_system->GetBodyInterface().GetPosition(id));
    }
    return core::Vec3(0.0f, 0.0f, 0.0f);
}

core::Quat PhysicsSystem::get_body_rotation(JPH::BodyID id) const {
    if (m_initialized && !id.IsInvalid()) {
        return to_engine(m_physics_system->GetBodyInterface().GetRotation(id));
    }
    return core::Quat::identity();
}

void PhysicsSystem::register_scene(scene::Scene& scene) {
    // Register Flecs physics components
    scene.get_world().component<RigidBodyComponent>();
    scene.get_world().component<ColliderComponent>();
}

void PhysicsSystem::sync_to_physics(scene::Scene& scene) {
    if (!m_initialized) return;

    auto& world = scene.get_world();
    world.each([this](flecs::entity e, RigidBodyComponent& rb, const ColliderComponent& col, const scene::TransformComponent& transform) {
        if (!rb.is_registered) {
            core::Transform t{
                .position = transform.position,
                .rotation = transform.rotation,
                .scale = transform.scale
            };
            create_body(t, rb, col);
        } else if (rb.motion_type == BodyMotionType::Kinematic) {
            auto& bi = m_physics_system->GetBodyInterface();
            bi.SetPositionAndRotation(rb.body_id, to_jolt(transform.position), to_jolt(transform.rotation), JPH::EActivation::Activate);
        }
    });
}

void PhysicsSystem::sync_from_physics(scene::Scene& scene) {
    if (!m_initialized) return;

    auto& world = scene.get_world();
    auto& bi = m_physics_system->GetBodyInterface();

    world.each([&bi](flecs::entity e, const RigidBodyComponent& rb, scene::TransformComponent& transform) {
        if (rb.is_registered && rb.motion_type == BodyMotionType::Dynamic && !rb.body_id.IsInvalid()) {
            JPH::RVec3 pos;
            JPH::Quat rot;
            bi.GetPositionAndRotation(rb.body_id, pos, rot);

            transform.position = to_engine(pos);
            transform.rotation = to_engine(rot);
            transform.is_dirty = true;
        }
    });
}

} // namespace engine::physics
