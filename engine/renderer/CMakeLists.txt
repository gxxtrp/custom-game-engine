add_library(engine_renderer STATIC
    include/engine/renderer/render_graph_types.h
    include/engine/renderer/render_pass.h
    include/engine/renderer/render_graph.h
    include/engine/renderer/mesh.h
    include/engine/renderer/material.h
    include/engine/renderer/gpu_scene.h
    src/render_pass.cpp
    src/render_graph.cpp
    src/mesh.cpp
    src/material.cpp
    src/gpu_scene.cpp
)

target_include_directories(engine_renderer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_renderer PUBLIC
    engine_core
    engine_rhi
    engine_importer
    engine_assets
)

engine_target_compile_options(engine_renderer)
