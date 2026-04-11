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

#include "fuzz_tests/common.h"
#include "fuzz_tests/registry.h"

void fuzz_divisor()
{
    std::cout << std::endl << "=== Fuzz: Divisor ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 16);

    static const unsigned char zero32[32] = {0};

    const int sizes[] = {2, 3, 4, 5, 8};

    /* Ran */
    for (int si = 0; si < 5; si++)
    {
        size_t n = (size_t)sizes[si];
        for (int trial = 0; trial < 10; trial++)
        {
            std::string label = "ran_div[n=" + std::to_string(n) + ",t=" + std::to_string(trial) + "]";

            std::vector<RanPoint> pts(n);
            for (size_t j = 0; j < n; j++)
                pts[j] = random_ran_point(rng);

            auto div_opt = RanDivisor::compute(pts.data(), n);
            check_true((label + " compute").c_str(), div_opt.has_value());
            auto div = *div_opt;

            /* Vanishing: evaluate at each defining point */
            bool vanish_ok = true;
            for (size_t j = 0; j < n; j++)
            {
                /* Get affine coordinates */
                ran_affine aff;
                ran_to_affine(&aff, &pts[j].raw());
                unsigned char xb[32], yb[32];
                fp_tobytes(xb, aff.x);
                fp_tobytes(yb, aff.y);

                auto ev_opt = div.evaluate(xb, yb);
                if (!ev_opt || std::memcmp(ev_opt->data(), zero32, 32) != 0)
                    vanish_ok = false;
            }
            check_true((label + " vanish").c_str(), vanish_ok);

            /* Non-member: evaluate at a random point NOT in the set */
            auto rp = random_ran_point(rng);
            ran_affine raff;
            ran_to_affine(&raff, &rp.raw());
            unsigned char rxb[32], ryb[32];
            fp_tobytes(rxb, raff.x);
            fp_tobytes(ryb, raff.y);
            auto rev_opt = div.evaluate(rxb, ryb);
            check_true(
                (label + " non_member").c_str(), rev_opt.has_value() && std::memcmp(rev_opt->data(), zero32, 32) != 0);
        }
    }

    /* Shaw */
    for (int si = 0; si < 5; si++)
    {
        size_t n = (size_t)sizes[si];
        for (int trial = 0; trial < 10; trial++)
        {
            std::string label = "shaw_div[n=" + std::to_string(n) + ",t=" + std::to_string(trial) + "]";

            std::vector<ShawPoint> pts(n);
            for (size_t j = 0; j < n; j++)
                pts[j] = random_shaw_point(rng);

            auto div_opt = ShawDivisor::compute(pts.data(), n);
            check_true((label + " compute").c_str(), div_opt.has_value());
            auto div = *div_opt;

            bool vanish_ok = true;
            for (size_t j = 0; j < n; j++)
            {
                shaw_affine aff;
                shaw_to_affine(&aff, &pts[j].raw());
                unsigned char xb[32], yb[32];
                fq_tobytes(xb, aff.x);
                fq_tobytes(yb, aff.y);

                auto ev_opt = div.evaluate(xb, yb);
                if (!ev_opt || std::memcmp(ev_opt->data(), zero32, 32) != 0)
                    vanish_ok = false;
            }
            check_true((label + " vanish").c_str(), vanish_ok);

            auto rp = random_shaw_point(rng);
            shaw_affine raff;
            shaw_to_affine(&raff, &rp.raw());
            unsigned char rxb[32], ryb[32];
            fq_tobytes(rxb, raff.x);
            fq_tobytes(ryb, raff.y);
            auto rev_opt = div.evaluate(rxb, ryb);
            check_true(
                (label + " non_member").c_str(), rev_opt.has_value() && std::memcmp(rev_opt->data(), zero32, 32) != 0);
        }
    }
}

/* ======================================================================
 * 17. fuzz_divisor_scalar_mul — ~200 checks
 * ====================================================================== */


void fuzz_divisor_scalar_mul()
{
    std::cout << std::endl << "=== Fuzz: Divisor ScalarMul ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 17);

    /* Ran */
    for (int trial = 0; trial < 25; trial++)
    {
        std::string label = "ran_smdiv[" + std::to_string(trial) + "]";

        auto s = random_ran_scalar(rng);
        auto sb = s.to_bytes();
        auto P = random_ran_point(rng);

        /* Get affine point */
        ran_affine aff;
        ran_to_affine(&aff, &P.raw());

        ran_divisor d;
        ran_scalar_mul_divisor(&d, sb.data(), &aff);

        /* a(x) should have nontrivial degree */
        check_true((label + " a_nontrivial").c_str(), d.a.coeffs.size() > 1);

        /* Evaluate at the input point — should vanish */
        unsigned char xb[32], yb[32];
        fp_tobytes(xb, aff.x);
        fp_tobytes(yb, aff.y);

        fp_fe result;
        ran_evaluate_divisor(result, &d, aff.x, aff.y);
        unsigned char result_bytes[32];
        fp_tobytes(result_bytes, result);
        static const unsigned char zero32[32] = {0};
        check_true((label + " vanish").c_str(), std::memcmp(result_bytes, zero32, 32) == 0);
    }

    /* Shaw */
    for (int trial = 0; trial < 25; trial++)
    {
        std::string label = "shaw_smdiv[" + std::to_string(trial) + "]";

        auto s = random_shaw_scalar(rng);
        auto sb = s.to_bytes();
        auto P = random_shaw_point(rng);

        shaw_affine aff;
        shaw_to_affine(&aff, &P.raw());

        shaw_divisor d;
        shaw_scalar_mul_divisor(&d, sb.data(), &aff);

        check_true((label + " a_nontrivial").c_str(), d.a.coeffs.size() > 1);

        fq_fe result;
        shaw_evaluate_divisor(result, &d, aff.x, aff.y);
        unsigned char result_bytes[32];
        fq_tobytes(result_bytes, result);
        static const unsigned char zero32[32] = {0};
        check_true((label + " vanish").c_str(), std::memcmp(result_bytes, zero32, 32) == 0);
    }
}

/* ======================================================================
 * 18. fuzz_operator_plus_regression — ~2,000 checks
 * ====================================================================== */
