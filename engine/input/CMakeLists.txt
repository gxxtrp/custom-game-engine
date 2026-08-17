add_library(engine_input STATIC
    include/engine/input/input_manager.h
    src/input_manager.cpp
)

target_include_directories(engine_input PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_input PUBLIC
    engine_core
)

engine_target_compile_options(engine_input)
