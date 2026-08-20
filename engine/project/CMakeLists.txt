add_library(engine_project STATIC
    include/engine/project/project.h
    src/project.cpp
)

target_include_directories(engine_project PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(tomlplusplus CONFIG REQUIRED)

target_link_libraries(engine_project PUBLIC
    engine_core
    engine_assets
    engine_vfs
    tomlplusplus::tomlplusplus
)

engine_target_compile_options(engine_project)
