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
 * @file src/tests_fuzz_main.cpp
 * @brief Thin main() dispatcher for the split ranshaw property-based fuzz suite.
 */

#include "fuzz_tests/common.h"
#include "fuzz_tests/registry.h"

int main(int argc, char *argv[])
{
    uint64_t seed = 0ULL;
    const char *dispatch_label = "baseline (x64/portable)";
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
        else if (std::strcmp(argv[i], "--quiet") == 0)
        {
            quiet_mode = true;
        }
        else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
        {
            seed = std::strtoull(argv[++i], nullptr, 0);
        }
        else
        {
            std::cerr << "Usage: " << argv[0] << " [--init | --autotune] [--quiet] [--seed <N>]" << std::endl;
            return 1;
        }
    }

    std::cout << "RanShaw Fuzz Tests" << std::endl;
    std::cout << "======================" << std::endl;
    std::cout << "Dispatch: " << dispatch_label << std::endl;
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
    std::cout << "PRNG seed: 0x" << std::hex << seed << std::dec << std::endl;
#ifdef RANSHAW_ECFFT
    std::cout << "ECFFT: enabled" << std::endl;
#else
    std::cout << "ECFFT: disabled" << std::endl;
#endif

    global_seed = seed;

#ifdef RANSHAW_ECFFT
    /* Initialize global ECFFT contexts so that FqPolynomial::operator* and
     * FpPolynomial::operator* dispatch to the ECFFT path for large multiplies
     * (degree >= 1024).  Without this, fq_poly_mul / fp_poly_mul fall through
     * to Karatsuba even when ECFFT is compiled in. */
    ecfft_global_init();
#endif

    fuzz_scalar_arithmetic();
    fuzz_scalar_edge_cases();
    fuzz_point_arithmetic();
    fuzz_ipa_edge_cases();
    fuzz_serialization_roundtrip();
    fuzz_cross_curve_cycle();
    fuzz_scalarmul_consistency();
    fuzz_msm_random();
    fuzz_msm_sparse();
    fuzz_map_to_curve();
    fuzz_wei25519_bridge();
    fuzz_pedersen();
    fuzz_batch_affine();
    fuzz_polynomial();
    fuzz_polynomial_protocol_sizes();
    fuzz_divisor();
    fuzz_divisor_scalar_mul();
    fuzz_operator_plus_regression();
    fuzz_verification_equation();
    fuzz_reduce_wide_distribution();
    fuzz_all_path_cross_validation();
#ifdef RANSHAW_ECFFT
    fuzz_ecfft_poly_mul();
#endif

    std::cout << std::endl << "======================" << std::endl;
    std::cout << "Total:  " << tests_run << std::endl;
    std::cout << "Passed: " << tests_passed << std::endl;
    std::cout << "Failed: " << tests_failed << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
