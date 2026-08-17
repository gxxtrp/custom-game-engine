add_library(engine_assets STATIC
    include/engine/assets/uuid.h
    include/engine/assets/asset.h
    include/engine/assets/asset_manager.h
    src/asset.cpp
    src/asset_manager.cpp
)

target_include_directories(engine_assets PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(tomlplusplus CONFIG REQUIRED)

target_link_libraries(engine_assets PUBLIC
    engine_core
    engine_vfs
    engine_jobs
    tomlplusplus::tomlplusplus
)

engine_target_compile_options(engine_assets)
