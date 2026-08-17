add_library(engine_vfs STATIC
    include/engine/vfs/vfs.h
    include/engine/vfs/mount_point.h
    include/engine/vfs/pak_archive.h
    src/vfs.cpp
    src/mount_point.cpp
    src/pak_archive.cpp
)

target_include_directories(engine_vfs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_vfs PUBLIC
    engine_core
    engine_jobs
)

engine_target_compile_options(engine_vfs)
