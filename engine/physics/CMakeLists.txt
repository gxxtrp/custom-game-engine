add_library(engine_physics STATIC
    include/engine/physics/physics_types.h
    include/engine/physics/physics_components.h
    include/engine/physics/physics_system.h
    src/physics_system.cpp
)

target_include_directories(engine_physics PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(Jolt CONFIG REQUIRED)

target_link_libraries(engine_physics PUBLIC
    engine_core
    engine_scene
    Jolt::Jolt
)

engine_target_compile_options(engine_physics)
