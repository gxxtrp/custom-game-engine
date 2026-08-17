add_executable(sandbox
    src/main.cpp
)

target_link_libraries(sandbox PRIVATE
    engine_core
)

engine_target_compile_options(sandbox)
