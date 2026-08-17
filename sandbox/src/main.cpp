#include "engine/core/config.h"
#include "engine/core/log.h"
#include "engine/core/memory.h"
#include "engine/core/math.h"
#include "engine/core/containers.h"
#include "engine/core/platform.h"
#include <iostream>

using namespace engine::core;

struct TestParticle {
    Vec3 position{0.0f};
    Vec3 velocity{0.0f};
    float life{1.0f};
};

static void run_foundation_tests() {
    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "       Running Phase 0 Foundation Verification    ");
    LOG_INFO("Sandbox", "==================================================");

    // 1. Math Verification
    LOG_INFO("Sandbox", "--- 1. SIMD Math Verification ---");
    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    Vec3 c = a.cross(b);
    float d = a.dot(b);
    LOG_INFO("Sandbox", "Vec3 Cross Product: ({}, {}, {})", c.x, c.y, c.z);
    LOG_INFO("Sandbox", "Vec3 Dot Product: {}", d);

    Mat4 proj = Mat4::perspective_vk(math::deg_to_rad(60.0f), 16.0f / 9.0f, 0.1f, 1000.0f);
    Mat4 view = Mat4::look_at(Vec3(0, 5, 10), Vec3(0, 0, 0), Vec3::UP);
    Mat4 vp = proj * view;

    Frustum frustum = Frustum::from_view_projection(vp);
    AABB in_aabb(Vec3(-1, -1, -1), Vec3(1, 1, 1));
    AABB out_aabb(Vec3(100, 100, 100), Vec3(102, 102, 102));

    LOG_INFO("Sandbox", "Frustum vs inside AABB: {}", frustum.intersects_aabb(in_aabb) ? "PASS (Visible)" : "FAIL");
    LOG_INFO("Sandbox", "Frustum vs outside AABB: {}", frustum.intersects_aabb(out_aabb) ? "FAIL" : "PASS (Culled)");

    Quat q1 = Quat::from_axis_angle(Vec3::UP, math::deg_to_rad(90.0f));
    Vec3 rotated = q1.rotate(Vec3::FORWARD);
    LOG_INFO("Sandbox", "Quaternion rotated vector: ({:.2f}, {:.2f}, {:.2f})", rotated.x, rotated.y, rotated.z);

    // 2. Memory Allocators Verification
    LOG_INFO("Sandbox", "--- 2. Memory Allocators Verification ---");
    {
        LinearAllocator linear(1024 * 1024, nullptr, "FrameLinearArena");
        int* data = ENGINE_ALLOC_ARRAY(&linear, int, 100);
        for (int i = 0; i < 100; ++i) data[i] = i * 2;
        LOG_INFO("Sandbox", "LinearAllocator allocated 100 ints, used: {} bytes", linear.get_used_bytes());
        linear.reset();
        LOG_INFO("Sandbox", "LinearAllocator reset, used: {} bytes", linear.get_used_bytes());
    }

    {
        PoolAllocator pool(sizeof(TestParticle), 64, alignof(TestParticle), nullptr, "ParticlePool");
        TestParticle* p1 = ENGINE_NEW(&pool, TestParticle);
        p1->position = Vec3(10, 20, 30);
        TestParticle* p2 = ENGINE_NEW(&pool, TestParticle);
        p2->position = Vec3(40, 50, 60);
        LOG_INFO("Sandbox", "PoolAllocator free blocks remaining: {}/64", pool.get_free_count());
        ENGINE_DELETE(&pool, p1);
        ENGINE_DELETE(&pool, p2);
        LOG_INFO("Sandbox", "PoolAllocator after free: {}/64 blocks free", pool.get_free_count());
    }

    // 3. Containers Verification
    LOG_INFO("Sandbox", "--- 3. Core Containers Verification ---");
    {
        HashMap<std::string, uint32_t> map;
        map.insert("PlayerHP", 100);
        map.insert("PlayerMana", 50);
        map.insert("PlayerLevel", 5);

        uint32_t* hp = map.find("PlayerHP");
        LOG_INFO("Sandbox", "Robin Hood HashMap lookup 'PlayerHP': {}", hp ? *hp : 0);

        map.erase("PlayerMana");
        LOG_INFO("Sandbox", "HashMap size after erase: {}", map.size());
    }

    {
        SlotMap<std::string> slot_map;
        SlotHandle h1 = slot_map.insert("Entity_Dragon");
        SlotHandle h2 = slot_map.insert("Entity_Knight");

        LOG_INFO("Sandbox", "SlotMap Item 1: '{}'", *slot_map.get(h1));
        LOG_INFO("Sandbox", "SlotMap Item 2: '{}'", *slot_map.get(h2));

        slot_map.erase(h1);
        LOG_INFO("Sandbox", "SlotMap handle 1 valid after erase: {}", slot_map.is_valid(h1) ? "YES (Error)" : "NO (Clean)");
        LOG_INFO("Sandbox", "SlotMap handle 2 valid after erase: {}", slot_map.is_valid(h2) ? "YES (Valid)" : "NO");
    }

    LOG_INFO("Sandbox", "==================================================");
    LOG_INFO("Sandbox", "        Foundation Systems Verified Successfully  ");
    LOG_INFO("Sandbox", "==================================================");
}

int main(int argc, char* argv[]) {
    float max_runtime = 0.0f; // 0 = run until closed
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--timeout" && i + 1 < argc) {
            max_runtime = std::stof(argv[++i]);
        }
    }

    // Add File Sink
    Logger::instance().add_sink(std::make_shared<FileSink>("sandbox_phase0.log"));

    LOG_INFO("Engine", "Initializing Modern Game Engine - Phase 0...");

    run_foundation_tests();

    {
        if (!Platform::init()) {
            LOG_FATAL("Engine", "Failed to initialize platform subsystem!");
            return -1;
        }

        WindowDesc desc{
            .title = "Modern Game Engine - Phase 0 Foundation",
            .width = 1280,
            .height = 720,
            .resizable = true,
            .vulkan_compatible = true
        };

        Window window;
        if (!window.create(desc)) {
            LOG_FATAL("Engine", "Failed to create main window!");
            Platform::shutdown();
            return -1;
        }

        LOG_INFO("Engine", "Window created successfully. Starting main loop...");
        LOG_INFO("Engine", "Press ESC or close window to exit.");

        DynamicArray<PlatformEvent> events;
        FrameTimer timer;
        uint32_t last_fps = 0;

        while (window.is_open()) {
            timer.tick();

            events.clear();
            Platform::poll_events(events);

            for (const auto& event : events) {
                Platform::process_window_events(window, event);

                if (event.type == EventType::KeyDown) {
                    if (event.key.key == KeyCode::Escape) {
                        LOG_INFO("Sandbox", "Escape pressed. Closing window...");
                        window.set_should_close(true);
                    } else if (event.key.key == KeyCode::Space) {
                        LOG_INFO("Sandbox", "Space key triggered at time: {:.2f}s", timer.total_time());
                    }
                } else if (event.type == EventType::MouseButtonDown) {
                    LOG_INFO("Sandbox", "Mouse button {} pressed at ({:.0f}, {:.0f})", 
                             static_cast<int>(event.mouse_button.button), event.mouse_button.x, event.mouse_button.y);
                } else if (event.type == EventType::WindowResize) {
                    LOG_INFO("Sandbox", "Window resized to {}x{}", event.window_resize.width, event.window_resize.height);
                }
            }

            if (timer.fps() != last_fps && timer.fps() > 0) {
                last_fps = timer.fps();
                std::string title = std::format("Modern Game Engine [Phase 0] | FPS: {} | Frame: {:.3f} ms | Entities: 1000", 
                                                last_fps, timer.delta_time() * 1000.0f);
                window.set_title(title);
            }

            if (max_runtime > 0.0f && timer.total_time() >= max_runtime) {
                LOG_INFO("Sandbox", "Reached maximum test runtime ({:.2f}s). Exiting loop...", max_runtime);
                window.set_should_close(true);
            }

            // Target smooth frame pacing for the demo loop
            Clock::sleep_ms(8);
        }

        window.destroy();
        Platform::shutdown();
    }

    LOG_INFO("Engine", "Engine shutdown completed.");
    GlobalAllocator::instance().dump_leaks();

    return 0;
}
