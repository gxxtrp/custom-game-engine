# Platform definitions and macros

if(WIN32)
    add_compile_definitions(ENGINE_PLATFORM_WINDOWS=1 NOMINMAX WIN32_LEAN_AND_MEAN)
elseif(APPLE)
    add_compile_definitions(ENGINE_PLATFORM_MACOS=1)
elseif(UNIX)
    add_compile_definitions(ENGINE_PLATFORM_LINUX=1)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_definitions(ENGINE_ARCH_X64=1)
else()
    add_compile_definitions(ENGINE_ARCH_X86=1)
endif()
