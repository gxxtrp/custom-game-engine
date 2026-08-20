add_executable(editor
    # Main Entrypoint
    src/main.cpp

    # Core
    src/core/editor_app.cpp
    src/core/editor_state.cpp
    src/core/selection_context.cpp
    src/core/command_history.cpp
    src/core/autosave_manager.cpp
    src/core/editor_preferences.cpp

    # Panels
    src/panels/scene_viewport.cpp
    src/panels/outliner_panel.cpp
    src/panels/inspector_panel.cpp
    src/panels/content_browser.cpp
    src/panels/console_panel.cpp
    src/panels/profiler_panel.cpp
    src/panels/project_hub.cpp

    # Assets & Templates
    src/assets/asset_importer.cpp
    src/assets/prefab_manager.cpp

    # Tools & Diagnostics
    src/tools/debug_draw_pass.cpp
    src/tools/game_exporter.cpp
)

target_include_directories(editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)

target_link_libraries(editor PRIVATE
    engine_master
    imgui::imgui
    imguizmo::imguizmo
)

engine_target_compile_options(editor)
