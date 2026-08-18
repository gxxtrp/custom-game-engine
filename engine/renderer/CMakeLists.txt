add_library(engine_renderer STATIC
    include/engine/renderer/render_graph_types.h
    include/engine/renderer/render_pass.h
    include/engine/renderer/render_graph.h
    src/render_pass.cpp
    src/render_graph.cpp
)

target_include_directories(engine_renderer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_renderer PUBLIC
    engine_core
    engine_rhi
)

engine_target_compile_options(engine_renderer)
