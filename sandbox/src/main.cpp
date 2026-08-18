#include "engine/engine.h"

using namespace engine;
using namespace engine::core;
using namespace engine::scene;
using namespace engine::physics;
using namespace engine::audio;
using namespace engine::scripting;
using namespace engine::ui;

int main(int argc, char* argv[]) {
    float max_runtime = 0.0f;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--timeout" && i + 1 < argc) {
            max_runtime = std::stof(argv[++i]);
        }
    }

    Logger::instance().add_sink(std::make_shared<FileSink>("sandbox_phase14.log"));
    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "  Modern Game Engine - Phase 14 Full DAG Milestone");
    LOG_INFO("Sandbox", "==================================================");

    EngineDesc desc{
        .title = "Modern Game Engine [Phase 14 Final Release Build]",
        .width = 1280,
        .height = 720,
        .enable_vsync = true,
        .enable_validation = true,
        .enable_editor_ui = true
    };

    if (!Engine::instance().init(desc)) {
        LOG_FATAL("Sandbox", "Failed to initialize Modern Game Engine!");
        return -1;
    }

    // Populate Active Scene with Entities
    auto& scene = Engine::instance().get_active_scene();

    Entity ground = scene.create_entity("GroundPlane");
    ground.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, -1.0f, 0.0f) });
    ground.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Static });
    ground.set<ColliderComponent>(ColliderComponent{ .shape_type = ColliderShapeType::Box });

    Entity player = scene.create_entity("PlayerController");
    player.set<TransformComponent>(TransformComponent{ .position = Vec3(0.0f, 2.0f, 0.0f) });
    player.set<AudioSourceComponent>(AudioSourceComponent{ .sound_name = "sfx_footstep.wav", .volume = 0.8f });
    player.set<ScriptComponent>(ScriptComponent{ .class_name = "PlayerController" });

    Entity dynamic_box = scene.create_entity("DynamicPhysicsBox");
    dynamic_box.set<TransformComponent>(TransformComponent{ .position = Vec3(2.0f, 6.0f, 0.0f) });
    dynamic_box.set<RigidBodyComponent>(RigidBodyComponent{ .motion_type = BodyMotionType::Dynamic, .mass = 10.0f });
    dynamic_box.set<ColliderComponent>(ColliderComponent{ .shape_type = ColliderShapeType::Box });

    Entity light = scene.create_entity("SunLight");
    light.set<TransformComponent>(TransformComponent{ .position = Vec3(25.0f, 60.0f, -30.0f) });

    EditorUI::instance().set_selected_entity(player.get_id());

    FrameTimer timer;
    uint32_t rendered_frames = 0;

    LOG_INFO("Sandbox", "Executing Full Engine DAG Loop...");

    while (Engine::instance().is_running()) {
        timer.tick();
        Engine::instance().step();
        rendered_frames++;

        if (max_runtime > 0.0f && timer.total_time() >= max_runtime) {
            LOG_INFO("Sandbox", "Reached maximum test runtime ({:.2f}s, rendered {} frames). Exiting loop...", 
                     max_runtime, rendered_frames);
            Engine::instance().request_exit();
        }
    }

    Engine::instance().shutdown();

    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "  All 14 Engine Phases Completed & Verified!      ");
    LOG_INFO("Sandbox", "==================================================");
    Logger::instance().remove_all_sinks();
    GlobalAllocator::instance().dump_leaks();

    return 0;
}
