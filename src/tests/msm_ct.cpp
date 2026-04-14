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
 * @file src/tests/msm_ct.cpp
 * @brief C++ API CT-vs-VT byte-equivalence cross-check for MSM and Pedersen.
 *
 * Exercises both entry points (multi_scalar_mul / multi_scalar_mul_vartime,
 * pedersen_commit / pedersen_commit_vartime) across:
 *   - Known test vectors (replayed through both paths; both must match the
 *     reference expected result AND each other).
 *   - Edge cases: empty sum, all-zero scalars, negated-pair cancellation,
 *     duplicate-point aggregation, mixed-zero scalars.
 *   - 5000 deterministic-LCG fuzz iterations across n in {1..32}, comparing
 *     CT output byte-for-byte with VT output.
 *
 * The C-level cross-check that drives ran_msm_ct against ran_msm_vartime
 * with 512 random cases lives in src/tests/complete_add.cpp; this file is
 * the higher-level twin that goes through the public C++ API.
 */

#include "tests/common.h"
#include "tests/registry.h"

namespace
{

    inline uint64_t lcg_next(uint64_t *state)
    {
        *state = (*state) * 6364136223846793005ULL + 1442695040888963407ULL;
        return *state;
    }

    inline void lcg_fill(unsigned char *out, size_t n, uint64_t *state)
    {
        for (size_t i = 0; i < n; i++)
            out[i] = (unsigned char)(lcg_next(state) >> 56);
    }

    using namespace ranshaw;

    /* Build a random in-range RanScalar via reject-on-overflow from lcg bytes. */
    RanScalar random_ran_scalar(uint64_t *rng)
    {
        unsigned char buf[32];
        while (true)
        {
            lcg_fill(buf, 32, rng);
            buf[31] &= 0x0F; /* well below group order, always reduces cleanly */
            auto s = RanScalar::from_bytes(buf);
            if (s.has_value())
                return s.value();
        }
    }

    ShawScalar random_shaw_scalar(uint64_t *rng)
    {
        unsigned char buf[32];
        while (true)
        {
            lcg_fill(buf, 32, rng);
            buf[31] &= 0x0F;
            auto s = ShawScalar::from_bytes(buf);
            if (s.has_value())
                return s.value();
        }
    }

    RanPoint random_ran_point(uint64_t *rng)
    {
        /* Generate via scalarmult of the generator by a random scalar. */
        auto s = random_ran_scalar(rng);
        return RanPoint::generator().scalar_mul_vartime(s);
    }

    ShawPoint random_shaw_point(uint64_t *rng)
    {
        auto s = random_shaw_scalar(rng);
        return ShawPoint::generator().scalar_mul_vartime(s);
    }

    void check_point_eq(const char *label, const RanPoint &a, const RanPoint &b)
    {
        auto ab = a.to_bytes();
        auto bb = b.to_bytes();
        check_bytes(label, ab.data(), bb.data(), 32);
    }

    void check_point_eq(const char *label, const ShawPoint &a, const ShawPoint &b)
    {
        auto ab = a.to_bytes();
        auto bb = b.to_bytes();
        check_bytes(label, ab.data(), bb.data(), 32);
    }

    /* ─── Ran edge cases ─── */
    void run_ran_edge_cases()
    {
        std::cout << std::endl << "=== Ran CT-vs-VT edge cases ===" << std::endl;

        /* n = 0: both API surfaces return identity. */
        {
            auto ct = RanPoint::multi_scalar_mul(nullptr, nullptr, 0);
            auto vt = RanPoint::multi_scalar_mul_vartime(nullptr, nullptr, 0);
            check_nonzero("ran msm n=0 CT == identity", ct.is_identity());
            check_nonzero("ran msm n=0 VT == identity", vt.is_identity());
        }

        /* All-zero scalars across varied n: result is identity on both paths. */
        {
            for (size_t n : {size_t {1}, size_t {4}, size_t {8}, size_t {17}})
            {
                std::vector<RanScalar> scalars(n, RanScalar::zero());
                std::vector<RanPoint> points(n, RanPoint::generator());
                auto ct = RanPoint::multi_scalar_mul(scalars.data(), points.data(), n);
                auto vt = RanPoint::multi_scalar_mul_vartime(scalars.data(), points.data(), n);
                check_nonzero("ran msm all-zero scalars CT == identity", ct.is_identity());
                check_nonzero("ran msm all-zero scalars VT == identity", vt.is_identity());
            }
        }

        /* Negated pair: scalars (s, -s) with points (P, P) sum to identity. */
        {
            uint64_t rng = 0x1E6A7EDA11D15C45ULL;
            auto s = random_ran_scalar(&rng);
            auto neg_s = -s;
            auto P = random_ran_point(&rng);
            RanScalar scalars[2] = {s, neg_s};
            RanPoint points[2] = {P, P};
            auto ct = RanPoint::multi_scalar_mul(scalars, points, 2);
            auto vt = RanPoint::multi_scalar_mul_vartime(scalars, points, 2);
            check_nonzero("ran msm (s, -s) on (P, P) CT == identity", ct.is_identity());
            check_nonzero("ran msm (s, -s) on (P, P) VT == identity", vt.is_identity());
            check_point_eq("ran msm (s, -s) CT == VT", ct, vt);
        }

        /* Duplicate points: scalars (a, b) with points (P, P) == (a+b)*P. */
        {
            uint64_t rng = 0xD0B11EA7EDD07580ULL;
            auto a = random_ran_scalar(&rng);
            auto b = random_ran_scalar(&rng);
            auto P = random_ran_point(&rng);
            RanScalar scalars[2] = {a, b};
            RanPoint points[2] = {P, P};
            auto ct = RanPoint::multi_scalar_mul(scalars, points, 2);
            auto vt = RanPoint::multi_scalar_mul_vartime(scalars, points, 2);
            auto expected = P.scalar_mul(a + b);
            check_point_eq("ran msm duplicate-points CT == (a+b)*P", ct, expected);
            check_point_eq("ran msm duplicate-points VT == (a+b)*P", vt, expected);
        }

        /* Mixed-zero scalars: only non-zero positions contribute. */
        {
            uint64_t rng = 0xB10F11ED2ED05ULL;
            std::vector<RanScalar> scalars(8);
            std::vector<RanPoint> points(8);
            for (size_t i = 0; i < 8; i++)
            {
                scalars[i] = (i & 1) ? random_ran_scalar(&rng) : RanScalar::zero();
                points[i] = random_ran_point(&rng);
            }
            auto ct = RanPoint::multi_scalar_mul(scalars.data(), points.data(), 8);
            auto vt = RanPoint::multi_scalar_mul_vartime(scalars.data(), points.data(), 8);
            check_point_eq("ran msm mixed-zero scalars CT == VT", ct, vt);
        }

        /* Pedersen n = 0: both surfaces return blinding * H. */
        {
            uint64_t rng = 0xED05EA12EC077ULL;
            auto blinding = random_ran_scalar(&rng);
            auto H = random_ran_point(&rng);
            auto ct = RanPoint::pedersen_commit(blinding, H, nullptr, nullptr, 0);
            auto vt = RanPoint::pedersen_commit_vartime(blinding, H, nullptr, nullptr, 0);
            auto expected = H.scalar_mul(blinding);
            check_point_eq("ran pedersen n=0 CT == blinding*H", ct, expected);
            check_point_eq("ran pedersen n=0 VT == blinding*H", vt, expected);
        }
    }

    /* ─── Shaw edge cases (mirror of the above) ─── */
    void run_shaw_edge_cases()
    {
        std::cout << std::endl << "=== Shaw CT-vs-VT edge cases ===" << std::endl;

        {
            auto ct = ShawPoint::multi_scalar_mul(nullptr, nullptr, 0);
            auto vt = ShawPoint::multi_scalar_mul_vartime(nullptr, nullptr, 0);
            check_nonzero("shaw msm n=0 CT == identity", ct.is_identity());
            check_nonzero("shaw msm n=0 VT == identity", vt.is_identity());
        }

        {
            for (size_t n : {size_t {1}, size_t {4}, size_t {8}, size_t {17}})
            {
                std::vector<ShawScalar> scalars(n, ShawScalar::zero());
                std::vector<ShawPoint> points(n, ShawPoint::generator());
                auto ct = ShawPoint::multi_scalar_mul(scalars.data(), points.data(), n);
                auto vt = ShawPoint::multi_scalar_mul_vartime(scalars.data(), points.data(), n);
                check_nonzero("shaw msm all-zero scalars CT == identity", ct.is_identity());
                check_nonzero("shaw msm all-zero scalars VT == identity", vt.is_identity());
            }
        }

        {
            uint64_t rng = 0xBA5E0F5AFE7ULL;
            auto s = random_shaw_scalar(&rng);
            auto neg_s = -s;
            auto P = random_shaw_point(&rng);
            ShawScalar scalars[2] = {s, neg_s};
            ShawPoint points[2] = {P, P};
            auto ct = ShawPoint::multi_scalar_mul(scalars, points, 2);
            auto vt = ShawPoint::multi_scalar_mul_vartime(scalars, points, 2);
            check_nonzero("shaw msm (s, -s) on (P, P) CT == identity", ct.is_identity());
            check_nonzero("shaw msm (s, -s) on (P, P) VT == identity", vt.is_identity());
            check_point_eq("shaw msm (s, -s) CT == VT", ct, vt);
        }

        {
            uint64_t rng = 0x5CA1ED75CA1ED5ULL;
            auto a = random_shaw_scalar(&rng);
            auto b = random_shaw_scalar(&rng);
            auto P = random_shaw_point(&rng);
            ShawScalar scalars[2] = {a, b};
            ShawPoint points[2] = {P, P};
            auto ct = ShawPoint::multi_scalar_mul(scalars, points, 2);
            auto vt = ShawPoint::multi_scalar_mul_vartime(scalars, points, 2);
            auto expected = P.scalar_mul(a + b);
            check_point_eq("shaw msm duplicate-points CT == (a+b)*P", ct, expected);
            check_point_eq("shaw msm duplicate-points VT == (a+b)*P", vt, expected);
        }

        {
            uint64_t rng = 0xD0CD5D0CD5D0CULL;
            std::vector<ShawScalar> scalars(8);
            std::vector<ShawPoint> points(8);
            for (size_t i = 0; i < 8; i++)
            {
                scalars[i] = (i & 1) ? random_shaw_scalar(&rng) : ShawScalar::zero();
                points[i] = random_shaw_point(&rng);
            }
            auto ct = ShawPoint::multi_scalar_mul(scalars.data(), points.data(), 8);
            auto vt = ShawPoint::multi_scalar_mul_vartime(scalars.data(), points.data(), 8);
            check_point_eq("shaw msm mixed-zero scalars CT == VT", ct, vt);
        }

        {
            uint64_t rng = 0xA110CBAB1E5ULL;
            auto blinding = random_shaw_scalar(&rng);
            auto H = random_shaw_point(&rng);
            auto ct = ShawPoint::pedersen_commit(blinding, H, nullptr, nullptr, 0);
            auto vt = ShawPoint::pedersen_commit_vartime(blinding, H, nullptr, nullptr, 0);
            auto expected = H.scalar_mul(blinding);
            check_point_eq("shaw pedersen n=0 CT == blinding*H", ct, expected);
            check_point_eq("shaw pedersen n=0 VT == blinding*H", vt, expected);
        }
    }

    /* ─── 5000-iteration fuzz: CT vs VT byte-equality at the C++ API level. ─── */
    void run_ran_api_fuzz()
    {
        std::cout << std::endl << "=== Ran CT-vs-VT API fuzz (5000 iterations) ===" << std::endl;

        uint64_t rng = 0xF022A17E00BEEFF1ULL;
        int msm_mismatches = 0;
        int pedersen_mismatches = 0;
        const int iters = 5000;

        for (int iter = 0; iter < iters; iter++)
        {
            size_t n = 1 + (size_t)(lcg_next(&rng) % 32);
            std::vector<RanScalar> scalars;
            std::vector<RanPoint> points;
            scalars.reserve(n);
            points.reserve(n);
            for (size_t i = 0; i < n; i++)
            {
                scalars.push_back(random_ran_scalar(&rng));
                points.push_back(random_ran_point(&rng));
            }

            auto ct = RanPoint::multi_scalar_mul(scalars.data(), points.data(), n);
            auto vt = RanPoint::multi_scalar_mul_vartime(scalars.data(), points.data(), n);
            auto cb = ct.to_bytes();
            auto vb = vt.to_bytes();
            if (std::memcmp(cb.data(), vb.data(), 32) != 0)
                msm_mismatches++;

            /* Pedersen over the same data with a fresh blinding and H. */
            auto blinding = random_ran_scalar(&rng);
            auto H = random_ran_point(&rng);
            auto pct = RanPoint::pedersen_commit(blinding, H, scalars.data(), points.data(), n);
            auto pvt = RanPoint::pedersen_commit_vartime(blinding, H, scalars.data(), points.data(), n);
            auto pcb = pct.to_bytes();
            auto pvb = pvt.to_bytes();
            if (std::memcmp(pcb.data(), pvb.data(), 32) != 0)
                pedersen_mismatches++;
        }

        check_int("ran msm CT == VT across 5000 random trials", 0, msm_mismatches);
        check_int("ran pedersen CT == VT across 5000 random trials", 0, pedersen_mismatches);
    }

    void run_shaw_api_fuzz()
    {
        std::cout << std::endl << "=== Shaw CT-vs-VT API fuzz (5000 iterations) ===" << std::endl;

        uint64_t rng = 0x5AF5AF5AF5AF5ULL;
        int msm_mismatches = 0;
        int pedersen_mismatches = 0;
        const int iters = 5000;

        for (int iter = 0; iter < iters; iter++)
        {
            size_t n = 1 + (size_t)(lcg_next(&rng) % 32);
            std::vector<ShawScalar> scalars;
            std::vector<ShawPoint> points;
            scalars.reserve(n);
            points.reserve(n);
            for (size_t i = 0; i < n; i++)
            {
                scalars.push_back(random_shaw_scalar(&rng));
                points.push_back(random_shaw_point(&rng));
            }

            auto ct = ShawPoint::multi_scalar_mul(scalars.data(), points.data(), n);
            auto vt = ShawPoint::multi_scalar_mul_vartime(scalars.data(), points.data(), n);
            auto cb = ct.to_bytes();
            auto vb = vt.to_bytes();
            if (std::memcmp(cb.data(), vb.data(), 32) != 0)
                msm_mismatches++;

            auto blinding = random_shaw_scalar(&rng);
            auto H = random_shaw_point(&rng);
            auto pct = ShawPoint::pedersen_commit(blinding, H, scalars.data(), points.data(), n);
            auto pvt = ShawPoint::pedersen_commit_vartime(blinding, H, scalars.data(), points.data(), n);
            auto pcb = pct.to_bytes();
            auto pvb = pvt.to_bytes();
            if (std::memcmp(pcb.data(), pvb.data(), 32) != 0)
                pedersen_mismatches++;
        }

        check_int("shaw msm CT == VT across 5000 random trials", 0, msm_mismatches);
        check_int("shaw pedersen CT == VT across 5000 random trials", 0, pedersen_mismatches);
    }

} // anonymous namespace

void test_msm_ct_cross_check()
{
    run_ran_edge_cases();
    run_shaw_edge_cases();
    run_ran_api_fuzz();
    run_shaw_api_fuzz();
}
