# libFuzzer harnesses for gibme-c/ranshaw.
#
# Opt-in via -DRANSHAW_BUILD_FUZZERS=ON at the top level. Clang-only,
# Linux/macOS only (Windows clang-cl has a libFuzzer + ASan static/dynamic
# CRT mismatch that is out of scope). Requires BUILD_TESTS=ON and
# RANSHAW_TEST_SANITIZERS to include 'address' (libFuzzer is paired with
# ASan end-to-end; a plain fuzzer build with no sanitizer defeats the
# purpose of coverage-guided fuzzing on untrusted inputs).

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "RANSHAW_BUILD_FUZZERS requires Clang (libFuzzer)")
endif()

if(WIN32)
    message(FATAL_ERROR "RANSHAW_BUILD_FUZZERS is not supported on Windows; use a Linux or macOS Clang build")
endif()

list(FIND _ranshaw_sanitizers "address" _san_addr_idx)
if(_san_addr_idx EQUAL -1)
    message(FATAL_ERROR
        "RANSHAW_BUILD_FUZZERS requires RANSHAW_TEST_SANITIZERS to include 'address' "
        "(current value: '${RANSHAW_TEST_SANITIZERS}')")
endif()

# Fuzz-specific compile flags. `fuzzer-no-link` enables coverage
# instrumentation without pulling in libFuzzer's main; each harness links
# libFuzzer via `fuzzer` in LINK_FLAGS. ASan/UBSan/TSan flags are already
# applied to ranshaw-build-flags at the top level via the sanitizer CSV.
set(_fuzz_cflags -fsanitize=fuzzer-no-link -g -O1)
set(_fuzz_lflags "-fsanitize=fuzzer")

set(RANSHAW_FUZZ_TARGETS
    fuzz_ran_scalar_roundtrip
    fuzz_shaw_scalar_roundtrip
    fuzz_ran_scalar_reduce
    fuzz_shaw_scalar_reduce
    fuzz_ran_point_decode
    fuzz_shaw_point_decode
    fuzz_ran_map_to_curve
    fuzz_shaw_map_to_curve
)

foreach(t ${RANSHAW_FUZZ_TARGETS})
    add_executable(ranshaw-${t} ${t}.cpp)
    set_target_properties(ranshaw-${t} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    target_link_libraries(ranshaw-${t} PRIVATE ranshaw ranshaw-build-flags)
    target_compile_definitions(ranshaw-${t} PRIVATE RANSHAW_INTERNAL_ACCESS)
    target_compile_options(ranshaw-${t} PRIVATE ${_fuzz_cflags})
    set_property(TARGET ranshaw-${t} APPEND_STRING PROPERTY LINK_FLAGS " ${_fuzz_lflags}")
    ranshaw_harden_executable(ranshaw-${t})
endforeach()

message(STATUS "RANSHAW: libFuzzer harnesses: ${RANSHAW_FUZZ_TARGETS}")
