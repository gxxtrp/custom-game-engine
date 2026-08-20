#include "runtime/runtime_app.h"
#include "engine/core/log.h"
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    LOG_INFO("Runtime", "==================================================");
    LOG_INFO("Runtime", "   Launching Modern Game Engine Standalone Game   ");
    LOG_INFO("Runtime", "==================================================");

    // 1. Parse Command Line Arguments
    runtime::RuntimeAppDesc desc{};
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--project" && i + 1 < argc) {
            desc.project_path = argv[++i];
        } else if (arg == "--timeout" && i + 1 < argc) {
            desc.timeout = std::stof(argv[++i]);
        } else if (arg == "--no-validation") {
            desc.enable_validation = false;
        } else if (arg == "--headless") {
            desc.headless = true;
        }
    }

    // 2. Initialize Game Runtime
    if (!runtime::RuntimeApp::instance().init(desc)) {
        LOG_FATAL("Runtime", "Fatal: Failed to initialize Game Runtime!");
        return 1;
    }

    // 3. Run Game Loop
    runtime::RuntimeApp::instance().run();

    // 4. Shutdown
    runtime::RuntimeApp::instance().shutdown();
    LOG_INFO("Runtime", "Game Exited Successfully!");
    return 0;
}
