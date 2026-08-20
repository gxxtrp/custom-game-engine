#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>

// Platform Detection
#if defined(_WIN32) || defined(_WIN64)
    #ifndef ENGINE_PLATFORM_WINDOWS
        #define ENGINE_PLATFORM_WINDOWS 1
    #endif
#elif defined(__APPLE__)
    #ifndef ENGINE_PLATFORM_MACOS
        #define ENGINE_PLATFORM_MACOS 1
    #endif
#elif defined(__linux__)
    #ifndef ENGINE_PLATFORM_LINUX
        #define ENGINE_PLATFORM_LINUX 1
    #endif
#endif

// Compiler Detection
#if defined(__clang__)
    #define ENGINE_COMPILER_CLANG 1
#elif defined(_MSC_VER)
    #define ENGINE_COMPILER_MSVC 1
#elif defined(__GNUC__)
    #define ENGINE_COMPILER_GCC 1
#endif

// Debug Break & Assertions
#if defined(ENGINE_PLATFORM_WINDOWS)
    #define ENGINE_DEBUG_BREAK() __debugbreak()
#else
    #define ENGINE_DEBUG_BREAK() __builtin_trap()
#endif

#if defined(NDEBUG)
    #define ENGINE_ASSERT(condition, message) ((void)0)
#else
    #define ENGINE_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                ::engine::core::internal_assert_failed(#condition, message, __FILE__, __LINE__); \
                ENGINE_DEBUG_BREAK(); \
            } \
        } while (0)
#endif

#define ENGINE_FORCE_INLINE inline __attribute__((always_inline))
#if defined(ENGINE_COMPILER_MSVC) && !defined(ENGINE_COMPILER_CLANG)
    #undef ENGINE_FORCE_INLINE
    #define ENGINE_FORCE_INLINE __forceinline
#endif

#define ENGINE_NODISCARD [[nodiscard]]

namespace engine::core {
    void internal_assert_failed(const char* expr, const char* msg, const char* file, uint32_t line);
}
