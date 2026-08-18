add_executable(sandbox
    src/main.cpp
)

target_link_libraries(sandbox PRIVATE
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
)

engine_target_compile_options(sandbox)
