add_subdirectory(core)
add_subdirectory(jobs)
add_subdirectory(events)
add_subdirectory(config)
add_subdirectory(vfs)
add_subdirectory(assets)
add_subdirectory(project)
add_subdirectory(input)
add_subdirectory(importer)
add_subdirectory(rhi)
add_subdirectory(scene)
add_subdirectory(renderer)
add_subdirectory(physics)
add_subdirectory(audio)
add_subdirectory(scripting)
add_subdirectory(ui)

add_library(engine_master STATIC
    include/engine/engine.h
    src/engine.cpp
)

target_include_directories(engine_master PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_master PUBLIC
    engine_core
    engine_jobs
    engine_events
    engine_config
    engine_vfs
    engine_assets
    engine_project
    engine_input
    engine_importer
    engine_rhi
    engine_scene
    engine_renderer
    engine_physics
    engine_audio
    engine_scripting
    engine_ui
)

engine_target_compile_options(engine_master)
