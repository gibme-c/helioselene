cmake_minimum_required(VERSION 3.10)

if(DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Choose the type of build, options are: Debug, Release, RelWithDebInfo")
else()
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: Debug, Release, RelWithDebInfo")
endif()

message(STATUS "Mode: ${CMAKE_BUILD_TYPE}")

set(CMAKE_CONFIGURATION_TYPES Debug RelWithDebInfo Release CACHE STRING INTERNAL)

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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Wextra -Wuninitialized -fstack-protector-strong")

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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

option(FORCE_REF10 "Force ref10 (32-bit) implementation even on 64-bit platforms" OFF)

set(HELIOSELENE_SRC
    src/helioselene_secure_erase.cpp
)

if(NOT FORCE_REF10 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^([xX]86_64|[aA][mM][dD]64|[aA][rR][mM]64|[aA][aA][rR][cC][hH]64)$")
    set(HELIOSELENE_X64_SRC
        src/x64/fp_frombytes.cpp
        src/x64/fp_invert.cpp
        src/x64/fp_mul.cpp
        src/x64/fp_pow22523.cpp
        src/x64/fp_sq.cpp
        src/x64/fp_sq2.cpp
        src/x64/fp_sqn.cpp
        src/x64/fp_tobytes.cpp
        src/x64/fq_frombytes.cpp
        src/x64/fq_invert.cpp
        src/x64/fq_mul.cpp
        src/x64/fq_sq.cpp
        src/x64/fq_sq2.cpp
        src/x64/fq_sqn.cpp
        src/x64/fq_sqrt.cpp
        src/x64/fq_tobytes.cpp
        src/x64/fp_sqrt.cpp
        src/x64/helios_dbl.cpp
        src/x64/helios_madd.cpp
        src/x64/helios_add.cpp
        src/x64/helios_tobytes.cpp
        src/x64/helios_frombytes.cpp
        src/x64/helios_scalarmult.cpp
        src/x64/helios_scalarmult_vartime.cpp
        src/x64/selene_dbl.cpp
        src/x64/selene_madd.cpp
        src/x64/selene_add.cpp
        src/x64/selene_tobytes.cpp
        src/x64/selene_frombytes.cpp
        src/x64/selene_scalarmult.cpp
        src/x64/selene_scalarmult_vartime.cpp
    )
    list(APPEND HELIOSELENE_SRC ${HELIOSELENE_X64_SRC})
    message(STATUS "HELIOSELENE: 64-bit optimized field arithmetic enabled (radix-2^51)")
else()
    message(STATUS "HELIOSELENE: No 64-bit backend available (ref10 not yet implemented)")
endif()

if(FORCE_REF10)
    message(STATUS "HELIOSELENE: Forced ref10 (32-bit) implementation via FORCE_REF10")
endif()

add_library(helioselene STATIC ${HELIOSELENE_SRC})
target_include_directories(helioselene PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(FORCE_REF10)
    target_compile_definitions(helioselene PUBLIC HELIOSELENE_FORCE_REF10=1)
endif()

option(BUILD_TESTS "Build unit tests and benchmarks" OFF)
if(DEFINED ENV{BUILD_TESTS})
    set(BUILD_TESTS $ENV{BUILD_TESTS})
    message(STATUS "HELIOSELENE: Activating unit tests and benchmarks")
endif()

if(BUILD_TESTS)
    enable_testing()

    add_executable(test-helioselene src/tests.cpp)
    target_link_libraries(test-helioselene helioselene)
    set_property(TARGET test-helioselene PROPERTY OUTPUT_NAME "helioselene-tests")
    add_test(NAME helioselene-tests COMMAND test-helioselene)

    add_executable(benchmark-helioselene src/benchmark.cpp)
    target_link_libraries(benchmark-helioselene helioselene)
    set_property(TARGET benchmark-helioselene PROPERTY OUTPUT_NAME "helioselene-benchmark")
endif()
