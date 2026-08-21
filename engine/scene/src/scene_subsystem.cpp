#include "engine/scene/scene_subsystem.h"
#include "engine/core/engine_context.h"
#include "engine/core/log.h"

namespace engine::scene {

SceneSubsystem::SceneSubsystem() : m_active_scene("MainScene") {}

SceneSubsystem::SceneSubsystem(std::string default_scene_name) : m_active_scene(std::move(default_scene_name)) {}

SceneSubsystem::~SceneSubsystem() = default;

void SceneSubsystem::declare_dependencies(core::SubsystemDependencyBuilder& builder) {
    (void)builder;
}

bool SceneSubsystem::initialize(core::EngineContext& context) {
    context.register_service<SceneSubsystem>(this);
    context.register_service<Scene>(&m_active_scene);
    LOG_INFO("SceneSubsystem", "SceneSubsystem initialized with active scene '{}'", m_active_scene.get_name());
    return true;
}

void SceneSubsystem::tick(core::EngineContext& context, core::ExecutionPhase phase, float dt) {
    (void)context;
    if (phase == core::ExecutionPhase::Simulation) {
        m_active_scene.update(dt);
    } else if (phase == core::ExecutionPhase::PostSimulation) {
        // Hierarchy / transform synchronization if needed
    }
}

void SceneSubsystem::shutdown(core::EngineContext& context) {
    context.unregister_service<Scene>();
    context.unregister_service<SceneSubsystem>();
    m_active_scene.clear();
    LOG_INFO("SceneSubsystem", "SceneSubsystem shut down cleanly.");
}

} // namespace engine::scene
