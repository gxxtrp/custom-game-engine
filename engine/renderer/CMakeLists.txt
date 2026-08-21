add_library(engine_renderer STATIC
    include/engine/renderer/render_graph_types.h
    include/engine/renderer/render_pass.h
    include/engine/renderer/render_graph.h
    include/engine/renderer/mesh.h
    include/engine/renderer/material.h
    include/engine/renderer/gpu_scene.h
    include/engine/renderer/lighting.h
    include/engine/renderer/shadow_map.h
    include/engine/renderer/volumetrics.h
    include/engine/renderer/oit.h
    include/engine/renderer/restir.h
    include/engine/renderer/post_process.h
    include/engine/renderer/taa.h
    include/engine/renderer/bloom.h
    include/engine/renderer/auto_exposure.h
    include/engine/renderer/scene_renderer.h
    src/render_pass.cpp
    src/render_graph.cpp
    src/mesh.cpp
    src/material.cpp
    src/gpu_scene.cpp
    src/lighting.cpp
    src/shadow_map.cpp
    src/volumetrics.cpp
    src/oit.cpp
    src/restir.cpp
    src/post_process.cpp
    src/taa.cpp
    src/bloom.cpp
    src/auto_exposure.cpp
    src/scene_renderer.cpp
)

target_include_directories(engine_renderer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(flecs CONFIG REQUIRED)

target_link_libraries(engine_renderer PUBLIC
    engine_core
    engine_rhi
    engine_importer
    engine_assets
    engine_scene
    flecs::flecs_static
)

engine_target_compile_options(engine_renderer)

find_program(DXC_EXECUTABLE dxc)
if(DXC_EXECUTABLE)
    message(STATUS "Found Microsoft DXC: ${DXC_EXECUTABLE}")
    
    set(HLSL_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shaders/hlsl")
    set(SPV_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shaders")

    set(HLSL_SHADERS
        "Mesh.hlsl:VSMain:vs_6_6:mesh_vert.spv:MESH_VERT_SPV"
        "Mesh.hlsl:PSMain:ps_6_6:mesh_frag.spv:MESH_FRAG_SPV"
        "Shadow.hlsl:VSMain:vs_6_6:shadow_vert.spv:SHADOW_VERT_SPV"
        "Shadow.hlsl:PSMain:ps_6_6:shadow_frag.spv:SHADOW_FRAG_SPV"
        "Tonemap.hlsl:VSMain:vs_6_6:tonemap_vert.spv:TONEMAP_VERT_SPV"
        "Tonemap.hlsl:PSMain:ps_6_6:tonemap_frag.spv:TONEMAP_FRAG_SPV"
        "Triangle.hlsl:VSMain:vs_6_6:triangle_vert.spv:TRIANGLE_VERT_SPV"
        "Triangle.hlsl:PSMain:ps_6_6:triangle_frag.spv:TRIANGLE_FRAG_SPV"
    )

    set(GENERATED_SPV_HEADERS "")
    foreach(SHADER_SPEC IN LISTS HLSL_SHADERS)
        string(REPLACE ":" ";" SPEC_LIST ${SHADER_SPEC})
        list(GET SPEC_LIST 0 S_FILE)
        list(GET SPEC_LIST 1 S_ENTRY)
        list(GET SPEC_LIST 2 S_STAGE)
        list(GET SPEC_LIST 3 S_OUT)
        list(GET SPEC_LIST 4 S_SYM)

        set(IN_PATH "${HLSL_SHADER_DIR}/${S_FILE}")
        set(OUT_PATH "${SPV_OUT_DIR}/${S_OUT}")

        add_custom_command(
            OUTPUT "${OUT_PATH}"
            COMMAND ${DXC_EXECUTABLE} -spirv -T ${S_STAGE} -E ${S_ENTRY} "-fspv-target-env=vulkan1.3" -fspv-entrypoint-name=main -HV 2021 "${IN_PATH}" -Fh "${OUT_PATH}" -Vn ${S_SYM}
            DEPENDS "${IN_PATH}"
            COMMENT "Compiling HLSL shader ${S_FILE} (${S_ENTRY}:${S_STAGE}) -> ${S_OUT}"
            VERBATIM
        )
        list(APPEND GENERATED_SPV_HEADERS "${OUT_PATH}")
    endforeach()

    add_custom_target(compile_hlsl_shaders DEPENDS ${GENERATED_SPV_HEADERS})
    add_dependencies(engine_renderer compile_hlsl_shaders)
endif()
