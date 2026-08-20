add_library(engine_scripting STATIC
    include/engine/scripting/script_types.h
    include/engine/scripting/script_components.h
    include/engine/scripting/script_engine.h
    src/script_engine.cpp
)

target_include_directories(engine_scripting PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(sol2 CONFIG REQUIRED)
find_package(Lua REQUIRED)

target_include_directories(engine_scripting PUBLIC ${LUA_INCLUDE_DIR})

target_link_libraries(engine_scripting PUBLIC
    engine_core
    engine_vfs
    engine_scene
    engine_input
    engine_audio
    engine_physics
    sol2::sol2
    ${LUA_LIBRARIES}
)

engine_target_compile_options(engine_scripting)
