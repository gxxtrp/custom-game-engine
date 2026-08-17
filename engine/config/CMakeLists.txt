add_library(engine_config STATIC
    include/engine/config/cvar.h
    include/engine/config/config_system.h
    src/cvar.cpp
    src/config_system.cpp
)

target_include_directories(engine_config PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(tomlplusplus CONFIG REQUIRED)

target_link_libraries(engine_config PUBLIC
    engine_core
    tomlplusplus::tomlplusplus
)

engine_target_compile_options(engine_config)
