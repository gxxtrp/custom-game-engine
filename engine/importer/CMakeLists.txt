add_library(engine_importer STATIC
    include/engine/importer/importer.h
    src/gltf_importer.cpp
)

target_include_directories(engine_importer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_path(CGLTF_INCLUDE_DIRS "cgltf.h")
target_include_directories(engine_importer PRIVATE ${CGLTF_INCLUDE_DIRS})

target_link_libraries(engine_importer PUBLIC
    engine_core
    engine_assets
    engine_vfs
)

engine_target_compile_options(engine_importer)
