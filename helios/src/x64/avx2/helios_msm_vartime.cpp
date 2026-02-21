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
 * @file x64/avx2/helios_msm_vartime.cpp
 * @brief AVX2 multi-scalar multiplication for Helios: Straus (n<=32) with 4-way parallel
 *        fp10x4 point operations, and Pippenger (n>32) with scalar fp51 point operations.
 */

#include "helios_msm_vartime.h"

#include "fp_mul.h"
#include "fp_ops.h"
#include "fp_sq.h"
#include "fp_utils.h"
#include "helios.h"
#include "helios_ops.h"
#include "x64/avx2/fp10_avx2.h"
#include "x64/avx2/fp10x4_avx2.h"
#include "x64/avx2/helios_avx2.h"
#include "x64/helios_add.h"
#include "x64/helios_dbl.h"

#include <cstring>
#include <vector>

// ============================================================================
// Safe variable-time addition for Jacobian coordinates (fp51)
// ============================================================================

/*
 * Variable-time "safe" addition that handles all edge cases:
 * - p == identity: return q
 * - q == identity: return p
 * - p == q: use doubling
 * - p == -q: return identity
 * - otherwise: standard addition
 *
 * Uses the x64 baseline helios_dbl_x64/helios_add_x64 for scalar point ops.
 */
static void helios_add_safe(helios_jacobian *r, const helios_jacobian *p, const helios_jacobian *q)
{
    if (helios_is_identity(p))
    {
        helios_copy(r, q);
        return;
    }
    if (helios_is_identity(q))
    {
        helios_copy(r, p);
        return;
    }

    /* Check if x-coordinates match (projective comparison) */
    fp_fe z1z1, z2z2, u1, u2, diff;
    fp_sq(z1z1, p->Z);
    fp_sq(z2z2, q->Z);
    fp_mul(u1, p->X, z2z2);
    fp_mul(u2, q->X, z1z1);
    fp_sub(diff, u1, u2);

    if (!fp_isnonzero(diff))
    {
        /* Same x: check if same or opposite y */
        fp_fe s1, s2, t;
        fp_mul(t, q->Z, z2z2);
        fp_mul(s1, p->Y, t);
        fp_mul(t, p->Z, z1z1);
        fp_mul(s2, q->Y, t);
        fp_sub(diff, s1, s2);

        if (!fp_isnonzero(diff))
        {
            /* P == Q: double */
            helios_dbl_x64(r, p);
        }
        else
        {
            /* P == -Q: identity */
            helios_identity(r);
        }
        return;
    }

    helios_add_x64(r, p, q);
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
 * Each group of 4 scalars shares one 4-way accumulator, using fp10x4 arithmetic
 * for doubling and addition. Table lookups use per-lane conditional moves.
 */
static void
    msm_straus_avx2(helios_jacobian *result, const unsigned char *scalars, const helios_jacobian *points, size_t n)
{
    // Encode all scalars into signed 4-bit digits
    std::vector<signed char> all_digits(n * 64);
    for (size_t i = 0; i < n; i++)
    {
        encode_signed_w4(all_digits.data() + i * 64, scalars + i * 32);
    }

    // Precompute tables: table[i][j] = (j+1) * points[i], in Jacobian (fp51)
    std::vector<helios_jacobian> tables(n * 8);
    for (size_t i = 0; i < n; i++)
    {
        helios_jacobian *Ti = tables.data() + i * 8;
        helios_copy(&Ti[0], &points[i]); // Ti[0] = 1*P
        helios_dbl_x64(&Ti[1], &points[i]); // Ti[1] = 2*P
        for (int j = 1; j < 7; j++)
        {
            helios_add_safe(&Ti[j + 1], &Ti[j], &points[i]); // Ti[j+1] = (j+2)*P
        }
    }

    // Process groups of 4 scalars using 4-way parallel ops
    const size_t num_groups = (n + 3) / 4;

    // Build 4-way precomputed tables: tables_4x[g*8+j] = packed table entry j for group g
    std::vector<helios_jacobian_4x> tables_4x(num_groups * 8);
    for (size_t g = 0; g < num_groups; g++)
    {
        for (int j = 0; j < 8; j++)
        {
            helios_jacobian id;
            helios_identity(&id);

            const helios_jacobian *p0 = (g * 4 + 0 < n) ? &tables[(g * 4 + 0) * 8 + j] : &id;
            const helios_jacobian *p1 = (g * 4 + 1 < n) ? &tables[(g * 4 + 1) * 8 + j] : &id;
            const helios_jacobian *p2 = (g * 4 + 2 < n) ? &tables[(g * 4 + 2) * 8 + j] : &id;
            const helios_jacobian *p3 = (g * 4 + 3 < n) ? &tables[(g * 4 + 3) * 8 + j] : &id;
            helios_pack_4x(&tables_4x[g * 8 + j], p0, p1, p2, p3);
        }
    }

    // Main loop: process digit positions from MSB to LSB
    // Each group has its own 4-way accumulator with per-lane start tracking.
    // The raw helios_add_4x formula corrupts lanes where either input has Z=0
    // (identity), so we must use cmov to protect those lanes.
    std::vector<helios_jacobian_4x> accum(num_groups);
    std::vector<uint8_t> lane_started(num_groups, 0);

    for (int d = 63; d >= 0; d--)
    {
        // 4 doublings on all started accumulators
        for (size_t g = 0; g < num_groups; g++)
        {
            if (lane_started[g])
            {
                helios_dbl_4x(&accum[g], &accum[g]);
                helios_dbl_4x(&accum[g], &accum[g]);
                helios_dbl_4x(&accum[g], &accum[g]);
                helios_dbl_4x(&accum[g], &accum[g]);
            }
        }

        // Add contributions from each group
        for (size_t g = 0; g < num_groups; g++)
        {
            // Get the 4 digits for this group at digit position d
            signed char d0 = (g * 4 + 0 < n) ? all_digits[(g * 4 + 0) * 64 + d] : 0;
            signed char d1 = (g * 4 + 1 < n) ? all_digits[(g * 4 + 1) * 64 + d] : 0;
            signed char d2 = (g * 4 + 2 < n) ? all_digits[(g * 4 + 2) * 64 + d] : 0;
            signed char d3 = (g * 4 + 3 < n) ? all_digits[(g * 4 + 3) * 64 + d] : 0;

            if (d0 == 0 && d1 == 0 && d2 == 0 && d3 == 0)
                continue;

            // For each lane, compute |digit| and sign
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

            // Per-lane table selection using conditional moves
            helios_jacobian_4x selected;
            helios_identity_4x(&selected);

            for (int j = 0; j < 8; j++)
            {
                // Build per-lane mask: lane k selected if abs_d[k] == j+1
                int64_t m0 = -static_cast<int64_t>(abs_d[0] == static_cast<unsigned int>(j + 1));
                int64_t m1 = -static_cast<int64_t>(abs_d[1] == static_cast<unsigned int>(j + 1));
                int64_t m2 = -static_cast<int64_t>(abs_d[2] == static_cast<unsigned int>(j + 1));
                int64_t m3 = -static_cast<int64_t>(abs_d[3] == static_cast<unsigned int>(j + 1));
                __m256i mask = _mm256_set_epi64x(m3, m2, m1, m0);
                helios_cmov_4x(&selected, &tables_4x[g * 8 + j], mask);
            }

            // Per-lane conditional negate: negate Y for lanes where digit was negative
            {
                helios_jacobian_4x neg_sel;
                helios_neg_4x(&neg_sel, &selected);
                int64_t nm0 = -static_cast<int64_t>(neg[0]);
                int64_t nm1 = -static_cast<int64_t>(neg[1]);
                int64_t nm2 = -static_cast<int64_t>(neg[2]);
                int64_t nm3 = -static_cast<int64_t>(neg[3]);
                __m256i neg_mask = _mm256_set_epi64x(nm3, nm2, nm1, nm0);
                // Blend Y coordinate: for lanes where neg, use negated Y
                for (int k = 0; k < 10; k++)
                    selected.Y.v[k] = _mm256_blendv_epi8(selected.Y.v[k], neg_sel.Y.v[k], neg_mask);
            }

            // Accumulate with per-lane identity protection
            uint8_t first_time = nonzero_mask & ~lane_started[g];
            uint8_t need_add = nonzero_mask & lane_started[g];

            if (need_add)
            {
                helios_jacobian_4x saved;
                helios_copy_4x(&saved, &accum[g]);
                helios_add_4x(&accum[g], &accum[g], &selected);
                // Restore accumulator for lanes where digit was 0
                uint8_t zero_lanes = lane_started[g] & ~nonzero_mask;
                if (zero_lanes)
                {
                    __m256i zmask = _mm256_set_epi64x(
                        (zero_lanes & 8) ? -1LL : 0LL, (zero_lanes & 4) ? -1LL : 0LL,
                        (zero_lanes & 2) ? -1LL : 0LL, (zero_lanes & 1) ? -1LL : 0LL);
                    helios_cmov_4x(&accum[g], &saved, zmask);
                }
            }

            if (first_time)
            {
                __m256i fmask = _mm256_set_epi64x(
                    (first_time & 8) ? -1LL : 0LL, (first_time & 4) ? -1LL : 0LL,
                    (first_time & 2) ? -1LL : 0LL, (first_time & 1) ? -1LL : 0LL);
                helios_cmov_4x(&accum[g], &selected, fmask);
            }

            lane_started[g] |= nonzero_mask;
        }
    }

    // Combine results from all groups: unpack each 4-way accumulator
    // and add the partial results together using scalar fp51 ops
    helios_jacobian total;
    helios_identity(&total);
    bool total_started = false;

    for (size_t g = 0; g < num_groups; g++)
    {
        if (!lane_started[g])
            continue;

        helios_jacobian p0, p1, p2, p3;
        helios_unpack_4x(&p0, &p1, &p2, &p3, &accum[g]);

        helios_jacobian parts[4] = {p0, p1, p2, p3};
        for (int k = 0; k < 4 && g * 4 + static_cast<size_t>(k) < n; k++)
        {
            if (helios_is_identity(&parts[k]))
                continue;
            if (!total_started)
            {
                helios_copy(&total, &parts[k]);
                total_started = true;
            }
            else
            {
                helios_add_safe(&total, &total, &parts[k]);
            }
        }
    }

    if (total_started)
        helios_copy(result, &total);
    else
        helios_identity(result);
}

// ============================================================================
// Pippenger (bucket method) -- used for n > 32
// ============================================================================

/*
 * Pippenger uses scalar fp51 point operations for bucket accumulation.
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
    msm_pippenger_avx2(helios_jacobian *result, const unsigned char *scalars, const helios_jacobian *points, size_t n)
{
    const int w = pippenger_window_size(n);
    const int num_buckets = (1 << (w - 1));
    const int num_windows = (256 + w - 1) / w;

    std::vector<signed char> all_digits(n * num_windows);
    for (size_t i = 0; i < n; i++)
    {
        encode_signed_wbit(all_digits.data() + i * num_windows, scalars + i * 32, w);
    }

    helios_jacobian total;
    helios_identity(&total);
    bool total_is_identity = true;

    for (int win = num_windows - 1; win >= 0; win--)
    {
        // Horner step: multiply accumulated result by 2^w
        if (!total_is_identity)
        {
            for (int dd = 0; dd < w; dd++)
                helios_dbl_x64(&total, &total);
        }

        // Initialize buckets
        std::vector<helios_jacobian> bucket_points(num_buckets);
        std::vector<bool> bucket_is_identity(num_buckets, true);

        // Distribute points into buckets
        for (size_t i = 0; i < n; i++)
        {
            signed char digit = all_digits[i * num_windows + win];
            if (digit == 0)
                continue;

            int bucket_idx;
            helios_jacobian effective_point;

            if (digit > 0)
            {
                bucket_idx = digit - 1;
                helios_copy(&effective_point, &points[i]);
            }
            else
            {
                bucket_idx = (-digit) - 1;
                helios_neg(&effective_point, &points[i]);
            }

            if (bucket_is_identity[bucket_idx])
            {
                helios_copy(&bucket_points[bucket_idx], &effective_point);
                bucket_is_identity[bucket_idx] = false;
            }
            else
            {
                helios_add_safe(&bucket_points[bucket_idx], &bucket_points[bucket_idx], &effective_point);
            }
        }

        // Running-sum combination
        helios_jacobian running;
        bool running_is_identity = true;

        helios_jacobian partial;
        bool partial_is_identity = true;

        for (int j = num_buckets - 1; j >= 0; j--)
        {
            if (!bucket_is_identity[j])
            {
                if (running_is_identity)
                {
                    helios_copy(&running, &bucket_points[j]);
                    running_is_identity = false;
                }
                else
                {
                    helios_add_safe(&running, &running, &bucket_points[j]);
                }
            }

            if (!running_is_identity)
            {
                if (partial_is_identity)
                {
                    helios_copy(&partial, &running);
                    partial_is_identity = false;
                }
                else
                {
                    helios_add_safe(&partial, &partial, &running);
                }
            }
        }

        // Add this window's result to total
        if (!partial_is_identity)
        {
            if (total_is_identity)
            {
                helios_copy(&total, &partial);
                total_is_identity = false;
            }
            else
            {
                helios_add_safe(&total, &total, &partial);
            }
        }
    }

    if (total_is_identity)
        helios_identity(result);
    else
        helios_copy(result, &total);
}

// ============================================================================
// Public API (AVX2)
// ============================================================================

static const size_t STRAUS_PIPPENGER_CROSSOVER = 16;

void helios_msm_vartime_avx2(
    helios_jacobian *result,
    const unsigned char *scalars,
    const helios_jacobian *points,
    size_t n)
{
    if (n == 0)
    {
        helios_identity(result);
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
