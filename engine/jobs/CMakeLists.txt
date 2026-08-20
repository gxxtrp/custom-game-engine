add_library(engine_jobs STATIC
    include/engine/jobs/job_system.h
    src/job_system.cpp
)

target_include_directories(engine_jobs PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_jobs PUBLIC engine_core)

engine_target_compile_options(engine_jobs)
