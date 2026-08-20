add_library(engine_ui STATIC
    include/engine/ui/editor_ui.h
    include/engine/ui/editor_camera.h
    include/engine/ui/panels/viewport_panel.h
    include/engine/ui/panels/hierarchy_panel.h
    include/engine/ui/panels/inspector_panel.h
    include/engine/ui/panels/content_browser_panel.h
    include/engine/ui/panels/profiler_panel.h
    src/editor_ui.cpp
    src/editor_camera.cpp
    src/viewport_panel.cpp
    src/hierarchy_panel.cpp
    src/inspector_panel.cpp
    src/content_browser_panel.cpp
    src/profiler_panel.cpp
)

target_include_directories(engine_ui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)

target_link_libraries(engine_ui PUBLIC
    engine_core
    engine_rhi
    engine_scene
    engine_vfs
    engine_physics
    engine_audio
    engine_scripting
    engine_input
    imgui::imgui
    imguizmo::imguizmo
)

engine_target_compile_options(engine_ui)
