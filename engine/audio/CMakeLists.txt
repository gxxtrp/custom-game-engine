add_library(engine_audio STATIC
    include/engine/audio/audio_types.h
    include/engine/audio/audio_components.h
    include/engine/audio/audio_engine.h
    src/audio_engine.cpp
)

target_include_directories(engine_audio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")
target_include_directories(engine_audio PRIVATE ${MINIAUDIO_INCLUDE_DIRS})

target_link_libraries(engine_audio PUBLIC
    engine_core
    engine_scene
)

engine_target_compile_options(engine_audio)
