# Compiler Flags & C++ standard configuration

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(engine_target_compile_options TARGET_NAME)
    if(MSVC)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${TARGET_NAME} PRIVATE
                /W4
                /utf-8
                /permissive-
                /EHsc
                /arch:AVX2
                -Wno-unused-parameter
                -Wno-missing-field-initializers
                $<$<CONFIG:Debug>:/Od /Zi>
                $<$<CONFIG:Release>:/O2 /DNDEBUG>
            )
        else()
            target_compile_options(${TARGET_NAME} PRIVATE
                /W4
                /utf-8
                /permissive-
                /EHsc
                /Zc:preprocessor
                /arch:AVX2
                $<$<CONFIG:Debug>:/Od /Zi>
                $<$<CONFIG:Release>:/O2 /DNDEBUG>
            )
        endif()
    else()
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -mavx2
            -mfma
            $<$<CONFIG:Debug>:-O0 -g>
            $<$<CONFIG:Release>:-O3 -DNDEBUG>
        )
    endif()
endfunction()
