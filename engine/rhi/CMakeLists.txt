add_library(engine_rhi STATIC
    include/engine/rhi/rhi_types.h
    include/engine/rhi/rhi_context.h
    include/engine/rhi/rhi_swapchain.h
    include/engine/rhi/rhi_command_buffer.h
    include/engine/rhi/rhi_sync.h
    include/engine/rhi/rhi_pipeline.h
    include/engine/rhi/rhi_buffer.h
    include/engine/rhi/rhi_texture.h
    include/engine/rhi/rhi_bindless.h
    include/engine/rhi/viewport_presenter.h
    include/engine/rhi/window_swapchain_presenter.h
    include/engine/rhi/headless_presenter.h
    src/rhi_context.cpp
    src/rhi_swapchain.cpp
    src/rhi_command_buffer.cpp
    src/rhi_sync.cpp
    src/rhi_pipeline.cpp
    src/rhi_buffer.cpp
    src/rhi_texture.cpp
    src/rhi_bindless.cpp
    src/window_swapchain_presenter.cpp
    src/headless_presenter.cpp
)

target_include_directories(engine_rhi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

target_link_libraries(engine_rhi PUBLIC
    engine_core
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    SDL3::SDL3
)

engine_target_compile_options(engine_rhi)
