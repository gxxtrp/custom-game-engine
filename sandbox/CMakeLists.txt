add_executable(sandbox
    src/main.cpp
)

target_link_libraries(sandbox PRIVATE
    engine_master
)

engine_target_compile_options(sandbox)
