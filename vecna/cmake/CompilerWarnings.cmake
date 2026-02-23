# CompilerWarnings.cmake
# Configures strict compiler warnings for the project

function(set_project_warnings target_name)
    set(MSVC_WARNINGS
        /W4         # High warning level
        /WX         # Warnings as errors
        /permissive-  # Strict conformance mode
        /w14242     # Possible loss of data (conversion)
        /w14254     # Operator conversion
        /w14263     # Member function does not override
        /w14265     # Class has virtual functions but destructor is not virtual
        /w14287     # Unsigned/negative constant mismatch
        /we4289     # Loop control variable used outside loop
        /w14296     # Expression is always true/false
        /w14311     # Pointer truncation
        /w14545     # Expression before comma evaluates to function
        /w14546     # Function call before comma missing argument list
        /w14547     # Operator before comma has no effect
        /w14549     # Operator before comma has no effect
        /w14555     # Expression has no effect
        /w14619     # Pragma warning: unknown warning number
        /w14640     # Thread-unsafe static member initialization
        /w14826     # Conversion is sign-extended
        /w14905     # Wide string literal cast to LPSTR
        /w14906     # String literal cast to LPWSTR
        /w14928     # Illegal copy-initialization
    )

    set(CLANG_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        # Disable some warnings that are too noisy
        -Wno-unused-parameter
    )

    set(GCC_WARNINGS
        ${CLANG_WARNINGS}
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
    )

    if(MSVC)
        set(PROJECT_WARNINGS ${MSVC_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        set(PROJECT_WARNINGS ${CLANG_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(PROJECT_WARNINGS ${GCC_WARNINGS})
    else()
        message(AUTHOR_WARNING "No compiler warnings set for '${CMAKE_CXX_COMPILER_ID}' compiler.")
    endif()

    target_compile_options(${target_name} PRIVATE ${PROJECT_WARNINGS})

    # Debug-specific options
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /Zi /Od)
        else()
            target_compile_options(${target_name} PRIVATE -g -O0)
        endif()
    endif()

    # Release-specific options
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /O2)
        else()
            target_compile_options(${target_name} PRIVATE -O3)
        endif()
    endif()
endfunction()
