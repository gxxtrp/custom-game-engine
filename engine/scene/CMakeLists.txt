add_library(engine_scene STATIC
    include/engine/scene/components.h
    include/engine/scene/entity.h
    include/engine/scene/scene.h
    include/engine/scene/map_serializer.h
    include/engine/scene/scene_importer.h
    src/entity.cpp
    src/scene.cpp
    src/map_serializer.cpp
    src/scene_importer.cpp
)

target_include_directories(engine_scene PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(flecs CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

target_link_libraries(engine_scene PUBLIC
    engine_core
    engine_assets
    engine_importer
    engine_vfs
    flecs::flecs_static
    tomlplusplus::tomlplusplus
)

engine_target_compile_options(engine_scene)
