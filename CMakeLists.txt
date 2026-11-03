cmake_minimum_required(VERSION 3.25)
project(GodXplr
    VERSION 0.1.0
    DESCRIPTION "High-Performance Windows File Manager"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Build Options
# ------------------------------------------------------------------------------
option(BUILD_TESTING "Build the test suite" ON)
option(BUILD_HEADLESS_ONLY "Build only headless core and services (disable Qt requirement)" OFF)
option(ENABLE_STRICT_WARNINGS "Treat warnings as errors" ON)

# ------------------------------------------------------------------------------
# Compiler Warnings & Hardening
# ------------------------------------------------------------------------------
if(MSVC)
    add_compile_options(
        /W4
        /permissive-
        /Zc:inline
        /Zc:preprocessor
        /utf-8
        /volatile:iso
        /EHsc
    )
    if(ENABLE_STRICT_WARNINGS)
        add_compile_options(/WX)
    endif()
    # Enable AVX2 instruction set for SIMD search scan
    add_compile_options(/arch:AVX2)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -mavx2
    )
    if(ENABLE_STRICT_WARNINGS)
        add_compile_options(-Werror)
    endif()
endif()

# ------------------------------------------------------------------------------
# Qt 6 Detection (Optional / Feature Gated)
# ------------------------------------------------------------------------------
if(NOT BUILD_HEADLESS_ONLY)
    find_package(Qt6 6.5 QUIET COMPONENTS Core Gui Widgets Test)
    if(Qt6_FOUND)
        message(STATUS "Qt 6 found (${Qt6_VERSION}). UI targets enabled.")
        set(GODXPLR_HAVE_QT ON)
    else()
        message(STATUS "Qt 6 not detected. Building headless core, platform, index service, and tests.")
        set(GODXPLR_HAVE_QT OFF)
    endif()
else()
    message(STATUS "BUILD_HEADLESS_ONLY is ON. Skipping Qt 6 detection.")
    set(GODXPLR_HAVE_QT OFF)
endif()

# ------------------------------------------------------------------------------
# Subdirectories
# ------------------------------------------------------------------------------
add_subdirectory(src/core)

if(WIN32)
    add_subdirectory(src/platform/windows)
    add_subdirectory(src/index)
endif()

if(GODXPLR_HAVE_QT)
    add_subdirectory(src/ui)
    add_subdirectory(src/app)
endif()

add_subdirectory(tools)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

