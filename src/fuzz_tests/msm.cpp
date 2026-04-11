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

void fuzz_msm_random()
{
    std::cout << std::endl << "=== Fuzz: MSM Random ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 8);

    const int sizes[] = {1, 2, 4, 8, 16, 33, 64};

    for (int si = 0; si < 7; si++)
    {
        size_t n = (size_t)sizes[si];
        for (int trial = 0; trial < 8; trial++)
        {
            std::string label = "ran_msm[n=" + std::to_string(n) + ",t=" + std::to_string(trial) + "]";

            std::vector<RanScalar> scalars(n);
            std::vector<RanPoint> points(n);
            for (size_t j = 0; j < n; j++)
            {
                scalars[j] = random_ran_scalar(rng);
                points[j] = random_ran_point(rng);
            }

            auto msm = RanPoint::multi_scalar_mul(scalars.data(), points.data(), n);
            auto naive = RanPoint::identity();
            for (size_t j = 0; j < n; j++)
                naive = naive + points[j].scalar_mul_vartime(scalars[j]);

            check_true(label.c_str(), ran_points_equal(msm, naive));
        }
    }

    for (int si = 0; si < 7; si++)
    {
        size_t n = (size_t)sizes[si];
        for (int trial = 0; trial < 8; trial++)
        {
            std::string label = "shaw_msm[n=" + std::to_string(n) + ",t=" + std::to_string(trial) + "]";

            std::vector<ShawScalar> scalars(n);
            std::vector<ShawPoint> points(n);
            for (size_t j = 0; j < n; j++)
            {
                scalars[j] = random_shaw_scalar(rng);
                points[j] = random_shaw_point(rng);
            }

            auto msm = ShawPoint::multi_scalar_mul(scalars.data(), points.data(), n);
            auto naive = ShawPoint::identity();
            for (size_t j = 0; j < n; j++)
                naive = naive + points[j].scalar_mul_vartime(scalars[j]);

            check_true(label.c_str(), shaw_points_equal(msm, naive));
        }
    }
}

/* ======================================================================
 * 9. fuzz_msm_sparse — ~400
 * ====================================================================== */


void fuzz_msm_sparse()
{
    std::cout << std::endl << "=== Fuzz: MSM Sparse ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 9);

    /* Helper lambda-style tests for each curve */
    /* Ran */
    for (int trial = 0; trial < 20; trial++)
    {
        std::string label = "ran_sparse[" + std::to_string(trial) + "]";
        size_t n = 8;
        std::vector<RanScalar> scalars(n);
        std::vector<RanPoint> points(n);

        /* Zero scalars mixed in */
        for (size_t j = 0; j < n; j++)
        {
            points[j] = random_ran_point(rng);
            if (j % 3 == 0)
                scalars[j] = RanScalar::zero();
            else
                scalars[j] = random_ran_scalar(rng);
        }

        auto msm = RanPoint::multi_scalar_mul(scalars.data(), points.data(), n);
        auto naive = RanPoint::identity();
        for (size_t j = 0; j < n; j++)
            naive = naive + points[j].scalar_mul_vartime(scalars[j]);
        check_true((label + " zero_mixed").c_str(), ran_points_equal(msm, naive));
    }

    /* All scalars = one */
    for (int trial = 0; trial < 10; trial++)
    {
        std::string label = "ran_all_one[" + std::to_string(trial) + "]";
        size_t n = 8;
        std::vector<RanScalar> scalars(n, RanScalar::one());
        std::vector<RanPoint> points(n);
        auto sum = RanPoint::identity();
        for (size_t j = 0; j < n; j++)
        {
            points[j] = random_ran_point(rng);
            sum = sum + points[j];
        }
        auto msm = RanPoint::multi_scalar_mul(scalars.data(), points.data(), n);
        check_true(label.c_str(), ran_points_equal(msm, sum));
    }

    /* Same point repeated */
    for (int trial = 0; trial < 10; trial++)
    {
        std::string label = "ran_same_pt[" + std::to_string(trial) + "]";
        size_t n = 8;
        auto P = random_ran_point(rng);
        std::vector<RanPoint> points(n, P);
        std::vector<RanScalar> scalars(n);
        auto ssum = RanScalar::zero();
        for (size_t j = 0; j < n; j++)
        {
            scalars[j] = random_ran_scalar(rng);
            ssum = ssum + scalars[j];
        }
        auto msm = RanPoint::multi_scalar_mul(scalars.data(), points.data(), n);
        auto expected = P.scalar_mul_vartime(ssum);
        check_true(label.c_str(), ran_points_equal(msm, expected));
    }

    /* All-zero scalars */
    for (int trial = 0; trial < 5; trial++)
    {
        std::string label = "ran_all_zero[" + std::to_string(trial) + "]";
        size_t n = 8;
        std::vector<RanScalar> scalars(n, RanScalar::zero());
        std::vector<RanPoint> points(n);
        for (size_t j = 0; j < n; j++)
            points[j] = random_ran_point(rng);
        auto msm = RanPoint::multi_scalar_mul(scalars.data(), points.data(), n);
        check_true(label.c_str(), msm.is_identity());
    }

    /* Single nonzero in sea of zeros */
    for (int trial = 0; trial < 10; trial++)
    {
        std::string label = "ran_single_nz[" + std::to_string(trial) + "]";
        size_t n = 8;
        std::vector<RanScalar> scalars(n, RanScalar::zero());
        std::vector<RanPoint> points(n);
        for (size_t j = 0; j < n; j++)
            points[j] = random_ran_point(rng);
        size_t idx = (size_t)trial % n;
        scalars[idx] = random_ran_scalar(rng);
        auto msm = RanPoint::multi_scalar_mul(scalars.data(), points.data(), n);
        auto expected = points[idx].scalar_mul_vartime(scalars[idx]);
        check_true(label.c_str(), ran_points_equal(msm, expected));
    }

    /* Shaw - same patterns */
    for (int trial = 0; trial < 20; trial++)
    {
        std::string label = "shaw_sparse[" + std::to_string(trial) + "]";
        size_t n = 8;
        std::vector<ShawScalar> scalars(n);
        std::vector<ShawPoint> points(n);
        for (size_t j = 0; j < n; j++)
        {
            points[j] = random_shaw_point(rng);
            scalars[j] = (j % 3 == 0) ? ShawScalar::zero() : random_shaw_scalar(rng);
        }
        auto msm = ShawPoint::multi_scalar_mul(scalars.data(), points.data(), n);
        auto naive = ShawPoint::identity();
        for (size_t j = 0; j < n; j++)
            naive = naive + points[j].scalar_mul_vartime(scalars[j]);
        check_true((label + " zero_mixed").c_str(), shaw_points_equal(msm, naive));
    }

    for (int trial = 0; trial < 10; trial++)
    {
        std::string label = "shaw_all_one[" + std::to_string(trial) + "]";
        size_t n = 8;
        std::vector<ShawScalar> scalars(n, ShawScalar::one());
        std::vector<ShawPoint> points(n);
        auto sum = ShawPoint::identity();
        for (size_t j = 0; j < n; j++)
        {
            points[j] = random_shaw_point(rng);
            sum = sum + points[j];
        }
        auto msm = ShawPoint::multi_scalar_mul(scalars.data(), points.data(), n);
        check_true(label.c_str(), shaw_points_equal(msm, sum));
    }

    for (int trial = 0; trial < 10; trial++)
    {
        std::string label = "shaw_same_pt[" + std::to_string(trial) + "]";
        size_t n = 8;
        auto P = random_shaw_point(rng);
        std::vector<ShawPoint> points(n, P);
        std::vector<ShawScalar> scalars(n);
        auto ssum = ShawScalar::zero();
        for (size_t j = 0; j < n; j++)
        {
            scalars[j] = random_shaw_scalar(rng);
            ssum = ssum + scalars[j];
        }
        auto msm = ShawPoint::multi_scalar_mul(scalars.data(), points.data(), n);
        auto expected = P.scalar_mul_vartime(ssum);
        check_true(label.c_str(), shaw_points_equal(msm, expected));
    }

    for (int trial = 0; trial < 5; trial++)
    {
        std::string label = "shaw_all_zero[" + std::to_string(trial) + "]";
        size_t n = 8;
        std::vector<ShawScalar> scalars(n, ShawScalar::zero());
        std::vector<ShawPoint> points(n);
        for (size_t j = 0; j < n; j++)
            points[j] = random_shaw_point(rng);
        auto msm = ShawPoint::multi_scalar_mul(scalars.data(), points.data(), n);
        check_true(label.c_str(), msm.is_identity());
    }

    for (int trial = 0; trial < 10; trial++)
    {
        std::string label = "shaw_single_nz[" + std::to_string(trial) + "]";
        size_t n = 8;
        std::vector<ShawScalar> scalars(n, ShawScalar::zero());
        std::vector<ShawPoint> points(n);
        for (size_t j = 0; j < n; j++)
            points[j] = random_shaw_point(rng);
        size_t idx = (size_t)trial % n;
        scalars[idx] = random_shaw_scalar(rng);
        auto msm = ShawPoint::multi_scalar_mul(scalars.data(), points.data(), n);
        auto expected = points[idx].scalar_mul_vartime(scalars[idx]);
        check_true(label.c_str(), shaw_points_equal(msm, expected));
    }
}

/* ======================================================================
 * 10. fuzz_map_to_curve — ~1,000 checks
 * ====================================================================== */
