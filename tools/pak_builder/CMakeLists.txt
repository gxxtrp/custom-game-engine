add_executable(pak_builder
    src/main.cpp
)

target_link_libraries(pak_builder PRIVATE
    engine_core
    engine_vfs
)

engine_target_compile_options(pak_builder)
