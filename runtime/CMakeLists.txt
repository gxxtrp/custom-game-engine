add_executable(runtime
    src/main.cpp
    src/runtime_app.cpp
)

target_include_directories(runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(runtime PRIVATE
    engine_master
)

engine_target_compile_options(runtime)
