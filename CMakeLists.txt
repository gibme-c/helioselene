cmake_minimum_required(VERSION 3.10)

if(DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Choose the type of build, options are: Debug, Release, RelWithDebInfo")
else()
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: Debug, Release, RelWithDebInfo")
endif()

message(STATUS "Mode: ${CMAKE_BUILD_TYPE}")

set(LIB_MAJOR_VERSION "0")
set(LIB_MINOR_VERSION "1")
set(LIB_PATCH_VERSION "0")
set(LIB_VERSION_STRING "${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION}.${LIB_PATCH_VERSION}")

cmake_policy(SET CMP0048 NEW)
project(ranshaw VERSION "${LIB_VERSION_STRING}" LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_SKIP_INSTALL_RULES OFF FORCE)
set(CMAKE_SKIP_PACKAGE_ALL_DEPENDENCY ON FORCE)
set(CMAKE_SUPPRESS_REGENERATION ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Test / sanitizer options (declared early so build-flag setup can see them) ──
# BUILD_TESTS is also settable via environment variable for CI convenience.
option(BUILD_TESTS "Build unit tests" OFF)
option(ENABLE_TIMING_TESTS "Build the statistical CT-timing harness (nightly/advisory)" OFF)
if(DEFINED ENV{BUILD_TESTS})
    set(BUILD_TESTS $ENV{BUILD_TESTS})
endif()

# RANSHAW_TEST_SANITIZERS: comma-separated sanitizer list for TEST binaries only.
# Valid values (csv): address, undefined, thread. Examples:
#   -DRANSHAW_TEST_SANITIZERS=address,undefined
#   -DRANSHAW_TEST_SANITIZERS=thread
# Requires BUILD_TESTS=ON. Rejected on MinGW. MSVC only supports "address".
set(RANSHAW_TEST_SANITIZERS "" CACHE STRING
    "CSV sanitizer list for test binaries (address,undefined,thread); requires BUILD_TESTS=ON")

set(_ranshaw_sanitizers "")
set(_ranshaw_sanitized FALSE)
if(RANSHAW_TEST_SANITIZERS)
    if(NOT BUILD_TESTS)
        message(FATAL_ERROR
            "RANSHAW_TEST_SANITIZERS=${RANSHAW_TEST_SANITIZERS} requires BUILD_TESTS=ON")
    endif()
    if(MINGW)
        message(FATAL_ERROR "RANSHAW_TEST_SANITIZERS is not supported on MinGW")
    endif()
    string(REPLACE "," ";" _ranshaw_sanitizers "${RANSHAW_TEST_SANITIZERS}")
    foreach(_san IN LISTS _ranshaw_sanitizers)
        if(NOT _san MATCHES "^(address|undefined|thread)$")
            message(FATAL_ERROR
                "RANSHAW_TEST_SANITIZERS: unknown sanitizer '${_san}' (expected address,undefined,thread)")
        endif()
    endforeach()
    if(MSVC)
        if(NOT _ranshaw_sanitizers STREQUAL "address")
            message(FATAL_ERROR
                "MSVC only supports RANSHAW_TEST_SANITIZERS=address (got: ${RANSHAW_TEST_SANITIZERS})")
        endif()
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            message(FATAL_ERROR
                "MSVC ASan is incompatible with /RTC* in Debug builds — use Release or RelWithDebInfo")
        endif()
    endif()
    set(_ranshaw_sanitized TRUE)
    message(STATUS "RANSHAW: test binaries will be built with sanitizers: ${RANSHAW_TEST_SANITIZERS}")
endif()

# ── Internal build flags (PRIVATE to all ranshaw targets) ──
# Sub-libraries link PRIVATE to this target so compile flags do not leak
# to downstream consumers via add_subdirectory or FetchContent.
add_library(ranshaw-build-flags INTERFACE)
target_compile_features(ranshaw-build-flags INTERFACE cxx_std_17)

include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)

if(MSVC)
    # Replace CMake's default /W3 with /W4 to avoid D9025 override warning
    string(REPLACE "/W3" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    # Base MSVC warning level (always applied)
    target_compile_options(ranshaw-build-flags INTERFACE
        /W4 /wd4267 /wd4804 /D_DLL
        # Debug: enable inlining + intrinsics even in debug for usable perf
        $<$<CONFIG:Debug>:/Ob2 /Oi>
    )

    # MSVC hardening flags — DISABLED under ASan.
    # /GS stack cookies, /sdl extra security diagnostics, /guard:cf CFG, and
    # /guard:ehcont EH continuation metadata all conflict with ASan's
    # interceptor trampoline dispatch and produce silent
    # STATUS_STACK_BUFFER_OVERRUN (__fastfail) at startup.
    if(NOT _ranshaw_sanitized)
        target_compile_options(ranshaw-build-flags INTERFACE /GS /sdl /guard:cf)

        # CET shadow stack (newer MSVC only)
        check_cxx_compiler_flag("/guard:ehcont" HAS_GUARD_EHCONT)
        if(HAS_GUARD_EHCONT)
            target_compile_options(ranshaw-build-flags INTERFACE /guard:ehcont)
        endif()
    else()
        # Sanitizer build: affirmatively disable hardening so any inherited or
        # default flags do not override. "Last flag wins" on the command line.
        target_compile_options(ranshaw-build-flags INTERFACE
            /GS- /sdl- /guard:cf- /guard:ehcont-)
        set(HAS_GUARD_EHCONT FALSE)
    endif()

    # Spectre mitigation (opt-in; significantly increases MSVC compile time)
    option(ENABLE_SPECTRE_MITIGATIONS "Enable MSVC Spectre mitigations (/Qspectre)" OFF)
    if(ENABLE_SPECTRE_MITIGATIONS AND NOT _ranshaw_sanitized)
        check_cxx_compiler_flag("/Qspectre" HAS_QSPECTRE)
        if(HAS_QSPECTRE)
            target_compile_options(ranshaw-build-flags INTERFACE /Qspectre)
        endif()
    endif()

    # MSVC auto-vectorization defense-in-depth:
    #
    # Modern MSVC (VS 2019+) has no documented global flag equivalent to
    # GCC's -fno-tree-vectorize or Clang's -fno-vectorize / -fno-slp-vectorize.
    # The MSVC auto-vectorizer only engages inside loops whose iterations are
    # statically independent; the ranshaw CT code inserts `ct_barrier` calls
    # (compiler memory clobbers) around every conditional select and field op,
    # which breaks the independence analysis and prevents vectorization at
    # the loop-body level regardless of compiler flag state. Each CT scalarmult
    # translation unit also uses `#pragma loop(no_vector)` (via the portable
    # RANSHAW_NO_VECTOR macro from ranshaw_ct_barrier.h) on the main loop for
    # belt-and-braces, so the compiler cannot rewrite the constant-time sweep
    # into a data-dependent SIMD gather/scatter. This is defense-in-depth; the
    # primary CT guarantee still lives in ct_barrier.
else()
    target_compile_options(ranshaw-build-flags INTERFACE
        -Wall -Wextra -Wuninitialized -Wformat-security -Wconversion -Wsign-conversion
        # Debug: full debug info + optimize for debugging
        $<$<CONFIG:Debug>:-g3 -Og>
    )

    # _FORTIFY_SOURCE requires optimization and is incompatible with sanitizers
    # (conflicts with ASan/UBSan runtime interceptors). Only apply when not
    # building a sanitized test configuration.
    if(NOT WIN32 AND NOT _ranshaw_sanitized)
        target_compile_options(ranshaw-build-flags INTERFACE
            $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>
        )
    endif()

    # Stack protector: try-compile probe (catches any toolchain where
    # libssp / compiler-rt is missing, not just Clang+MinGW). Stack protection
    # is compatible with sanitizers; keep it on.
    set(CMAKE_REQUIRED_FLAGS "-fstack-protector-strong")
    check_cxx_source_compiles("int main() { char buf[64]; buf[0] = 0; return buf[0]; }" HAS_STACK_PROTECTOR_STRONG)
    unset(CMAKE_REQUIRED_FLAGS)
    if(HAS_STACK_PROTECTOR_STRONG)
        target_compile_options(ranshaw-build-flags INTERFACE -fstack-protector-strong)
    endif()

    # Stack clash protection (Linux only — not supported on macOS/Apple Clang,
    # and Windows Clang accepts the flag but emits "argument unused" warnings)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        check_cxx_compiler_flag("-fstack-clash-protection" HAS_STACK_CLASH_PROTECTION)
        if(HAS_STACK_CLASH_PROTECTION)
            target_compile_options(ranshaw-build-flags INTERFACE -fstack-clash-protection)
        endif()
    endif()

    # Disable auto-vectorization to prevent the compiler from introducing
    # SIMD instructions that could create data-dependent timing behavior,
    # which would undermine the constant-time guarantees of the crypto code.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(ranshaw-build-flags INTERFACE -fno-tree-vectorize)
        # GCC libstdc++ debug assertions (bounds checks on operator[], iterator validation)
        target_compile_options(ranshaw-build-flags INTERFACE
            $<$<CONFIG:Debug>:-D_GLIBCXX_ASSERTIONS>
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(ranshaw-build-flags INTERFACE -fno-vectorize -fno-slp-vectorize)
    endif()

    # Enable BMI2 (MULX) and ADX (ADCX/ADOX) for the x64 inline-asm field ops
    # in fq51_inline.h. These are gated on __BMI2__; without them the codebase
    # falls through to a 5×5 schoolbook fallback at fq51_inline.h:1715+ which
    # is measurably slower. BMI2 is universally available on Haswell+ (2013+)
    # and Excavator+ (2015+). Non-x86 targets and FORCE_PORTABLE skip this.
    # Sanitized builds still get the flags — the asm is CT-safe and the
    # sanitizers do not need to see different codegen.
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^([xX]86_64|[aA][mM][dD]64)$" AND NOT FORCE_PORTABLE)
        check_cxx_compiler_flag("-mbmi2" HAS_MBMI2)
        check_cxx_compiler_flag("-madx" HAS_MADX)
        if(HAS_MBMI2)
            target_compile_options(ranshaw-build-flags INTERFACE -mbmi2)
        endif()
        if(HAS_MADX)
            target_compile_options(ranshaw-build-flags INTERFACE -madx)
        endif()
    endif()

    # CET control-flow integrity (Intel CET shadow stack + indirect branch tracking)
    # Incompatible with sanitizer runtimes — skip when building sanitized tests.
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^([xX]86_64|[aA][mM][dD]64)$" AND NOT _ranshaw_sanitized)
        check_cxx_compiler_flag("-fcf-protection=full" HAS_CF_PROTECTION)
        if(HAS_CF_PROTECTION)
            target_compile_options(ranshaw-build-flags INTERFACE -fcf-protection=full)
        endif()
    endif()
endif()

# ── Sanitizer compile flags (test builds only) ──
# Applied INTERFACE on ranshaw-build-flags so every ranshaw sub-library and the
# main library are instrumented in a sanitized test build. The shipping library
# (built without RANSHAW_TEST_SANITIZERS) is unaffected — this code only runs
# when RANSHAW_TEST_SANITIZERS is set, which requires BUILD_TESTS=ON.
if(_ranshaw_sanitized)
    if(MSVC)
        # MSVC ASan: /Zi generates PDBs the runtime needs for stack traces.
        target_compile_options(ranshaw-build-flags INTERFACE /fsanitize=address /Zi)
    else()
        set(_ranshaw_san_cflags "")
        foreach(_san IN LISTS _ranshaw_sanitizers)
            list(APPEND _ranshaw_san_cflags "-fsanitize=${_san}")
        endforeach()
        list(APPEND _ranshaw_san_cflags "-fno-omit-frame-pointer" "-g")
        target_compile_options(ranshaw-build-flags INTERFACE ${_ranshaw_san_cflags})
    endif()
endif()

# ── Linker hardening helper ──
# Only use CMake's built-in MINGW (set for GCC with MinGW). Clang on Windows can
# target either MinGW or MSVC ABI, and the linker varies (GNU ld vs lld-link) —
# applying the wrong linker flags breaks the build. We let Clang on Windows rely
# on the OS defaults for DEP/ASLR (enabled by default on modern Windows) rather
# than guessing the linker style.
function(ranshaw_harden_executable target)
    if(MSVC)
        if(_ranshaw_sanitized)
            # MSVC ASan: skip /guard:cf / /guard:ehcont / /CETCOMPAT (they conflict
            # with ASan's interceptor trampoline dispatch). /DYNAMICBASE /NXCOMPAT
            # /HIGHENTROPYVA are still safe — they are PE image bits, not runtime
            # hooks. /INCREMENTAL:NO /DEBUG are required so link.exe emits the
            # PDB ASan needs for symbolized reports.
            set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS
                " /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /INCREMENTAL:NO /DEBUG")
        else()
            # PE linker hardening — DEP + ASLR + CFG
            set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS
                " /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /guard:cf")
            # CET shadow stack compatibility — compile AND link flags must match
            if(HAS_GUARD_EHCONT)
                target_compile_options(${target} PRIVATE /guard:ehcont)
                set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS
                    " /guard:ehcont /CETCOMPAT")
            endif()
        endif()
    elseif(MINGW)
        # MinGW (GCC only) PE linker hardening — DEP + ASLR
        set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS
            " -Wl,--nxcompat -Wl,--dynamicbase -Wl,--high-entropy-va")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # ELF linker hardening
        set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS
            " -Wl,-z,relro,-z,now -Wl,-z,noexecstack")
    endif()

    # LTO link flags (disabled under sanitizers — LTO + instrumented builds
    # produce unreliable line tables and slow down builds significantly).
    if(ENABLE_LTO AND NOT _ranshaw_sanitized)
        if(MSVC)
            set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS " /LTCG")
        else()
            set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS " -flto")
        endif()
    endif()

    # Sanitizer link flags — GCC/Clang only (MSVC ASan links automatically
    # from the /fsanitize=address compile flag via clang_rt.asan_dynamic).
    if(_ranshaw_sanitized AND NOT MSVC)
        set(_san_ldflags "")
        foreach(_san IN LISTS _ranshaw_sanitizers)
            set(_san_ldflags "${_san_ldflags} -fsanitize=${_san}")
        endforeach()
        set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS "${_san_ldflags}")
    endif()
endfunction()

# ── Options ──
option(FORCE_PORTABLE "Force portable (32-bit) implementation even on 64-bit platforms" OFF)

# Detect x86_64 specifically (SIMD backends are x86_64-only)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^([xX]86_64|[aA][mM][dD]64)$")
    set(RANSHAW_IS_X86_64 TRUE)
else()
    set(RANSHAW_IS_X86_64 FALSE)
endif()

# SIMD options: default ON for x86_64, OFF otherwise
if(RANSHAW_IS_X86_64 AND NOT FORCE_PORTABLE)
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
    message(STATUS "RANSHAW: Forced portable (32-bit) implementation via FORCE_PORTABLE")
endif()

option(ENABLE_ECFFT "Enable ECFFT polynomial multiplication" OFF)

option(ENABLE_LTO "Enable link-time optimization" OFF)
if(ENABLE_LTO AND _ranshaw_sanitized)
    message(WARNING "RANSHAW: ENABLE_LTO ignored because RANSHAW_TEST_SANITIZERS is set (LTO + sanitizers produces unreliable debug info)")
elseif(ENABLE_LTO)
    if(MSVC)
        target_compile_options(ranshaw-build-flags INTERFACE /GL)
    else()
        target_compile_options(ranshaw-build-flags INTERFACE -flto)
    endif()
    message(STATUS "RANSHAW: Link-time optimization enabled")
endif()

# NOTE: Sanitizer options are declared near the top of this file as
# RANSHAW_TEST_SANITIZERS (comma-separated: address,undefined,thread).
# Legacy ENABLE_ASAN / ENABLE_UBSAN booleans have been removed.

# ── MinGW GCC + Debug SIMD alignment fix ──
# Windows x64 ABI only guarantees 16-byte stack alignment, but YMM/ZMM
# registers need 32/64-byte alignment.  At -O0, GCC spills AVX registers
# to the stack with aligned moves (vmovdqa), which SIGSEGV on the
# misaligned stack.  Force -O1 for SIMD backends in Debug mode so GCC
# keeps values in registers.
set(_MINGW_SIMD_FIX "")
if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(_MINGW_SIMD_FIX "$<$<CONFIG:Debug>:-O1>")
endif()

# ── Sub-libraries ──
add_subdirectory(common)
add_subdirectory(fp)
add_subdirectory(fq)
add_subdirectory(ran)
add_subdirectory(shaw)
add_subdirectory(ecfft)
add_subdirectory(ec-divisors)

# ── Main library: dispatch + master include + public C++ API ──
set(RANSHAW_API_SOURCES
    src/api_scalar.cpp
    src/api_point.cpp
    src/api_polynomial.cpp
    src/api_divisor.cpp
)

if(ENABLE_AVX2 OR ENABLE_AVX512)
    add_library(ranshaw STATIC
        src/cpuid.cpp
        src/dispatch.cpp
        ${RANSHAW_API_SOURCES}
    )
    message(STATUS "RANSHAW: SIMD dispatch enabled")

    if(ENABLE_AVX2)
        message(STATUS "RANSHAW: AVX2 backend enabled")
    endif()
    if(ENABLE_AVX512)
        message(STATUS "RANSHAW: AVX-512 IFMA backend enabled")
    endif()

    target_compile_definitions(ranshaw PUBLIC RANSHAW_SIMD=1)

    if(NOT ENABLE_AVX512)
        target_compile_definitions(ranshaw PUBLIC RANSHAW_NO_AVX512=1)
    endif()
    if(NOT ENABLE_AVX2)
        target_compile_definitions(ranshaw PUBLIC RANSHAW_NO_AVX2=1)
    endif()
else()
    add_library(ranshaw STATIC ${RANSHAW_API_SOURCES})
    target_compile_definitions(ranshaw PUBLIC RANSHAW_SIMD=0)
endif()

if(ENABLE_ECFFT)
    message(STATUS "RANSHAW: ECFFT polynomial multiplication enabled")
endif()

# Public interface for downstream consumers
target_compile_features(ranshaw PUBLIC cxx_std_17)
target_include_directories(ranshaw PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
# Downstream consumers get our headers as SYSTEM (suppresses their -Werror on our code)
target_include_directories(ranshaw SYSTEM INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(ranshaw PUBLIC
    ranshaw-ec-divisors
    ranshaw-ran
    ranshaw-shaw
    ranshaw-fp
    ranshaw-fq
    ranshaw-ecfft
    ranshaw-common
)

# Internal build flags (PRIVATE — not propagated to downstream)
target_link_libraries(ranshaw PRIVATE ranshaw-build-flags)
target_compile_definitions(ranshaw PRIVATE RANSHAW_INTERNAL_ACCESS)

# ── Tests ──
# BUILD_TESTS option is declared early (before ranshaw-build-flags) so
# RANSHAW_TEST_SANITIZERS can be validated against it.
if(BUILD_TESTS)
    enable_testing()
    message(STATUS "RANSHAW: Unit tests enabled")

    # Test binaries must also link ranshaw-build-flags so their own TUs
    # compile with the same flags as the library. In particular, under
    # RANSHAW_TEST_SANITIZERS=address, MSVC emits `annotate_string` /
    # `annotate_vector` STL annotations in the library .obj files that the
    # test TUs must match — a mismatch produces LNK2038 at link time.
    #
    # The monolithic src/tests.cpp has been split into per-topic TUs under
    # src/tests/ (field_fp, field_fq, point_ran, point_shaw, scalar_mul,
    # msm, sswu, batch_affine, pedersen, polynomial, ecfft, divisor, scalar,
    # serialization, dispatch, wei25519_bridge, verification) plus a shared
    # common.cpp and a thin tests_main.cpp dispatcher. This keeps any single
    # TU well under MSVC's link.exe / ASan instrumentation limits and lets
    # Phase 3 add new topic files without touching the existing monolith.
    set(RANSHAW_TEST_SOURCES
        src/tests_main.cpp
        src/tests/common.cpp
        src/tests/field_fp.cpp
        src/tests/field_fq.cpp
        src/tests/field_fq51x2_ifma.cpp
        src/tests/point_ran.cpp
        src/tests/point_shaw.cpp
        src/tests/scalar_mul.cpp
        src/tests/wei25519_bridge.cpp
        src/tests/msm.cpp
        src/tests/complete_add.cpp
        src/tests/complete_add_ifma.cpp
        src/tests/msm_ct.cpp
        src/tests/sswu.cpp
        src/tests/batch_affine.cpp
        src/tests/pedersen.cpp
        src/tests/polynomial.cpp
        src/tests/ecfft.cpp
        src/tests/divisor.cpp
        src/tests/scalar.cpp
        src/tests/serialization.cpp
        src/tests/dispatch.cpp
        src/tests/verification.cpp
        src/tests/edge_fields.cpp
        src/tests/malformed_decoders.cpp
        src/tests/concurrency.cpp
    )

    add_executable(ranshaw-tests ${RANSHAW_TEST_SOURCES})
    target_include_directories(ranshaw-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(ranshaw-tests PRIVATE ranshaw ranshaw-build-flags)
    target_compile_definitions(ranshaw-tests PRIVATE RANSHAW_INTERNAL_ACCESS)
    ranshaw_harden_executable(ranshaw-tests)
    add_test(NAME ranshaw-tests COMMAND ranshaw-tests)

    if(ENABLE_TIMING_TESTS)
        # ranshaw-tests-timing: standalone executable running a dudect-style
        # timing-leak statistical test on the CT Pedersen path. Advisory only
        # (the main() returns 0 even on statistical failure); the PR gate is
        # the disasm branch-grep, not this. Build opt-in because it takes
        # several minutes and jitters wildly on shared CI runners.
        add_executable(ranshaw-tests-timing src/tests_timing.cpp)
        target_link_libraries(ranshaw-tests-timing PRIVATE ranshaw ranshaw-build-flags)
        target_compile_definitions(ranshaw-tests-timing PRIVATE RANSHAW_INTERNAL_ACCESS)
        ranshaw_harden_executable(ranshaw-tests-timing)
        message(STATUS "RANSHAW: CT timing harness enabled (ranshaw-tests-timing)")
    endif()

    # IFMA test TUs need AVX-512F + IFMA to activate their bodies. In IFMA-
    # enabled builds we grant the flags; otherwise each TU compiles as a no-op
    # (its body is gated on __AVX512F__ && __AVX512IFMA__). Only a TU whose
    # AVX-512 code is reached behind a runtime ranshaw_has_avx512ifma() check
    # may carry these flags. complete_add.cpp is deliberately excluded: its
    # scalar complete-add tests run unconditionally, so it must build for the
    # baseline ISA or the compiler may emit AVX-512 into always-run code that
    # faults on CPUs without the feature. Its IFMA cross-checks were moved to
    # complete_add_ifma.cpp, which self-gates at runtime.
    if(ENABLE_AVX512)
        if(MSVC)
            set_source_files_properties(src/tests/field_fq51x2_ifma.cpp
                PROPERTIES COMPILE_FLAGS "/arch:AVX512")
        else()
            set_source_files_properties(src/tests/field_fq51x2_ifma.cpp
                src/tests/complete_add_ifma.cpp
                PROPERTIES COMPILE_FLAGS "-mavx512f -mavx512ifma ${_MINGW_SIMD_FIX}")
        endif()
    endif()

    # Same split pattern as ranshaw-tests: per-topic TUs under src/fuzz_tests/
    # with a shared common.cpp/h + registry.h and a thin tests_fuzz_main.cpp.
    set(RANSHAW_FUZZ_SOURCES
        src/tests_fuzz_main.cpp
        src/fuzz_tests/common.cpp
        src/fuzz_tests/scalar.cpp
        src/fuzz_tests/point.cpp
        src/fuzz_tests/serialization.cpp
        src/fuzz_tests/cycle.cpp
        src/fuzz_tests/scalarmult.cpp
        src/fuzz_tests/msm.cpp
        src/fuzz_tests/map.cpp
        src/fuzz_tests/is_qr.cpp
        src/fuzz_tests/fq_mul_diff.cpp
        src/fuzz_tests/fq_invert_diff.cpp
        src/fuzz_tests/fp_invert_diff.cpp
        src/fuzz_tests/msm_vartime_packed_diff.cpp
        src/fuzz_tests/scalarmult_vartime_packed_diff.cpp
        src/fuzz_tests/pedersen.cpp
        src/fuzz_tests/batch.cpp
        src/fuzz_tests/polynomial.cpp
        src/fuzz_tests/divisor.cpp
        src/fuzz_tests/verification.cpp
        src/fuzz_tests/ecfft.cpp
    )

    add_executable(ranshaw-fuzz-tests ${RANSHAW_FUZZ_SOURCES})
    target_include_directories(ranshaw-fuzz-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(ranshaw-fuzz-tests PRIVATE ranshaw ranshaw-build-flags)
    target_compile_definitions(ranshaw-fuzz-tests PRIVATE RANSHAW_INTERNAL_ACCESS)
    ranshaw_harden_executable(ranshaw-fuzz-tests)
    add_test(NAME ranshaw-fuzz-tests COMMAND ranshaw-fuzz-tests)

    # libFuzzer harnesses (Clang + Linux/macOS only, requires address sanitizer).
    # See tests/fuzz/CMakeLists.txt for the full gate and per-target setup.
    option(RANSHAW_BUILD_FUZZERS "Build libFuzzer harnesses (Clang + Linux/macOS only; requires RANSHAW_TEST_SANITIZERS to include address)" OFF)
    if(RANSHAW_BUILD_FUZZERS)
        add_subdirectory(tests/fuzz)
    endif()
endif()

# ── Benchmarks ──
option(BUILD_BENCHMARKS "Build benchmark executables" OFF)
if(DEFINED ENV{BUILD_BENCHMARKS})
    set(BUILD_BENCHMARKS $ENV{BUILD_BENCHMARKS})
endif()

if(BUILD_BENCHMARKS)
    message(STATUS "RANSHAW: Benchmarks enabled")

    add_executable(ranshaw-benchmark src/benchmark.cpp)
    target_link_libraries(ranshaw-benchmark PRIVATE ranshaw ranshaw-build-flags)
    target_compile_definitions(ranshaw-benchmark PRIVATE RANSHAW_INTERNAL_ACCESS)
    ranshaw_harden_executable(ranshaw-benchmark)

    add_executable(ranshaw-benchmark-contest src/benchmark_contest.cpp)
    target_link_libraries(ranshaw-benchmark-contest PRIVATE ranshaw ranshaw-build-flags)
    target_compile_definitions(ranshaw-benchmark-contest PRIVATE RANSHAW_INTERNAL_ACCESS)
    ranshaw_harden_executable(ranshaw-benchmark-contest)

    add_executable(ranshaw-benchmark-fcmpp src/benchmark_fcmpp.cpp)
    target_link_libraries(ranshaw-benchmark-fcmpp PRIVATE ranshaw ranshaw-build-flags)
    target_compile_definitions(ranshaw-benchmark-fcmpp PRIVATE RANSHAW_INTERNAL_ACCESS)
    ranshaw_harden_executable(ranshaw-benchmark-fcmpp)

    # Benchmarks are developer tools — relax sign-conversion warnings
    if(NOT MSVC)
        foreach(_bench ranshaw-benchmark ranshaw-benchmark-contest ranshaw-benchmark-fcmpp)
            target_compile_options(${_bench} PRIVATE -Wno-sign-conversion)
        endforeach()
    endif()
endif()

# ── Auxiliary tools ──
option(BUILD_TOOLS "Build auxiliary tools (ECFFT curve search)" OFF)
if(BUILD_TOOLS)
    # ECFFT tools use uint64_t field elements directly and require the x64 backend
    if(NOT FORCE_PORTABLE)
        add_executable(ranshaw-find-ecfft tools/find_ecfft_curve.cpp)
        target_link_libraries(ranshaw-find-ecfft PRIVATE ranshaw ranshaw-build-flags)
        ranshaw_harden_executable(ranshaw-find-ecfft)

        add_executable(ranshaw-gen-ecfft tools/gen_ecfft_data.cpp)
        target_link_libraries(ranshaw-gen-ecfft PRIVATE ranshaw ranshaw-build-flags)
        ranshaw_harden_executable(ranshaw-gen-ecfft)

        if(NOT MSVC)
            foreach(_tool ranshaw-find-ecfft ranshaw-gen-ecfft)
                target_compile_options(${_tool} PRIVATE -Wno-sign-conversion)
            endforeach()
        endif()
    endif()

    add_executable(ranshaw-gen-testvectors tools/gen_test_vectors.cpp)
    target_link_libraries(ranshaw-gen-testvectors PRIVATE ranshaw ranshaw-build-flags)
    target_compile_definitions(ranshaw-gen-testvectors PRIVATE RANSHAW_INTERNAL_ACCESS)
    ranshaw_harden_executable(ranshaw-gen-testvectors)

    if(NOT MSVC)
        target_compile_options(ranshaw-gen-testvectors PRIVATE -Wno-sign-conversion)
    endif()

    message(STATUS "RANSHAW: Auxiliary tools enabled")
endif()
