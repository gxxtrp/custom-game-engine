add_library(engine_events STATIC
    include/engine/events/event_bus.h
    src/event_bus.cpp
)

target_include_directories(engine_events PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(engine_events PUBLIC engine_core)

engine_target_compile_options(engine_events)
