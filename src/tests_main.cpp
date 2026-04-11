// Copyright (c) 2025-2026, Brandon Lehmann
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file src/tests_main.cpp
 * @brief Thin main() dispatcher for the split ranshaw unit test suite.
 *
 * Each topic test function lives in its own TU under src/tests/<topic>.cpp
 * and is declared in src/tests/registry.h. This file parses the CLI flags
 * (--init, --autotune), invokes each topic in order, and prints the final
 * tally from the shared counters in src/tests/common.cpp.
 */

#include "tests/common.h"
#include "tests/registry.h"

int main(int argc, char *argv[])
{
    const char *dispatch_label = "baseline (x64/portable)";
    bool concurrency_only = false;
    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--autotune") == 0)
        {
            ranshaw_autotune();
            dispatch_label = "autotune";
        }
        else if (std::strcmp(argv[i], "--init") == 0)
        {
            ranshaw_init();
            dispatch_label = "init (CPUID heuristic)";
        }
        else if (std::strcmp(argv[i], "--concurrency-only") == 0)
        {
            concurrency_only = true;
        }
        else
        {
            std::cerr << "Usage: " << argv[0] << " [--init | --autotune | --concurrency-only]" << std::endl;
            return 1;
        }
    }

    std::cout << "RanShaw Unit Tests" << std::endl;
    std::cout << "======================" << std::endl;
    std::cout << "Dispatch: " << dispatch_label << std::endl;
    if (concurrency_only)
        std::cout << "Mode: --concurrency-only (TSan)" << std::endl;
#if RANSHAW_SIMD
    std::cout << "CPU features:";
    if (ranshaw_has_avx2())
        std::cout << " AVX2";
    if (ranshaw_has_avx512f())
        std::cout << " AVX512F";
    if (ranshaw_has_avx512ifma())
        std::cout << " AVX512IFMA";
    if (!ranshaw_cpu_features())
        std::cout << " (none)";
    std::cout << std::endl;
#endif

    // --concurrency-only mode: skip the full suite and run only the
    // thread-race tests. Used by the TSan CI job, whose goal is to
    // surface races under thread-sanitizer instrumentation — running
    // the full suite under TSan would take hours and mostly duplicate
    // coverage the regular jobs already provide.
    if (concurrency_only)
    {
        test_concurrency();
        std::cout << std::endl << "======================" << std::endl;
        std::cout << "Total:  " << tests_run << std::endl;
        std::cout << "Passed: " << tests_passed << std::endl;
        std::cout << "Failed: " << tests_failed << std::endl;
        return tests_failed > 0 ? 1 : 0;
    }

    test_fp();
    test_fq();
    test_fp_sqrt();
    test_ran_points();
    test_shaw_points();
    test_ran_scalarmult();
    test_shaw_scalarmult();
    test_wei25519();
    test_ran_msm();
    test_shaw_msm();
    test_fp_sqrt_sswu();
    test_ran_sswu();
    test_shaw_sswu();
    test_ran_batch_affine();
    test_shaw_batch_affine();
    test_ran_pedersen();
    test_shaw_pedersen();
    test_fp_poly();
    test_fq_poly();
    test_ran_divisor();
    test_shaw_divisor();
    test_fp_extended();
    test_fq_extended();
    test_serialization_edges();
    test_ran_point_edges();
    test_shaw_point_edges();
    test_scalarmult_extended();
    test_msm_extended();
    test_batch_affine_extended();
    test_batch_invert();
    test_fixed_base_scalarmult();
    test_precomputed_tables();
    test_msm_fixed();
    test_pedersen_extended();
    test_poly_extended();
    test_divisor_extended();
    test_point_to_scalar();
    test_ran_scalar();
    test_shaw_scalar();
    test_poly_interpolate();
    test_karatsuba();
#ifdef RANSHAW_ECFFT
    test_ecfft();
#endif
    test_eval_divisor();
    test_serialization_roundtrip();
    test_vector_validation();
    test_vector_validation_c_primitives();
    test_dispatch();
    test_cpp_api();

    // Phase 3 additions: land as new topic TUs; existing topic files are
    // untouched so MSVC link.exe / ASan instrumentation limits stay well
    // below the per-TU ceiling that motivated Phase 1.5.
    test_edge_fields();
    test_malformed_decoders();

    std::cout << std::endl << "======================" << std::endl;
    std::cout << "Total:  " << tests_run << std::endl;
    std::cout << "Passed: " << tests_passed << std::endl;
    std::cout << "Failed: " << tests_failed << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
