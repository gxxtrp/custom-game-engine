cmake_minimum_required(VERSION 3.25)
project(ModernGameEngine
    VERSION 0.1.0
    LANGUAGES C CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerFlags)
include(Platforms)

add_subdirectory(engine)
add_subdirectory(tools)
add_subdirectory(runtime)

enable_testing()
add_subdirectory(tests)
