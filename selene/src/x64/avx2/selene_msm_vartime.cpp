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
 * @file x64/avx2/selene_msm_vartime.cpp
 * @brief AVX2 multi-scalar multiplication for Selene: Straus (n<=32) with 4-way parallel
 *        fq10x4 point operations, and Pippenger (n>32) with scalar fq51 point operations.
 */

#include "selene_msm_vartime.h"

#include "fq_mul.h"
#include "fq_ops.h"
#include "fq_sq.h"
#include "fq_utils.h"
#include "selene.h"
#include "selene_ops.h"
#include "x64/avx2/fq10_avx2.h"
#include "x64/avx2/fq10x4_avx2.h"
#include "x64/avx2/selene_avx2.h"
#include "x64/selene_add.h"
#include "x64/selene_dbl.h"

#include <cstring>
#include <vector>

// ============================================================================
// Safe variable-time addition for Jacobian coordinates (fq51)
// ============================================================================

/*
 * Variable-time "safe" addition that handles all edge cases:
 * - p == identity: return q
 * - q == identity: return p
 * - p == q: use doubling
 * - p == -q: return identity
 * - otherwise: standard addition
 *
 * Uses the x64 baseline selene_dbl_x64/selene_add_x64 for scalar point ops.
 */
static void selene_add_safe(selene_jacobian *r, const selene_jacobian *p, const selene_jacobian *q)
{
    if (selene_is_identity(p))
    {
        selene_copy(r, q);
        return;
    }
    if (selene_is_identity(q))
    {
        selene_copy(r, p);
        return;
    }

    /* Check if x-coordinates match (projective comparison) */
    fq_fe z1z1, z2z2, u1, u2, diff;
    fq_sq(z1z1, p->Z);
    fq_sq(z2z2, q->Z);
    fq_mul(u1, p->X, z2z2);
    fq_mul(u2, q->X, z1z1);
    fq_sub(diff, u1, u2);

    if (!fq_isnonzero(diff))
    {
        /* Same x: check if same or opposite y */
        fq_fe s1, s2, t;
        fq_mul(t, q->Z, z2z2);
        fq_mul(s1, p->Y, t);
        fq_mul(t, p->Z, z1z1);
        fq_mul(s2, q->Y, t);
        fq_sub(diff, s1, s2);

        if (!fq_isnonzero(diff))
        {
            /* P == Q: double */
            selene_dbl_x64(r, p);
        }
        else
        {
            /* P == -Q: identity */
            selene_identity(r);
        }
        return;
    }

    selene_add_x64(r, p, q);
}

// ============================================================================
// Signed digit encoding (curve-independent)
// ============================================================================

static void encode_signed_w4(signed char *digits, const unsigned char *scalar)
{
    int carry = 0;
    for (int i = 0; i < 31; i++)
    {
        carry += scalar[i];
        int carry2 = (carry + 8) >> 4;
        digits[2 * i] = static_cast<signed char>(carry - (carry2 << 4));
        carry = (carry2 + 8) >> 4;
        digits[2 * i + 1] = static_cast<signed char>(carry2 - (carry << 4));
    }
    carry += scalar[31];
    int carry2 = (carry + 8) >> 4;
    digits[62] = static_cast<signed char>(carry - (carry2 << 4));
    digits[63] = static_cast<signed char>(carry2);
}

static int encode_signed_wbit(signed char *digits, const unsigned char *scalar, int w)
{
    const int half = 1 << (w - 1);
    const int mask = (1 << w) - 1;
    const int num_digits = (256 + w - 1) / w;

    int carry = 0;
    for (int i = 0; i < num_digits; i++)
    {
        int bit_pos = i * w;
        int byte_pos = bit_pos / 8;
        int bit_off = bit_pos % 8;

        int raw = 0;
        if (byte_pos < 32)
            raw = scalar[byte_pos] >> bit_off;
        if (byte_pos + 1 < 32 && bit_off + w > 8)
            raw |= static_cast<int>(scalar[byte_pos + 1]) << (8 - bit_off);
        if (byte_pos + 2 < 32 && bit_off + w > 16)
            raw |= static_cast<int>(scalar[byte_pos + 2]) << (16 - bit_off);

        int val = (raw & mask) + carry;
        carry = val >> w;
        val &= mask;

        if (val >= half)
        {
            val -= (1 << w);
            carry = 1;
        }

        digits[i] = static_cast<signed char>(val);
    }

    return num_digits;
}

// ============================================================================
// 4-Way Straus (interleaved) method -- used for n <= 32
// ============================================================================

/*
 * Process groups of 4 scalars using AVX2 4-way parallel Jacobian point ops.
 * Each group of 4 scalars shares one 4-way accumulator, using fq10x4 arithmetic
 * for doubling and addition. Table lookups use per-lane conditional moves.
 */
static void
    msm_straus_avx2(selene_jacobian *result, const unsigned char *scalars, const selene_jacobian *points, size_t n)
{
    // Encode all scalars into signed 4-bit digits
    std::vector<signed char> all_digits(n * 64);
    for (size_t i = 0; i < n; i++)
    {
        encode_signed_w4(all_digits.data() + i * 64, scalars + i * 32);
    }

    // Precompute tables: table[i][j] = (j+1) * points[i], in Jacobian (fq51)
    std::vector<selene_jacobian> tables(n * 8);
    for (size_t i = 0; i < n; i++)
    {
        selene_jacobian *Ti = tables.data() + i * 8;
        selene_copy(&Ti[0], &points[i]); // Ti[0] = 1*P
        selene_dbl_x64(&Ti[1], &points[i]); // Ti[1] = 2*P
        for (int j = 1; j < 7; j++)
        {
            selene_add_safe(&Ti[j + 1], &Ti[j], &points[i]); // Ti[j+1] = (j+2)*P
        }
    }

    // Process groups of 4 scalars using 4-way parallel ops
    const size_t num_groups = (n + 3) / 4;

    // Build 4-way precomputed tables: tables_4x[g*8+j] = packed table entry j for group g
    std::vector<selene_jacobian_4x> tables_4x(num_groups * 8);
    for (size_t g = 0; g < num_groups; g++)
    {
        for (int j = 0; j < 8; j++)
        {
            selene_jacobian id;
            selene_identity(&id);

            const selene_jacobian *p0 = (g * 4 + 0 < n) ? &tables[(g * 4 + 0) * 8 + j] : &id;
            const selene_jacobian *p1 = (g * 4 + 1 < n) ? &tables[(g * 4 + 1) * 8 + j] : &id;
            const selene_jacobian *p2 = (g * 4 + 2 < n) ? &tables[(g * 4 + 2) * 8 + j] : &id;
            const selene_jacobian *p3 = (g * 4 + 3 < n) ? &tables[(g * 4 + 3) * 8 + j] : &id;
            selene_pack_4x(&tables_4x[g * 8 + j], p0, p1, p2, p3);
        }
    }

    // Main loop with per-lane start tracking for identity protection.
    std::vector<selene_jacobian_4x> accum(num_groups);
    std::vector<uint8_t> lane_started(num_groups, 0);

    for (int d = 63; d >= 0; d--)
    {
        for (size_t g = 0; g < num_groups; g++)
        {
            if (lane_started[g])
            {
                selene_dbl_4x(&accum[g], &accum[g]);
                selene_dbl_4x(&accum[g], &accum[g]);
                selene_dbl_4x(&accum[g], &accum[g]);
                selene_dbl_4x(&accum[g], &accum[g]);
            }
        }

        for (size_t g = 0; g < num_groups; g++)
        {
            signed char d0 = (g * 4 + 0 < n) ? all_digits[(g * 4 + 0) * 64 + d] : 0;
            signed char d1 = (g * 4 + 1 < n) ? all_digits[(g * 4 + 1) * 64 + d] : 0;
            signed char d2 = (g * 4 + 2 < n) ? all_digits[(g * 4 + 2) * 64 + d] : 0;
            signed char d3 = (g * 4 + 3 < n) ? all_digits[(g * 4 + 3) * 64 + d] : 0;

            if (d0 == 0 && d1 == 0 && d2 == 0 && d3 == 0)
                continue;

            signed char digits[4] = {d0, d1, d2, d3};
            unsigned int abs_d[4], neg[4];
            uint8_t nonzero_mask = 0;
            for (int k = 0; k < 4; k++)
            {
                abs_d[k] = static_cast<unsigned int>((digits[k] < 0) ? -digits[k] : digits[k]);
                neg[k] = (digits[k] < 0) ? 1u : 0u;
                if (digits[k] != 0)
                    nonzero_mask |= static_cast<uint8_t>(1 << k);
            }

            selene_jacobian_4x selected;
            selene_identity_4x(&selected);

            for (int j = 0; j < 8; j++)
            {
                int64_t m0 = -static_cast<int64_t>(abs_d[0] == static_cast<unsigned int>(j + 1));
                int64_t m1 = -static_cast<int64_t>(abs_d[1] == static_cast<unsigned int>(j + 1));
                int64_t m2 = -static_cast<int64_t>(abs_d[2] == static_cast<unsigned int>(j + 1));
                int64_t m3 = -static_cast<int64_t>(abs_d[3] == static_cast<unsigned int>(j + 1));
                __m256i mask = _mm256_set_epi64x(m3, m2, m1, m0);
                selene_cmov_4x(&selected, &tables_4x[g * 8 + j], mask);
            }

            {
                selene_jacobian_4x neg_sel;
                selene_neg_4x(&neg_sel, &selected);
                int64_t nm0 = -static_cast<int64_t>(neg[0]);
                int64_t nm1 = -static_cast<int64_t>(neg[1]);
                int64_t nm2 = -static_cast<int64_t>(neg[2]);
                int64_t nm3 = -static_cast<int64_t>(neg[3]);
                __m256i neg_mask = _mm256_set_epi64x(nm3, nm2, nm1, nm0);
                for (int k = 0; k < 10; k++)
                    selected.Y.v[k] = _mm256_blendv_epi8(selected.Y.v[k], neg_sel.Y.v[k], neg_mask);
            }

            uint8_t first_time = nonzero_mask & ~lane_started[g];
            uint8_t need_add = nonzero_mask & lane_started[g];

            if (need_add)
            {
                selene_jacobian_4x saved;
                selene_copy_4x(&saved, &accum[g]);
                selene_add_4x(&accum[g], &accum[g], &selected);
                uint8_t zero_lanes = lane_started[g] & ~nonzero_mask;
                if (zero_lanes)
                {
                    __m256i zmask = _mm256_set_epi64x(
                        (zero_lanes & 8) ? -1LL : 0LL,
                        (zero_lanes & 4) ? -1LL : 0LL,
                        (zero_lanes & 2) ? -1LL : 0LL,
                        (zero_lanes & 1) ? -1LL : 0LL);
                    selene_cmov_4x(&accum[g], &saved, zmask);
                }
            }

            if (first_time)
            {
                __m256i fmask = _mm256_set_epi64x(
                    (first_time & 8) ? -1LL : 0LL,
                    (first_time & 4) ? -1LL : 0LL,
                    (first_time & 2) ? -1LL : 0LL,
                    (first_time & 1) ? -1LL : 0LL);
                selene_cmov_4x(&accum[g], &selected, fmask);
            }

            lane_started[g] |= nonzero_mask;
        }
    }

    // Combine results from all groups
    selene_jacobian total;
    selene_identity(&total);
    bool total_started = false;

    for (size_t g = 0; g < num_groups; g++)
    {
        if (!lane_started[g])
            continue;

        selene_jacobian p0, p1, p2, p3;
        selene_unpack_4x(&p0, &p1, &p2, &p3, &accum[g]);

        selene_jacobian parts[4] = {p0, p1, p2, p3};
        for (int k = 0; k < 4 && g * 4 + static_cast<size_t>(k) < n; k++)
        {
            if (selene_is_identity(&parts[k]))
                continue;
            if (!total_started)
            {
                selene_copy(&total, &parts[k]);
                total_started = true;
            }
            else
            {
                selene_add_safe(&total, &total, &parts[k]);
            }
        }
    }

    if (total_started)
        selene_copy(result, &total);
    else
        selene_identity(result);
}

// ============================================================================
// Pippenger (bucket method) -- used for n > 32
// ============================================================================

/*
 * Pippenger uses scalar fq51 point operations for bucket accumulation.
 * The bucket-based approach doesn't benefit from 4-way grouping because
 * each point goes into a different bucket, so there's no parallelism to exploit.
 */

static int pippenger_window_size(size_t n)
{
    if (n < 96)
        return 5;
    if (n < 288)
        return 6;
    if (n < 864)
        return 7;
    if (n < 2592)
        return 8;
    if (n < 7776)
        return 9;
    if (n < 23328)
        return 10;
    return 11;
}

static void
    msm_pippenger_avx2(selene_jacobian *result, const unsigned char *scalars, const selene_jacobian *points, size_t n)
{
    const int w = pippenger_window_size(n);
    const int num_buckets = (1 << (w - 1));
    const int num_windows = (256 + w - 1) / w;

    std::vector<signed char> all_digits(n * num_windows);
    for (size_t i = 0; i < n; i++)
    {
        encode_signed_wbit(all_digits.data() + i * num_windows, scalars + i * 32, w);
    }

    selene_jacobian total;
    selene_identity(&total);
    bool total_is_identity = true;

    for (int win = num_windows - 1; win >= 0; win--)
    {
        // Horner step: multiply accumulated result by 2^w
        if (!total_is_identity)
        {
            for (int dd = 0; dd < w; dd++)
                selene_dbl_x64(&total, &total);
        }

        // Initialize buckets
        std::vector<selene_jacobian> bucket_points(num_buckets);
        std::vector<bool> bucket_is_identity(num_buckets, true);

        // Distribute points into buckets
        for (size_t i = 0; i < n; i++)
        {
            signed char digit = all_digits[i * num_windows + win];
            if (digit == 0)
                continue;

            int bucket_idx;
            selene_jacobian effective_point;

            if (digit > 0)
            {
                bucket_idx = digit - 1;
                selene_copy(&effective_point, &points[i]);
            }
            else
            {
                bucket_idx = (-digit) - 1;
                selene_neg(&effective_point, &points[i]);
            }

            if (bucket_is_identity[bucket_idx])
            {
                selene_copy(&bucket_points[bucket_idx], &effective_point);
                bucket_is_identity[bucket_idx] = false;
            }
            else
            {
                selene_add_safe(&bucket_points[bucket_idx], &bucket_points[bucket_idx], &effective_point);
            }
        }

        // Running-sum combination
        selene_jacobian running;
        bool running_is_identity = true;

        selene_jacobian partial;
        bool partial_is_identity = true;

        for (int j = num_buckets - 1; j >= 0; j--)
        {
            if (!bucket_is_identity[j])
            {
                if (running_is_identity)
                {
                    selene_copy(&running, &bucket_points[j]);
                    running_is_identity = false;
                }
                else
                {
                    selene_add_safe(&running, &running, &bucket_points[j]);
                }
            }

            if (!running_is_identity)
            {
                if (partial_is_identity)
                {
                    selene_copy(&partial, &running);
                    partial_is_identity = false;
                }
                else
                {
                    selene_add_safe(&partial, &partial, &running);
                }
            }
        }

        // Add this window's result to total
        if (!partial_is_identity)
        {
            if (total_is_identity)
            {
                selene_copy(&total, &partial);
                total_is_identity = false;
            }
            else
            {
                selene_add_safe(&total, &total, &partial);
            }
        }
    }

    if (total_is_identity)
        selene_identity(result);
    else
        selene_copy(result, &total);
}

// ============================================================================
// Public API (AVX2)
// ============================================================================

static const size_t STRAUS_PIPPENGER_CROSSOVER = 16;

void selene_msm_vartime_avx2(
    selene_jacobian *result,
    const unsigned char *scalars,
    const selene_jacobian *points,
    size_t n)
{
    if (n == 0)
    {
        selene_identity(result);
        return;
    }

    if (n <= STRAUS_PIPPENGER_CROSSOVER)
    {
        msm_straus_avx2(result, scalars, points, n);
    }
    else
    {
        msm_pippenger_avx2(result, scalars, points, n);
    }
}
