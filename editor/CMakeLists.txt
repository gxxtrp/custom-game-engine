add_executable(editor
    src/main.cpp
    src/editor_app.cpp
)

target_include_directories(editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)

target_link_libraries(editor PRIVATE
    engine_master
    imgui::imgui
    imguizmo::imguizmo
)

engine_target_compile_options(editor)
