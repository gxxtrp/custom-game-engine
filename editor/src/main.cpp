#include "editor/core/editor_app.h"
#include "engine/core/log.h"
#include "engine/core/memory.h"
#include <string_view>

int main(int argc, char* argv[]) {
    float timeout = 0.0f;
    std::string project_dir = "";
    std::string project_name = "";
    uint32_t width = 1600;
    uint32_t height = 900;
    bool vsync = true;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--timeout" && i + 1 < argc) {
            timeout = std::stof(argv[++i]);
        } else if (arg == "--project" && i + 1 < argc) {
            project_dir = argv[++i];
        } else if (arg == "--name" && i + 1 < argc) {
            project_name = argv[++i];
        } else if (arg == "--width" && i + 1 < argc) {
            width = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--height" && i + 1 < argc) {
            height = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--no-vsync") {
            vsync = false;
        }
    }

    engine::core::Logger::instance().add_sink(std::make_shared<engine::core::FileSink>("editor.log"));
    LOG_INFO("Editor", "==================================================");
    LOG_INFO("Editor", "  Launching Modern Game Engine Dedicated Editor   ");
    LOG_INFO("Editor", "==================================================");

    editor::EditorAppDesc desc{
        .title = "Modern Game Engine Editor [Vulkan 1.3]",
        .width = width,
        .height = height,
        .vsync = vsync,
        .enable_validation = true,
        .project_path = project_dir,
        .project_directory = project_dir,
        .project_name = project_name
    };

    if (!editor::EditorApp::instance().init(desc)) {
        LOG_FATAL("Editor", "Failed to initialize Editor Application!");
        return -1;
    }

    engine::core::FrameTimer timer;
    uint32_t rendered_frames = 0;

    LOG_INFO("Editor", "Entering Editor Application Main Loop...");

    while (editor::EditorApp::instance().is_running()) {
        timer.tick();
        editor::EditorApp::instance().step();
        rendered_frames++;

        if (timeout > 0.0f && timer.total_time() >= timeout) {
            LOG_INFO("Editor", "Reached automated test timeout ({:.2f}s, rendered {} frames). Requesting exit...",
                     timeout, rendered_frames);
            editor::EditorApp::instance().request_exit();
        }
    }

    editor::EditorApp::instance().shutdown();

    LOG_INFO("Editor", "==================================================");
    LOG_INFO("Editor", "    Editor Executed and Exited Successfully!      ");
    LOG_INFO("Editor", "==================================================");

    engine::core::Logger::instance().remove_all_sinks();
    engine::core::GlobalAllocator::instance().dump_leaks();

    return 0;
}
