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

void fuzz_pedersen()
{
    std::cout << std::endl << "=== Fuzz: Pedersen Commitments ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 12);

    const int sizes[] = {1, 2, 4, 8, 16};

    /* Ran */
    for (int si = 0; si < 5; si++)
    {
        size_t n = (size_t)sizes[si];
        for (int trial = 0; trial < 10; trial++)
        {
            std::string label = "ran_ped[n=" + std::to_string(n) + ",t=" + std::to_string(trial) + "]";

            auto blinding = random_ran_scalar(rng);
            auto H = random_ran_point(rng);
            std::vector<RanScalar> vals(n);
            std::vector<RanPoint> gens(n);
            for (size_t j = 0; j < n; j++)
            {
                vals[j] = random_ran_scalar(rng);
                gens[j] = random_ran_point(rng);
            }

            auto commit = RanPoint::pedersen_commit(blinding, H, vals.data(), gens.data(), n);

            /* Naive: b*H + sum(v[i]*G[i]) */
            auto naive = H.scalar_mul_vartime(blinding);
            for (size_t j = 0; j < n; j++)
                naive = naive + gens[j].scalar_mul_vartime(vals[j]);

            check_true((label + " correct").c_str(), ran_points_equal(commit, naive));

            /* Cross-check: pedersen_commit == multi_scalar_mul with combined arrays */
            std::vector<RanScalar> all_scalars(n + 1);
            std::vector<RanPoint> all_points(n + 1);
            all_scalars[0] = blinding;
            all_points[0] = H;
            for (size_t j = 0; j < n; j++)
            {
                all_scalars[j + 1] = vals[j];
                all_points[j + 1] = gens[j];
            }
            auto msm = RanPoint::multi_scalar_mul(all_scalars.data(), all_points.data(), n + 1);
            check_true((label + " ped==msm").c_str(), ran_points_equal(commit, msm));
        }
    }

    /* Homomorphism: C(b1,v1) + C(b2,v2) == C(b1+b2, v1+v2) */
    for (int trial = 0; trial < 10; trial++)
    {
        std::string label = "ran_ped_homo[" + std::to_string(trial) + "]";
        size_t n = 4;
        auto H = random_ran_point(rng);
        std::vector<RanPoint> gens(n);
        for (size_t j = 0; j < n; j++)
            gens[j] = random_ran_point(rng);

        auto b1 = random_ran_scalar(rng);
        auto b2 = random_ran_scalar(rng);
        std::vector<RanScalar> v1(n), v2(n), vsum(n);
        for (size_t j = 0; j < n; j++)
        {
            v1[j] = random_ran_scalar(rng);
            v2[j] = random_ran_scalar(rng);
            vsum[j] = v1[j] + v2[j];
        }

        auto C1 = RanPoint::pedersen_commit(b1, H, v1.data(), gens.data(), n);
        auto C2 = RanPoint::pedersen_commit(b2, H, v2.data(), gens.data(), n);
        auto Csum = RanPoint::pedersen_commit(b1 + b2, H, vsum.data(), gens.data(), n);
        check_true(label.c_str(), ran_points_equal(C1 + C2, Csum));
    }

    /* Zero blinding */
    for (int trial = 0; trial < 5; trial++)
    {
        std::string label = "ran_ped_zblind[" + std::to_string(trial) + "]";
        size_t n = 4;
        auto H = random_ran_point(rng);
        std::vector<RanPoint> gens(n);
        std::vector<RanScalar> vals(n);
        for (size_t j = 0; j < n; j++)
        {
            gens[j] = random_ran_point(rng);
            vals[j] = random_ran_scalar(rng);
        }
        auto commit = RanPoint::pedersen_commit(RanScalar::zero(), H, vals.data(), gens.data(), n);
        auto naive = RanPoint::identity();
        for (size_t j = 0; j < n; j++)
            naive = naive + gens[j].scalar_mul_vartime(vals[j]);
        check_true(label.c_str(), ran_points_equal(commit, naive));
    }

    /* Shaw */
    for (int si = 0; si < 5; si++)
    {
        size_t n = (size_t)sizes[si];
        for (int trial = 0; trial < 10; trial++)
        {
            std::string label = "shaw_ped[n=" + std::to_string(n) + ",t=" + std::to_string(trial) + "]";

            auto blinding = random_shaw_scalar(rng);
            auto H = random_shaw_point(rng);
            std::vector<ShawScalar> vals(n);
            std::vector<ShawPoint> gens(n);
            for (size_t j = 0; j < n; j++)
            {
                vals[j] = random_shaw_scalar(rng);
                gens[j] = random_shaw_point(rng);
            }

            auto commit = ShawPoint::pedersen_commit(blinding, H, vals.data(), gens.data(), n);
            auto naive = H.scalar_mul_vartime(blinding);
            for (size_t j = 0; j < n; j++)
                naive = naive + gens[j].scalar_mul_vartime(vals[j]);
            check_true((label + " correct").c_str(), shaw_points_equal(commit, naive));

            /* Cross-check: pedersen_commit == multi_scalar_mul with combined arrays */
            std::vector<ShawScalar> all_scalars(n + 1);
            std::vector<ShawPoint> all_points(n + 1);
            all_scalars[0] = blinding;
            all_points[0] = H;
            for (size_t j = 0; j < n; j++)
            {
                all_scalars[j + 1] = vals[j];
                all_points[j + 1] = gens[j];
            }
            auto msm = ShawPoint::multi_scalar_mul(all_scalars.data(), all_points.data(), n + 1);
            check_true((label + " ped==msm").c_str(), shaw_points_equal(commit, msm));
        }
    }

    for (int trial = 0; trial < 10; trial++)
    {
        std::string label = "shaw_ped_homo[" + std::to_string(trial) + "]";
        size_t n = 4;
        auto H = random_shaw_point(rng);
        std::vector<ShawPoint> gens(n);
        for (size_t j = 0; j < n; j++)
            gens[j] = random_shaw_point(rng);

        auto b1 = random_shaw_scalar(rng);
        auto b2 = random_shaw_scalar(rng);
        std::vector<ShawScalar> v1(n), v2(n), vsum(n);
        for (size_t j = 0; j < n; j++)
        {
            v1[j] = random_shaw_scalar(rng);
            v2[j] = random_shaw_scalar(rng);
            vsum[j] = v1[j] + v2[j];
        }

        auto C1 = ShawPoint::pedersen_commit(b1, H, v1.data(), gens.data(), n);
        auto C2 = ShawPoint::pedersen_commit(b2, H, v2.data(), gens.data(), n);
        auto Csum = ShawPoint::pedersen_commit(b1 + b2, H, vsum.data(), gens.data(), n);
        check_true(label.c_str(), shaw_points_equal(C1 + C2, Csum));
    }

    for (int trial = 0; trial < 5; trial++)
    {
        std::string label = "shaw_ped_zblind[" + std::to_string(trial) + "]";
        size_t n = 4;
        auto H = random_shaw_point(rng);
        std::vector<ShawPoint> gens(n);
        std::vector<ShawScalar> vals(n);
        for (size_t j = 0; j < n; j++)
        {
            gens[j] = random_shaw_point(rng);
            vals[j] = random_shaw_scalar(rng);
        }
        auto commit = ShawPoint::pedersen_commit(ShawScalar::zero(), H, vals.data(), gens.data(), n);
        auto naive = ShawPoint::identity();
        for (size_t j = 0; j < n; j++)
            naive = naive + gens[j].scalar_mul_vartime(vals[j]);
        check_true(label.c_str(), shaw_points_equal(commit, naive));
    }
}

/* ======================================================================
 * 13. fuzz_batch_affine — ~400
 * ====================================================================== */
