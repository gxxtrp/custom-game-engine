add_library(engine_core STATIC
    include/engine/core/config.h
    include/engine/core/log.h
    include/engine/core/memory.h
    include/engine/core/math.h
    include/engine/core/containers.h
    include/engine/core/platform.h
    include/engine/core/subsystem.h
    include/engine/core/engine_context.h
    include/engine/core/engine_kernel.h
    include/engine/core/reflection.h
    src/log.cpp
    src/memory.cpp
    src/math.cpp
    src/containers.cpp
    src/platform.cpp
    src/engine_kernel.cpp
)

target_include_directories(engine_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(engine_core PUBLIC SDL3::SDL3)

engine_target_compile_options(engine_core)
