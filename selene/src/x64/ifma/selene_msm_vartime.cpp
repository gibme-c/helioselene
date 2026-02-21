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
 * @file x64/ifma/selene_msm_vartime.cpp
 * @brief AVX-512 IFMA 8-way parallel MSM for Selene: Straus (n<=32) and Pippenger (n>32).
 *
 * Straus uses 8-way parallel fq51x8 point operations (selene_dbl_8x, selene_add_8x)
 * to process 8 independent scalar multiplications simultaneously. Points are packed
 * into selene_jacobian_8x structures, and per-lane table selection uses AVX-512 k-masks.
 *
 * Pippenger falls back to scalar x64 baseline point operations (selene_dbl_x64,
 * selene_add_x64) because the bucket accumulation method does not benefit from
 * lane-level parallelism.
 */

#include "selene_msm_vartime.h"

#include "fq_mul.h"
#include "fq_ops.h"
#include "fq_sq.h"
#include "fq_utils.h"
#include "selene_ops.h"
#include "x64/ifma/fq51x8_ifma.h"
#include "x64/ifma/selene_ifma.h"
#include "x64/selene_add.h"
#include "x64/selene_dbl.h"

#include <cstring>
#include <vector>

// ============================================================================
// Safe variable-time addition for Jacobian coordinates (scalar fq51 ops)
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
// Straus (interleaved) method with 8-way IFMA parallelism -- used for n <= 32
// ============================================================================

/*
 * 8-way parallel Straus MSM. Groups of 8 scalars are processed in parallel
 * using fq51x8 SIMD point operations. Each group of 8 shares a single
 * 8-way accumulator; after all digit positions are processed, the 8 results
 * are unpacked and combined with scalar additions.
 *
 * Precomputation: build scalar (fq51) tables for each point, then pack
 * groups of 8 table entries into selene_jacobian_8x structures.
 *
 * Main loop: for each digit position (63 down to 0):
 *   1. Double the 8-way accumulator 4 times (w=4 window)
 *   2. For each group, build a per-lane k-mask selection from the 8 table
 *      entries, conditionally negate per lane, and add to the accumulator
 *
 * Table selection uses AVX-512 k-mask conditional moves (selene_cmov_8x):
 * for table index j (1..8), a k-mask is built where bit k is set if
 * |digit[k]| == j. This selects the correct table entry per lane without
 * branches.
 */
static void
    msm_straus_ifma(selene_jacobian *result, const unsigned char *scalars, const selene_jacobian *points, size_t n)
{
    // Encode all scalars into signed w=4 digits
    std::vector<signed char> all_digits(n * 64);
    for (size_t i = 0; i < n; i++)
    {
        encode_signed_w4(all_digits.data() + i * 64, scalars + i * 32);
    }

    // Precompute scalar tables: table[i][j] = (j+1) * points[i], j=0..7
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

    // Number of groups of 8
    const size_t num_groups = (n + 7) / 8;

    // Pack tables into 8-way format: tables_8x[g*8+j] holds table entry j
    // for group g, with up to 8 lanes populated (identity for padding lanes)
    std::vector<selene_jacobian_8x> tables_8x(num_groups * 8);
    {
        selene_jacobian id;
        selene_identity(&id);

        for (size_t g = 0; g < num_groups; g++)
        {
            for (int j = 0; j < 8; j++)
            {
                const selene_jacobian *pts[8];
                for (int k = 0; k < 8; k++)
                {
                    pts[k] = (g * 8 + k < n) ? &tables[(g * 8 + k) * 8 + j] : &id;
                }

                selene_pack_8x(&tables_8x[g * 8 + j], pts[0], pts[1], pts[2], pts[3], pts[4], pts[5], pts[6], pts[7]);
            }
        }
    }

    // Per-group 8-way accumulators
    std::vector<selene_jacobian_8x> accum(num_groups);
    std::vector<bool> accum_started(num_groups, false);

    // Main loop: process digit positions from most significant to least
    for (int d = 63; d >= 0; d--)
    {
        // 4 doublings per digit position (w=4 window)
        for (size_t g = 0; g < num_groups; g++)
        {
            if (accum_started[g])
            {
                selene_dbl_8x(&accum[g], &accum[g]);
                selene_dbl_8x(&accum[g], &accum[g]);
                selene_dbl_8x(&accum[g], &accum[g]);
                selene_dbl_8x(&accum[g], &accum[g]);
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
            selene_jacobian_8x selected;
            selene_identity_8x(&selected);

            for (int j = 0; j < 8; j++)
            {
                __mmask8 mask = 0;
                for (int k = 0; k < 8; k++)
                {
                    if (abs_d[k] == static_cast<unsigned int>(j + 1))
                        mask |= static_cast<__mmask8>(1 << k);
                }

                if (mask)
                    selene_cmov_8x(&selected, &tables_8x[g * 8 + j], mask);
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
                    selene_jacobian_8x neg_sel;
                    selene_neg_8x(&neg_sel, &selected);
                    fq51x8_cmov(&selected.Y, &neg_sel.Y, neg_mask);
                }
            }

            // Accumulate
            if (!accum_started[g])
            {
                selene_copy_8x(&accum[g], &selected);
                accum_started[g] = true;
            }
            else
            {
                selene_add_8x(&accum[g], &accum[g], &selected);
            }
        }
    }

    // Combine all groups: unpack each 8-way accumulator and sum the individual
    // results with scalar additions
    selene_jacobian total;
    selene_identity(&total);
    bool total_started = false;

    for (size_t g = 0; g < num_groups; g++)
    {
        if (!accum_started[g])
            continue;

        selene_jacobian parts[8];
        selene_unpack_8x(
            &parts[0], &parts[1], &parts[2], &parts[3], &parts[4], &parts[5], &parts[6], &parts[7], &accum[g]);

        for (int k = 0; k < 8 && g * 8 + k < n; k++)
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
    msm_pippenger_ifma(selene_jacobian *result, const unsigned char *scalars, const selene_jacobian *points, size_t n)
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
            for (int d = 0; d < w; d++)
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
// Public API (IFMA)
// ============================================================================

static const size_t STRAUS_PIPPENGER_CROSSOVER = 16;

void selene_msm_vartime_ifma(
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
        msm_straus_ifma(result, scalars, points, n);
    }
    else
    {
        msm_pippenger_ifma(result, scalars, points, n);
    }
}
