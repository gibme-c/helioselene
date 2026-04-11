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

void fuzz_scalarmul_consistency()
{
    std::cout << std::endl << "=== Fuzz: ScalarMul Consistency ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 7);

    for (int i = 0; i < 250; i++)
    {
        std::string label = "ran_sm[" + std::to_string(i) + "]";

        auto P = random_ran_point(rng);
        auto a = random_ran_scalar(rng);
        auto b = random_ran_scalar(rng);

        /* CT vs vartime */
        check_true((label + " ct==vt").c_str(), ran_points_equal(P.scalar_mul(a), P.scalar_mul_vartime(a)));
        /* Linearity: P*(a+b) == P*a + P*b */
        auto lhs = P.scalar_mul_vartime(a + b);
        auto rhs = P.scalar_mul_vartime(a) + P.scalar_mul_vartime(b);
        check_true((label + " linear").c_str(), ran_points_equal(lhs, rhs));
        /* Composition: (a*b)*G == a*(b*G) */
        auto G = RanPoint::generator();
        auto lhs2 = G.scalar_mul_vartime(a * b);
        auto rhs2 = G.scalar_mul_vartime(b).scalar_mul_vartime(a);
        check_true((label + " compose").c_str(), ran_points_equal(lhs2, rhs2));
    }

    for (int i = 0; i < 250; i++)
    {
        std::string label = "shaw_sm[" + std::to_string(i) + "]";

        auto P = random_shaw_point(rng);
        auto a = random_shaw_scalar(rng);
        auto b = random_shaw_scalar(rng);

        check_true((label + " ct==vt").c_str(), shaw_points_equal(P.scalar_mul(a), P.scalar_mul_vartime(a)));
        auto lhs = P.scalar_mul_vartime(a + b);
        auto rhs = P.scalar_mul_vartime(a) + P.scalar_mul_vartime(b);
        check_true((label + " linear").c_str(), shaw_points_equal(lhs, rhs));
        auto G = ShawPoint::generator();
        auto lhs2 = G.scalar_mul_vartime(a * b);
        auto rhs2 = G.scalar_mul_vartime(b).scalar_mul_vartime(a);
        check_true((label + " compose").c_str(), shaw_points_equal(lhs2, rhs2));
    }
}

/* ======================================================================
 * 8. fuzz_msm_random — ~400 checks
 * ====================================================================== */


void fuzz_all_path_cross_validation()
{
    std::cout << std::endl << "=== Fuzz: All-Path Cross-Validation ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 21);

    /* ---- Ran ---- */
    {
        auto test_ran = [&](const std::string &label, const RanScalar &s, const RanPoint &P)
        {
            /* Path A: CT scalarmul (ground truth) */
            auto A = P.scalar_mul(s);

            /* Path B: Vartime wNAF */
            auto B = P.scalar_mul_vartime(s);
            check_true((label + " B==A").c_str(), ran_points_equal(B, A));

            /* Path C: MSM with n=1 */
            auto C = RanPoint::multi_scalar_mul(&s, &P, 1);
            check_true((label + " C==A").c_str(), ran_points_equal(C, A));

            /* Path D: Pedersen commit (s*P + 0*G) */
            auto zero_s = RanScalar::zero();
            auto G = RanPoint::generator();
            auto D = RanPoint::pedersen_commit(s, P, &zero_s, &G, 1);
            check_true((label + " D==A").c_str(), ran_points_equal(D, A));

            /* Path E: Fixed-base CT */
            auto sb = s.to_bytes();
            ran_affine fixed_table[16];
            ran_scalarmult_fixed_precompute(fixed_table, &P.raw());
            RanPoint E;
            ran_scalarmult_fixed(&E.raw(), sb.data(), fixed_table);
            check_true((label + " E==A").c_str(), ran_points_equal(E, A));

            /* Path F: Fixed-base MSM (n=1, delegates to E internally) */
            RanPoint F;
            const ran_affine *tp = fixed_table;
            ran_msm_fixed(&F.raw(), sb.data(), &tp, 1);
            check_true((label + " F==A").c_str(), ran_points_equal(F, A));
        };

        /* Edge scalars */
        RanScalar edge_scalars[] = {
            RanScalar::zero(),
            RanScalar::one(),
            RanScalar::one() + RanScalar::one(),
            -RanScalar::one(),
            -(RanScalar::one() + RanScalar::one()),
        };
        const char *edge_names[] = {"0", "1", "2", "q-1", "q-2"};
        for (int ei = 0; ei < 5; ei++)
        {
            for (int trial = 0; trial < 10; trial++)
            {
                auto P = random_ran_point(rng);
                std::string label = "ran_xval[s=" + std::string(edge_names[ei]) + ",t=" + std::to_string(trial) + "]";
                test_ran(label, edge_scalars[ei], P);
            }
        }

        /* Random 256-bit scalars */
        for (int trial = 0; trial < 200; trial++)
        {
            auto s = random_ran_scalar(rng);
            auto P = random_ran_point(rng);
            std::string label = "ran_xval[rand," + std::to_string(trial) + "]";
            test_ran(label, s, P);
        }

        /* Small scalars (< 2^64) */
        for (int trial = 0; trial < 20; trial++)
        {
            uint8_t wide[64] = {};
            rng.fill_bytes(wide, 8); /* only fill first 8 bytes */
            auto s = RanScalar::reduce_wide(wide);
            auto P = random_ran_point(rng);
            std::string label = "ran_xval[small," + std::to_string(trial) + "]";
            test_ran(label, s, P);
        }

        /* High-bit scalars (bit 254 set) */
        for (int trial = 0; trial < 20; trial++)
        {
            uint8_t wide[64] = {};
            rng.fill_bytes(wide, 32);
            wide[31] |= 0x40; /* set bit 254 */
            wide[31] &= 0x7f; /* clear bit 255 to stay in range */
            auto s = RanScalar::reduce_wide(wide);
            auto P = random_ran_point(rng);
            std::string label = "ran_xval[high," + std::to_string(trial) + "]";
            test_ran(label, s, P);
        }
    }

    /* ---- Shaw ---- */
    {
        auto test_shaw = [&](const std::string &label, const ShawScalar &s, const ShawPoint &P)
        {
            /* Path A: CT scalarmul (ground truth) */
            auto A = P.scalar_mul(s);

            /* Path B: Vartime wNAF */
            auto B = P.scalar_mul_vartime(s);
            check_true((label + " B==A").c_str(), shaw_points_equal(B, A));

            /* Path C: MSM with n=1 */
            auto C = ShawPoint::multi_scalar_mul(&s, &P, 1);
            check_true((label + " C==A").c_str(), shaw_points_equal(C, A));

            /* Path D: Pedersen commit (s*P + 0*G) */
            auto zero_s = ShawScalar::zero();
            auto G = ShawPoint::generator();
            auto D = ShawPoint::pedersen_commit(s, P, &zero_s, &G, 1);
            check_true((label + " D==A").c_str(), shaw_points_equal(D, A));

            /* Path E: Fixed-base CT */
            auto sb = s.to_bytes();
            shaw_affine fixed_table[16];
            shaw_scalarmult_fixed_precompute(fixed_table, &P.raw());
            ShawPoint E;
            shaw_scalarmult_fixed(&E.raw(), sb.data(), fixed_table);
            check_true((label + " E==A").c_str(), shaw_points_equal(E, A));

            /* Path F: Fixed-base MSM (n=1, delegates to E internally) */
            ShawPoint F;
            const shaw_affine *tp = fixed_table;
            shaw_msm_fixed(&F.raw(), sb.data(), &tp, 1);
            check_true((label + " F==A").c_str(), shaw_points_equal(F, A));
        };

        /* Edge scalars */
        ShawScalar edge_scalars[] = {
            ShawScalar::zero(),
            ShawScalar::one(),
            ShawScalar::one() + ShawScalar::one(),
            -ShawScalar::one(),
            -(ShawScalar::one() + ShawScalar::one()),
        };
        const char *edge_names[] = {"0", "1", "2", "p-1", "p-2"};
        for (int ei = 0; ei < 5; ei++)
        {
            for (int trial = 0; trial < 10; trial++)
            {
                auto P = random_shaw_point(rng);
                std::string label = "shaw_xval[s=" + std::string(edge_names[ei]) + ",t=" + std::to_string(trial) + "]";
                test_shaw(label, edge_scalars[ei], P);
            }
        }

        /* Random 256-bit scalars */
        for (int trial = 0; trial < 200; trial++)
        {
            auto s = random_shaw_scalar(rng);
            auto P = random_shaw_point(rng);
            std::string label = "shaw_xval[rand," + std::to_string(trial) + "]";
            test_shaw(label, s, P);
        }

        /* Small scalars (< 2^64) */
        for (int trial = 0; trial < 20; trial++)
        {
            uint8_t wide[64] = {};
            rng.fill_bytes(wide, 8);
            auto s = ShawScalar::reduce_wide(wide);
            auto P = random_shaw_point(rng);
            std::string label = "shaw_xval[small," + std::to_string(trial) + "]";
            test_shaw(label, s, P);
        }

        /* High-bit scalars (bit 254 set) */
        for (int trial = 0; trial < 20; trial++)
        {
            uint8_t wide[64] = {};
            rng.fill_bytes(wide, 32);
            wide[31] |= 0x40;
            wide[31] &= 0x7f;
            auto s = ShawScalar::reduce_wide(wide);
            auto P = random_shaw_point(rng);
            std::string label = "shaw_xval[high," + std::to_string(trial) + "]";
            test_shaw(label, s, P);
        }
    }
}
