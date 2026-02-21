cmake_minimum_required(VERSION 3.10)

if(DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Choose the type of build, options are: Debug, Release, RelWithDebInfo")
else()
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: Debug, Release, RelWithDebInfo")
endif()

message(STATUS "Mode: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo Release CACHE STRING INTERNAL)
set(ARCH native CACHE STRING  "CPU to build for: -march value or native")

message(STATUS "Building for target architecture: ${ARCH}")

set(LIB_MAJOR_VERSION "0")
set(LIB_MINOR_VERSION "1")
set(LIB_PATCH_VERSION "0")
set(LIB_VERSION_STRING "${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION}.${LIB_PATCH_VERSION}")

cmake_policy(SET CMP0048 NEW)
project(helioselene VERSION "${LIB_VERSION_STRING}" LANGUAGES CXX)

if(NOT MSVC)
    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        message(STATUS "Found ccache package... Activating...")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    endif()
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_SKIP_INSTALL_RULES OFF FORCE)
set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY ON FORCE)
set(CMAKE_SUPPRESS_REGENERATION ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(STATIC ON CACHE BOOL FORCE "Link libraries statically? Forced to ON")
add_definitions(-DSTATICLIB)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std:c++17 /W4 /GS /guard:cf /DWIN32_LEAN_AND_MEAN /wd4267 /wd4804 /D_DLL")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
    string(REPLACE "/Ob0" "/Ob2" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Oi")
else()
    if("${ARCH}" STREQUAL "default")
        set(ARCH_FLAG "")
    else()
        set(ARCH_FLAG "-march=${ARCH}")
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Wextra -Wuninitialized -fstack-protector-strong ${ARCH_FLAG}")

    # Disable auto-vectorization to prevent the compiler from introducing
    # SIMD instructions that could create data-dependent timing behavior,
    # which would undermine the constant-time guarantees of the crypto code.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-tree-vectorize")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-vectorize -fno-slp-vectorize")
    endif()

    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -Og")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -O3")

    if(MINGW OR STATIC_LIBC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libstdc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    endif()

    if(NOT APPLE)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif()
endif()

# ── Options ──
option(FORCE_PORTABLE "Force portable (32-bit) implementation even on 64-bit platforms" OFF)

# Detect x86_64 specifically (SIMD backends are x86_64-only)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^([xX]86_64|[aA][mM][dD]64)$")
    set(HELIOSELENE_IS_X86_64 TRUE)
else()
    set(HELIOSELENE_IS_X86_64 FALSE)
endif()

# SIMD options: default ON for x86_64, OFF otherwise
if(HELIOSELENE_IS_X86_64 AND NOT FORCE_PORTABLE)
    option(ENABLE_AVX512 "Enable AVX-512 IFMA backend" ON)
    option(ENABLE_AVX2 "Enable AVX2 backend" ON)
else()
    option(ENABLE_AVX512 "Enable AVX-512 IFMA backend" OFF)
    option(ENABLE_AVX2 "Enable AVX2 backend" OFF)
endif()

# Force SIMD off when portable is active
if(FORCE_PORTABLE)
    set(ENABLE_AVX512 OFF CACHE BOOL "" FORCE)
    set(ENABLE_AVX2 OFF CACHE BOOL "" FORCE)
    message(STATUS "HELIOSELENE: Forced portable (32-bit) implementation via FORCE_PORTABLE")
endif()

option(ENABLE_ECFFT "Enable ECFFT polynomial multiplication" OFF)

option(ENABLE_LTO "Enable link-time optimization" OFF)
if(ENABLE_LTO)
    if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GL")
        set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /LTCG")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
    endif()
    message(STATUS "HELIOSELENE: Link-time optimization enabled")
endif()

# ── Sub-libraries ──
add_subdirectory(common)
add_subdirectory(fp)
add_subdirectory(fq)
add_subdirectory(helios)
add_subdirectory(selene)
add_subdirectory(ecfft)
add_subdirectory(ec-divisors)

# ── Main library: dispatch + master include ──
if(ENABLE_AVX2 OR ENABLE_AVX512)
    # SIMD dispatch requires compiled sources
    add_library(helioselene STATIC
        src/cpuid.cpp
        src/dispatch.cpp
    )
    message(STATUS "HELIOSELENE: SIMD dispatch enabled")

    if(ENABLE_AVX2)
        message(STATUS "HELIOSELENE: AVX2 backend enabled")
    endif()
    if(ENABLE_AVX512)
        message(STATUS "HELIOSELENE: AVX-512 IFMA backend enabled")
    endif()

    target_compile_definitions(helioselene PUBLIC HELIOSELENE_SIMD=1)

    if(NOT ENABLE_AVX512)
        target_compile_definitions(helioselene PUBLIC HELIOSELENE_NO_AVX512=1)
    endif()
    if(NOT ENABLE_AVX2)
        target_compile_definitions(helioselene PUBLIC HELIOSELENE_NO_AVX2=1)
    endif()
else()
    # No compiled sources needed — header-only umbrella
    add_library(helioselene INTERFACE)
endif()

if(ENABLE_ECFFT)
    message(STATUS "HELIOSELENE: ECFFT polynomial multiplication enabled")
endif()

# Use generator expressions for PUBLIC/INTERFACE compatibility
if(ENABLE_AVX2 OR ENABLE_AVX512)
    target_include_directories(helioselene PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(helioselene PUBLIC
        helioselene-ec-divisors
        helioselene-helios
        helioselene-selene
        helioselene-fp
        helioselene-fq
        helioselene-ecfft
        helioselene-common
    )
else()
    target_include_directories(helioselene INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(helioselene INTERFACE
        helioselene-ec-divisors
        helioselene-helios
        helioselene-selene
        helioselene-fp
        helioselene-fq
        helioselene-ecfft
        helioselene-common
    )
endif()

# ── Tests and benchmarks ──
option(BUILD_TESTS "Build unit tests and benchmarks" OFF)
if(DEFINED ENV{BUILD_TESTS})
    set(BUILD_TESTS $ENV{BUILD_TESTS})
    message(STATUS "HELIOSELENE: Activating unit tests and benchmarks")
endif()

if(BUILD_TESTS)
    enable_testing()

    add_executable(test-helioselene src/tests.cpp)
    target_link_libraries(test-helioselene PRIVATE helioselene)
    set_property(TARGET test-helioselene PROPERTY OUTPUT_NAME "helioselene-tests")
    add_test(NAME helioselene-tests COMMAND test-helioselene)

    add_executable(benchmark-helioselene src/benchmark.cpp)
    target_link_libraries(benchmark-helioselene PRIVATE helioselene)
    set_property(TARGET benchmark-helioselene PROPERTY OUTPUT_NAME "helioselene-benchmark")

    add_executable(benchmark-contest src/benchmark_contest.cpp)
    target_link_libraries(benchmark-contest PRIVATE helioselene)
    set_property(TARGET benchmark-contest PROPERTY OUTPUT_NAME "helioselene-benchmark-contest")

    add_executable(benchmark-fcmpp src/benchmark_fcmpp.cpp)
    target_link_libraries(benchmark-fcmpp PRIVATE helioselene)
    set_property(TARGET benchmark-fcmpp PROPERTY OUTPUT_NAME "helioselene-benchmark-fcmpp")
endif()

# ── Auxiliary tools ──
option(BUILD_TOOLS "Build auxiliary tools (ECFFT curve search)" OFF)
if(BUILD_TOOLS)
    add_executable(find-ecfft-curve tools/find_ecfft_curve.cpp)
    target_link_libraries(find-ecfft-curve helioselene)
    set_property(TARGET find-ecfft-curve PROPERTY OUTPUT_NAME "helioselene-find-ecfft")

    add_executable(gen-ecfft-data tools/gen_ecfft_data.cpp)
    target_link_libraries(gen-ecfft-data helioselene)
    set_property(TARGET gen-ecfft-data PROPERTY OUTPUT_NAME "helioselene-gen-ecfft")

    message(STATUS "HELIOSELENE: Auxiliary tools enabled")
endif()
