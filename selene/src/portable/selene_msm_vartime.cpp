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
 * @file portable/selene_msm_vartime.cpp
 * @brief portable multi-scalar multiplication for Selene: Straus (n<=32) and Pippenger (n>32).
 */

#include "selene_msm_vartime.h"

#include "fq_mul.h"
#include "fq_ops.h"
#include "fq_sq.h"
#include "fq_utils.h"
#include "selene_add.h"
#include "selene_dbl.h"
#include "selene_ops.h"

#include <cstring>
#include <vector>

// ============================================================================
// Safe variable-time addition for Jacobian coordinates
// ============================================================================

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
            selene_dbl(r, p);
        }
        else
        {
            /* P == -Q: identity */
            selene_identity(r);
        }
        return;
    }

    selene_add(r, p, q);
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
// Straus (interleaved) method -- used for n <= 32
// ============================================================================

static void msm_straus(selene_jacobian *result, const unsigned char *scalars, const selene_jacobian *points, size_t n)
{
    std::vector<signed char> all_digits(n * 64);
    for (size_t i = 0; i < n; i++)
    {
        encode_signed_w4(all_digits.data() + i * 64, scalars + i * 32);
    }

    // Precompute tables: table[i][j] = (j+1) * points[i]
    std::vector<selene_jacobian> tables(n * 8);
    for (size_t i = 0; i < n; i++)
    {
        selene_jacobian *Ti = tables.data() + i * 8;
        selene_copy(&Ti[0], &points[i]);
        selene_dbl(&Ti[1], &points[i]);
        for (int j = 1; j < 7; j++)
        {
            selene_add_safe(&Ti[j + 1], &Ti[j], &points[i]);
        }
    }

    // Main loop
    selene_jacobian acc;
    selene_identity(&acc);
    bool acc_is_identity = true;

    for (int d = 63; d >= 0; d--)
    {
        if (!acc_is_identity)
        {
            selene_dbl(&acc, &acc);
            selene_dbl(&acc, &acc);
            selene_dbl(&acc, &acc);
            selene_dbl(&acc, &acc);
        }

        for (size_t i = 0; i < n; i++)
        {
            signed char digit = all_digits[i * 64 + d];
            if (digit == 0)
                continue;

            selene_jacobian pt;
            if (digit > 0)
            {
                selene_copy(&pt, &tables[i * 8 + digit - 1]);
            }
            else
            {
                selene_neg(&pt, &tables[i * 8 + (-digit) - 1]);
            }

            if (acc_is_identity)
            {
                selene_copy(&acc, &pt);
                acc_is_identity = false;
            }
            else
            {
                selene_add_safe(&acc, &acc, &pt);
            }
        }
    }

    if (acc_is_identity)
        selene_identity(result);
    else
        selene_copy(result, &acc);
}

// ============================================================================
// Pippenger (bucket method) -- used for n > 32
// ============================================================================

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
    msm_pippenger(selene_jacobian *result, const unsigned char *scalars, const selene_jacobian *points, size_t n)
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
        if (!total_is_identity)
        {
            for (int d = 0; d < w; d++)
                selene_dbl(&total, &total);
        }

        std::vector<selene_jacobian> bucket_points(num_buckets);
        std::vector<bool> bucket_is_identity(num_buckets, true);

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
// Public API (portable)
// ============================================================================

static const size_t STRAUS_PIPPENGER_CROSSOVER = 16;

void selene_msm_vartime_portable(
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
        msm_straus(result, scalars, points, n);
    }
    else
    {
        msm_pippenger(result, scalars, points, n);
    }
}
