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

#include "fq_ops.h"
#include "fuzz_tests/common.h"
#include "fuzz_tests/registry.h"
#include "ranshaw_platform.h"
#include "shaw.h"
#include "shaw_constants.h"
#include "shaw_ops.h"
#include "shaw_scalarmult_vartime.h"
#include "x64/shaw_msm_vartime.h"

/*
 * Differential fuzz for shaw_msm_vartime_x64: compare the packed 4x64
 * bucket rewrite against the always-compiled 5x51 unpacked reference on
 * toolchains where both are present. On MSVC / non-BMI2 / portable builds
 * the two entry points alias, so this harness is a no-op (skipped).
 *
 * Sizes span the Straus/Pippenger crossover (8) and the Pippenger window
 * steps (5 -> 6 -> 7 at n=96, 288, 864). Adversarial inputs targeting the
 * edge cases of shaw_add_safe_packed (self-add and add-of-negation inside
 * bucket / running-sum loops) are included up front before the random
 * sweep.
 */

#if RANSHAW_PLATFORM_64BIT && (defined(__GNUC__) || defined(__clang__)) && defined(__BMI2__) && RANSHAW_HAVE_INT128
#define RANSHAW_MSM_PACKED_DIFF_ACTIVE 1
#endif

#ifdef RANSHAW_MSM_PACKED_DIFF_ACTIVE

extern "C" void shaw_msm_vartime_x64_unpacked(
    shaw_jacobian *result,
    const unsigned char *scalars,
    const shaw_jacobian *points,
    size_t n);

static bool compare_msm(const char *label, const unsigned char *scalars, const shaw_jacobian *points, size_t n)
{
    shaw_jacobian got, want;
    shaw_msm_vartime_x64(&got, scalars, points, n);
    shaw_msm_vartime_x64_unpacked(&want, scalars, points, n);

    unsigned char got_bytes[32], want_bytes[32];
    shaw_tobytes(got_bytes, &got);
    shaw_tobytes(want_bytes, &want);
    return check_bytes(label, want_bytes, got_bytes, 32);
}

static void make_random_scalar(unsigned char out[32], xoshiro256ss &rng)
{
    rng.fill_bytes(out, 32);
    out[31] &= 0x7F;
}

static void make_generator(shaw_jacobian *g)
{
    fq_copy(g->X, SHAW_GX);
    fq_copy(g->Y, SHAW_GY);
    fq_1(g->Z);
}

/* Generate a random on-curve Shaw point as a random-scalar multiple of
 * the generator. Cheaper and cleaner than map-to-curve for fuzz inputs. */
static void make_random_on_curve_point(shaw_jacobian *p, const shaw_jacobian *g, xoshiro256ss &rng)
{
    unsigned char s[32];
    make_random_scalar(s, rng);
    shaw_scalarmult_vartime(p, s, g);
}

#endif /* RANSHAW_MSM_PACKED_DIFF_ACTIVE */

void fuzz_msm_vartime_packed_diff()
{
    std::cout << std::endl << "=== Fuzz: shaw_msm_vartime packed vs unpacked diff ===" << std::endl;

#ifndef RANSHAW_MSM_PACKED_DIFF_ACTIVE
    std::cout << "(skipped: FQ51_HAVE_ADX_MUL not defined on this toolchain; packed and unpacked paths coincide)"
              << std::endl;
    return;
#else
    xoshiro256ss rng;
    rng.seed(global_seed + 293);

    shaw_jacobian G;
    make_generator(&G);

    /* Edge cases driven by the shaw_add_safe_packed branches. */

    /* n=8 Straus: all scalars=1, all points=G. First nonzero digit window
     * sets acc=G, next add produces G+G (self-double), exercising the
     * doubling branch of shaw_add_safe_packed. */
    {
        unsigned char scalars[8 * 32] = {};
        shaw_jacobian points[8];
        for (int i = 0; i < 8; i++)
        {
            scalars[i * 32] = 0x01;
            shaw_copy(&points[i], &G);
        }
        compare_msm("edge: n=8 all_ones_all_G (self-double)", scalars, points, 8);
    }

    /* n=33 Pippenger crossover: all scalars=1, all points=G. Bucket 0
     * receives G 33 times; step 2 triggers doubling. */
    {
        unsigned char scalars[33 * 32] = {};
        shaw_jacobian points[33];
        for (int i = 0; i < 33; i++)
        {
            scalars[i * 32] = 0x01;
            shaw_copy(&points[i], &G);
        }
        compare_msm("edge: n=33 all_ones_all_G (Pippenger self-double)", scalars, points, 33);
    }

    /* n=2 self-negate: scalars=[1, p-1], points=[G, G] sums to 0*G =
     * identity, exercising the X-collision Y-differ branch of
     * shaw_add_safe_packed. p is the Ran base field prime 2^255 - 19
     * (Shaw's group order). */
    {
        unsigned char scalars[2 * 32] = {};
        scalars[0] = 0x01;
        static const unsigned char P_MINUS_1[32] = {
            0xEC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F,
        };
        std::memcpy(scalars + 32, P_MINUS_1, 32);
        shaw_jacobian points[2];
        shaw_copy(&points[0], &G);
        shaw_copy(&points[1], &G);
        compare_msm("edge: n=2 [1,p-1] both G (self-negate)", scalars, points, 2);
    }

    /* All-identity points (pathological but valid). */
    {
        unsigned char scalars[4 * 32];
        rng.fill_bytes(scalars, 4 * 32);
        for (int i = 0; i < 4; i++)
            scalars[i * 32 + 31] &= 0x7F;
        shaw_jacobian points[4];
        for (int i = 0; i < 4; i++)
            shaw_identity(&points[i]);
        compare_msm("edge: n=4 all_identity_points", scalars, points, 4);
    }

    /* All-zero scalars -> identity result. */
    {
        unsigned char scalars[16 * 32] = {};
        shaw_jacobian points[16];
        for (int i = 0; i < 16; i++)
        {
            shaw_copy(&points[i], &G);
        }
        compare_msm("edge: n=16 all_zero_scalars", scalars, points, 16);
    }

    /* Random sweep across Straus, Straus/Pippenger crossover, and
     * Pippenger window-size steps. 8 iterations per size keeps wall-clock
     * modest while still touching the code paths. */
    const size_t sizes[] = {1, 2, 3, 7, 8, 9, 16, 32, 33, 64, 96, 128, 256, 288};
    const size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t s = 0; s < num_sizes; s++)
    {
        const size_t n = sizes[s];
        for (int iter = 0; iter < 8; iter++)
        {
            std::vector<unsigned char> scalars(n * 32);
            std::vector<shaw_jacobian> points(n);

            for (size_t i = 0; i < n; i++)
            {
                make_random_scalar(scalars.data() + i * 32, rng);
                make_random_on_curve_point(&points[i], &G, rng);
            }

            std::string label = "msm_packed_diff[n=" + std::to_string(n) + ", iter=" + std::to_string(iter) + "]";
            compare_msm(label.c_str(), scalars.data(), points.data(), n);
        }
    }
#endif
}
