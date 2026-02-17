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
 * @file x64/ifma/helios_msm_vartime.cpp
 * @brief AVX-512 IFMA 8-way parallel MSM for Helios: Straus (n<=32) and Pippenger (n>32).
 *
 * Straus uses 8-way parallel fp51x8 point operations (helios_dbl_8x, helios_add_8x)
 * to process 8 independent scalar multiplications simultaneously. Points are packed
 * into helios_jacobian_8x structures, and per-lane table selection uses AVX-512 k-masks.
 *
 * Pippenger falls back to scalar x64 baseline point operations (helios_dbl_x64,
 * helios_add_x64) because the bucket accumulation method does not benefit from
 * lane-level parallelism.
 */

#include "helios_msm_vartime.h"

#include "fp_mul.h"
#include "fp_ops.h"
#include "fp_sq.h"
#include "fp_utils.h"
#include "helios_ops.h"
#include "x64/helios_add.h"
#include "x64/helios_dbl.h"
#include "x64/ifma/fp51x8_ifma.h"
#include "x64/ifma/helios_ifma.h"

#include <cstring>
#include <vector>

// ============================================================================
// Safe variable-time addition for Jacobian coordinates (scalar fp51 ops)
// ============================================================================

/*
 * Variable-time "safe" addition that handles all edge cases:
 * - p == identity: return q
 * - q == identity: return p
 * - p == q: use doubling
 * - p == -q: return identity
 * - otherwise: standard addition
 *
 * Uses x64 baseline scalar ops (not dispatch table) since this file is
 * compiled with AVX-512 flags and we need the x64 implementations directly.
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
// Straus (interleaved) method with 8-way IFMA parallelism -- used for n <= 32
// ============================================================================

/*
 * 8-way parallel Straus MSM. Groups of 8 scalars are processed in parallel
 * using fp51x8 SIMD point operations. Each group of 8 shares a single
 * 8-way accumulator; after all digit positions are processed, the 8 results
 * are unpacked and combined with scalar additions.
 *
 * Precomputation: build scalar (fp51) tables for each point, then pack
 * groups of 8 table entries into helios_jacobian_8x structures.
 *
 * Main loop: for each digit position (63 down to 0):
 *   1. Double the 8-way accumulator 4 times (w=4 window)
 *   2. For each group, build a per-lane k-mask selection from the 8 table
 *      entries, conditionally negate per lane, and add to the accumulator
 *
 * Table selection uses AVX-512 k-mask conditional moves (helios_cmov_8x):
 * for table index j (1..8), a k-mask is built where bit k is set if
 * |digit[k]| == j. This selects the correct table entry per lane without
 * branches.
 */
static void
    msm_straus_ifma(helios_jacobian *result, const unsigned char *scalars, const helios_jacobian *points, size_t n)
{
    // Encode all scalars into signed w=4 digits
    std::vector<signed char> all_digits(n * 64);
    for (size_t i = 0; i < n; i++)
    {
        encode_signed_w4(all_digits.data() + i * 64, scalars + i * 32);
    }

    // Precompute scalar tables: table[i][j] = (j+1) * points[i], j=0..7
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

    // Number of groups of 8
    const size_t num_groups = (n + 7) / 8;

    // Pack tables into 8-way format: tables_8x[g*8+j] holds table entry j
    // for group g, with up to 8 lanes populated (identity for padding lanes)
    std::vector<helios_jacobian_8x> tables_8x(num_groups * 8);
    {
        helios_jacobian id;
        helios_identity(&id);

        for (size_t g = 0; g < num_groups; g++)
        {
            for (int j = 0; j < 8; j++)
            {
                const helios_jacobian *pts[8];
                for (int k = 0; k < 8; k++)
                {
                    pts[k] = (g * 8 + k < n) ? &tables[(g * 8 + k) * 8 + j] : &id;
                }

                helios_pack_8x(&tables_8x[g * 8 + j], pts[0], pts[1], pts[2], pts[3], pts[4], pts[5], pts[6], pts[7]);
            }
        }
    }

    // Per-group 8-way accumulators
    std::vector<helios_jacobian_8x> accum(num_groups);
    std::vector<bool> accum_started(num_groups, false);

    // Main loop: process digit positions from most significant to least
    for (int d = 63; d >= 0; d--)
    {
        // 4 doublings per digit position (w=4 window)
        for (size_t g = 0; g < num_groups; g++)
        {
            if (accum_started[g])
            {
                helios_dbl_8x(&accum[g], &accum[g]);
                helios_dbl_8x(&accum[g], &accum[g]);
                helios_dbl_8x(&accum[g], &accum[g]);
                helios_dbl_8x(&accum[g], &accum[g]);
            }
        }

        // Add contributions for each group
        for (size_t g = 0; g < num_groups; g++)
        {
            // Gather the 8 digits for this group at this position
            signed char digits[8];
            unsigned int abs_d[8];
            unsigned int neg_flag[8];
            bool all_zero = true;

            for (int k = 0; k < 8; k++)
            {
                digits[k] = (g * 8 + k < n) ? all_digits[(g * 8 + k) * 64 + d] : 0;
                abs_d[k] = static_cast<unsigned int>((digits[k] < 0) ? -digits[k] : digits[k]);
                neg_flag[k] = (digits[k] < 0) ? 1u : 0u;
                if (digits[k] != 0)
                    all_zero = false;
            }

            if (all_zero)
                continue;

            // Per-lane table selection using k-masks:
            // Start with identity, then for each table index j (1..8), build a
            // mask of lanes whose |digit| == j and conditionally move that table
            // entry into those lanes.
            helios_jacobian_8x selected;
            helios_identity_8x(&selected);

            for (int j = 0; j < 8; j++)
            {
                __mmask8 mask = 0;
                for (int k = 0; k < 8; k++)
                {
                    if (abs_d[k] == static_cast<unsigned int>(j + 1))
                        mask |= static_cast<__mmask8>(1 << k);
                }

                if (mask)
                    helios_cmov_8x(&selected, &tables_8x[g * 8 + j], mask);
            }

            // Per-lane conditional negate: for lanes where digit < 0, negate Y
            {
                __mmask8 neg_mask = 0;
                for (int k = 0; k < 8; k++)
                {
                    if (neg_flag[k])
                        neg_mask |= static_cast<__mmask8>(1 << k);
                }

                if (neg_mask)
                {
                    helios_jacobian_8x neg_sel;
                    helios_neg_8x(&neg_sel, &selected);
                    fp51x8_cmov(&selected.Y, &neg_sel.Y, neg_mask);
                }
            }

            // Accumulate
            if (!accum_started[g])
            {
                helios_copy_8x(&accum[g], &selected);
                accum_started[g] = true;
            }
            else
            {
                helios_add_8x(&accum[g], &accum[g], &selected);
            }
        }
    }

    // Combine all groups: unpack each 8-way accumulator and sum the individual
    // results with scalar additions
    helios_jacobian total;
    helios_identity(&total);
    bool total_started = false;

    for (size_t g = 0; g < num_groups; g++)
    {
        if (!accum_started[g])
            continue;

        helios_jacobian parts[8];
        helios_unpack_8x(
            &parts[0], &parts[1], &parts[2], &parts[3], &parts[4], &parts[5], &parts[6], &parts[7], &accum[g]);

        for (int k = 0; k < 8 && g * 8 + k < n; k++)
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
// Pippenger (bucket method) using scalar x64 ops -- used for n > 32
// ============================================================================

/*
 * Pippenger's bucket method does not benefit from 8-way lane parallelism
 * because bucket accumulation involves irregular scatter-gather patterns
 * (each point goes to a different bucket based on its digit). Instead, we
 * use the x64 baseline scalar point operations which are already efficient
 * for this access pattern.
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
    msm_pippenger_ifma(helios_jacobian *result, const unsigned char *scalars, const helios_jacobian *points, size_t n)
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
            for (int d = 0; d < w; d++)
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
// Public API (IFMA)
// ============================================================================

static const size_t STRAUS_PIPPENGER_CROSSOVER = 32;

void helios_msm_vartime_ifma(
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
        msm_straus_ifma(result, scalars, points, n);
    }
    else
    {
        msm_pippenger_ifma(result, scalars, points, n);
    }
}
