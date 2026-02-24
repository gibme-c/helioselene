cmake_minimum_required(VERSION 3.10)

if(DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Choose the type of build, options are: Debug, Release, RelWithDebInfo")
else()
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: Debug, Release, RelWithDebInfo")
endif()

message(STATUS "Mode: ${CMAKE_BUILD_TYPE}")

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
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
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

if(MSVC)
    include(CheckCXXCompilerFlag)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std:c++17 /W4 /GS /sdl /guard:cf /wd4267 /wd4804 /D_DLL")
    string(REPLACE "/Ob0" "/Ob2" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Oi")

    # Spectre mitigation
    check_cxx_compiler_flag("/Qspectre" HAS_QSPECTRE)
    if(HAS_QSPECTRE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Qspectre")
    endif()

    # PE linker hardening — DEP + ASLR
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA")

    # CET shadow stack (newer MSVC only)
    check_cxx_compiler_flag("/guard:ehcont" HAS_GUARD_EHCONT)
    if(HAS_GUARD_EHCONT)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /guard:ehcont")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /guard:ehcont /CETCOMPAT")
    endif()
else()
    if("${ARCH}" STREQUAL "default")
        set(ARCH_FLAG "")
    else()
        set(ARCH_FLAG "-march=${ARCH}")
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Wextra -Wuninitialized -Wformat-security -Wconversion -Wsign-conversion ${ARCH_FLAG}")

    # Stack protector: try-compile probe (catches any toolchain where
    # libssp / compiler-rt is missing, not just Clang+MinGW).
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "-fstack-protector-strong")
    check_cxx_source_compiles("int main() { char buf[64]; buf[0] = 0; return buf[0]; }" HAS_STACK_PROTECTOR_STRONG)
    unset(CMAKE_REQUIRED_FLAGS)
    if(HAS_STACK_PROTECTOR_STRONG)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-strong")
    endif()

    # Stack clash protection (GCC 8.3+, Clang 11+)
    # Exclude AppleClang and Windows Clang: they accept the flag at configure
    # time but emit "argument unused during compilation" warnings on every TU.
    include(CheckCXXCompilerFlag)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND (WIN32 OR APPLE)))
        check_cxx_compiler_flag("-fstack-clash-protection" HAS_STACK_CLASH_PROTECTION)
        if(HAS_STACK_CLASH_PROTECTION)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-clash-protection")
        endif()
    endif()

    # Disable auto-vectorization to prevent the compiler from introducing
    # SIMD instructions that could create data-dependent timing behavior,
    # which would undermine the constant-time guarantees of the crypto code.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-tree-vectorize")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-vectorize -fno-slp-vectorize")
    endif()

    # CET control-flow integrity (Intel CET shadow stack + indirect branch tracking)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^([xX]86_64|[aA][mM][dD]64)$")
        check_cxx_compiler_flag("-fcf-protection=full" HAS_CF_PROTECTION)
        if(HAS_CF_PROTECTION)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcf-protection=full")
        endif()
    endif()

    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -Og")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -O3")
    if(NOT WIN32)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D_FORTIFY_SOURCE=2")
    endif()

    # GCC libstdc++ debug assertions (bounds checks on operator[], iterator validation)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_ASSERTIONS")
    endif()

    # ELF linker hardening (not applicable on Apple, Windows, or MSVC)
    if(NOT APPLE AND NOT WIN32)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack")
    endif()

    # MinGW (GCC) PE linker hardening — DEP + ASLR
    if(MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--nxcompat -Wl,--dynamicbase -Wl,--high-entropy-va")
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

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
if(NOT MSVC)
    if(ENABLE_ASAN)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        message(STATUS "HELIOSELENE: AddressSanitizer enabled")
    endif()
    if(ENABLE_UBSAN)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
        message(STATUS "HELIOSELENE: UndefinedBehaviorSanitizer enabled")
    endif()
endif()

# ── Sub-libraries ──
add_subdirectory(common)
add_subdirectory(fp)
add_subdirectory(fq)
add_subdirectory(helios)
add_subdirectory(selene)
add_subdirectory(ecfft)
add_subdirectory(ec-divisors)

# ── Main library: dispatch + master include + public C++ API ──
set(HELIOSELENE_API_SOURCES
    src/api_scalar.cpp
    src/api_point.cpp
    src/api_polynomial.cpp
    src/api_divisor.cpp
)

if(ENABLE_AVX2 OR ENABLE_AVX512)
    add_library(helioselene STATIC
        src/cpuid.cpp
        src/dispatch.cpp
        ${HELIOSELENE_API_SOURCES}
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
    add_library(helioselene STATIC ${HELIOSELENE_API_SOURCES})
endif()

if(ENABLE_ECFFT)
    message(STATUS "HELIOSELENE: ECFFT polynomial multiplication enabled")
endif()

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

# ── Tests ──
option(BUILD_TESTS "Build unit tests" OFF)
if(DEFINED ENV{BUILD_TESTS})
    set(BUILD_TESTS $ENV{BUILD_TESTS})
endif()

if(BUILD_TESTS)
    enable_testing()
    message(STATUS "HELIOSELENE: Unit tests enabled")

    add_executable(helioselene-tests src/tests.cpp)
    target_link_libraries(helioselene-tests PRIVATE helioselene)
    add_test(NAME helioselene-tests COMMAND helioselene-tests)

    add_executable(helioselene-fuzz-tests src/tests_fuzz.cpp)
    target_link_libraries(helioselene-fuzz-tests PRIVATE helioselene)
    add_test(NAME helioselene-fuzz-tests COMMAND helioselene-fuzz-tests)
endif()

# ── Benchmarks ──
option(BUILD_BENCHMARKS "Build benchmark executables" OFF)
if(DEFINED ENV{BUILD_BENCHMARKS})
    set(BUILD_BENCHMARKS $ENV{BUILD_BENCHMARKS})
endif()

if(BUILD_BENCHMARKS)
    message(STATUS "HELIOSELENE: Benchmarks enabled")

    add_executable(helioselene-benchmark src/benchmark.cpp)
    target_link_libraries(helioselene-benchmark PRIVATE helioselene)

    add_executable(helioselene-benchmark-contest src/benchmark_contest.cpp)
    target_link_libraries(helioselene-benchmark-contest PRIVATE helioselene)

    add_executable(helioselene-benchmark-fcmpp src/benchmark_fcmpp.cpp)
    target_link_libraries(helioselene-benchmark-fcmpp PRIVATE helioselene)

    # Benchmarks are developer tools — relax sign-conversion warnings
    if(NOT MSVC)
        foreach(_bench helioselene-benchmark helioselene-benchmark-contest helioselene-benchmark-fcmpp)
            target_compile_options(${_bench} PRIVATE -Wno-sign-conversion)
        endforeach()
    endif()
endif()

# ── Auxiliary tools ──
option(BUILD_TOOLS "Build auxiliary tools (ECFFT curve search)" OFF)
if(BUILD_TOOLS)
    add_executable(helioselene-find-ecfft tools/find_ecfft_curve.cpp)
    target_link_libraries(helioselene-find-ecfft helioselene)

    add_executable(helioselene-gen-ecfft tools/gen_ecfft_data.cpp)
    target_link_libraries(helioselene-gen-ecfft helioselene)

    add_executable(helioselene-gen-testvectors tools/gen_test_vectors.cpp)
    target_link_libraries(helioselene-gen-testvectors helioselene)

    # Tools are developer utilities — relax sign-conversion warnings
    if(NOT MSVC)
        foreach(_tool helioselene-find-ecfft helioselene-gen-ecfft helioselene-gen-testvectors)
            target_compile_options(${_tool} PRIVATE -Wno-sign-conversion)
        endforeach()
    endif()

    message(STATUS "HELIOSELENE: Auxiliary tools enabled")
endif()
