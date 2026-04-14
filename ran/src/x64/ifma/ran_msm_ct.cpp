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

#include "ran_msm_ct.h"

#include "ran_complete_add.h"
#include "ran_ops.h"
#include "ran_projective.h"
#include "ranshaw_secure_erase.h"
#include "x64/ifma/ran_complete_add_ifma.h"
#include "x64/ifma/ran_msm_ct_ifma.h"
#include "x64/ifma/ran_projective_8x.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{

    /* Recode a 32-byte LE scalar to 64 signed-4 digits in [-8, +7]. Duplicated
     * from ran_msm_ct.cpp to avoid cross-TU coupling; branchless as usual. */
    void scalar_recode_signed4(int8_t digits[64], const unsigned char scalar[32])
    {
        uint8_t nibbles[64];
        for (int i = 0; i < 32; i++)
        {
            nibbles[2 * i] = scalar[i] & 0x0f;
            nibbles[2 * i + 1] = (scalar[i] >> 4) & 0x0f;
        }

        int carry = 0;
        for (int i = 0; i < 63; i++)
        {
            int val = (int)nibbles[i] + carry;
            carry = (val + 8) >> 4;
            digits[i] = (int8_t)(val - (carry << 4));
        }
        digits[63] = (int8_t)((int)nibbles[63] + carry);
        ranshaw_secure_erase(nibbles, sizeof(nibbles));
    }

} // namespace

/* IFMA per-point Straus CT MSM driver.
 *
 * Splits n points into groups of 8 (padding the last group with identity
 * lanes when n % 8 != 0). Each group runs 8 scalar-multiplications in
 * parallel across the fp51x8 lanes of one packed accumulator. Between
 * windows the accumulator is doubled via the packed RCB complete-add
 * (ran_complete_add_ifma_8x); per lane, the signed-4 digit for that window
 * selects one of 8 projective table entries (or identity for digit 0) via
 * per-lane mmask cmov, gets negated on sign, and folds into the accumulator
 * with another packed RCB add.
 *
 * After 64 windows, lane k of group g holds scalar_k * P_k; we unpack each
 * group and sum the 8 contributions via scalar RCB. Padded lanes stay at
 * identity so they are no-ops in the final sum.
 *
 * For n < 4 the overhead of lane-packing plus 7 extra RCB-adds in the
 * reduction outweighs the SIMD parallelism, so we fall through to the
 * scalar ran_msm_ct.
 */
void ran_msm_ct_ifma(ran_jacobian *result, const unsigned char *scalars, const ran_jacobian *points, size_t n)
{
    if (n < 4)
    {
        /* Call the scalar implementation directly rather than the dispatched
         * wrapper, since the wrapper could route back here on an IFMA build
         * and recurse indefinitely. */
        ran_msm_ct_scalar(result, scalars, points, n);
        return;
    }

    const size_t num_groups = (n + 7) / 8;

    /* Per point: build an 8-entry projective table via the scalar RCB (used
     * only during precompute; the main loop uses the 8-lane packed RCB). */
    std::vector<ran_projective> scalar_tables(8 * n);
    for (size_t i = 0; i < n; i++)
    {
        ran_projective *row = &scalar_tables[8 * i];
        ran_jac_to_proj(&row[0], &points[i]);
        ran_complete_add(&row[1], &row[0], &row[0]);
        ran_complete_add(&row[2], &row[0], &row[1]);
        ran_complete_add(&row[3], &row[0], &row[2]);
        ran_complete_add(&row[4], &row[0], &row[3]);
        ran_complete_add(&row[5], &row[0], &row[4]);
        ran_complete_add(&row[6], &row[0], &row[5]);
        ran_complete_add(&row[7], &row[0], &row[6]);
    }

    /* Pack tables into 8-lane form: packed_tables[g * 8 + j] holds the
     * j+1 multiple of each of the 8 points in group g (identity in padded
     * lanes). */
    std::vector<ran_projective_8x> packed_tables(num_groups * 8);
    for (size_t g = 0; g < num_groups; g++)
    {
        for (size_t j = 0; j < 8; j++)
        {
            const ran_projective *slots[8];
            for (size_t k = 0; k < 8; k++)
            {
                size_t point_idx = g * 8 + k;
                slots[k] = (point_idx < n) ? &scalar_tables[8 * point_idx + j] : nullptr;
            }
            ran_pack_proj_8x(
                &packed_tables[g * 8 + j],
                slots[0],
                slots[1],
                slots[2],
                slots[3],
                slots[4],
                slots[5],
                slots[6],
                slots[7]);
        }
    }

    /* Recode all scalars to signed-4 digits. digits[i * 64 + w] = digit for
     * point i at window w (0 = LSB, 63 = MSB). */
    std::vector<int8_t> digits(64 * n);
    for (size_t i = 0; i < n; i++)
    {
        scalar_recode_signed4(&digits[64 * i], &scalars[32 * i]);
    }

    /* Per-group accumulator, initialized to projective identity in every lane.
     * Safe starting state: adding identity plus anything gives anything, and
     * doubling identity gives identity — both handled correctly by RCB. */
    std::vector<ran_projective_8x> accum(num_groups);
    for (size_t g = 0; g < num_groups; g++)
    {
        ran_proj_identity_8x(&accum[g]);
    }

    for (int w = 63; w >= 0; w--)
    {
        for (size_t g = 0; g < num_groups; g++)
        {
            /* Four doublings per window (w = 4 → multiply by 2^4). */
            ran_projective_8x doubled;
            ran_complete_add_ifma_8x(&doubled, &accum[g], &accum[g]);
            ran_complete_add_ifma_8x(&accum[g], &doubled, &doubled);
            ran_complete_add_ifma_8x(&doubled, &accum[g], &accum[g]);
            ran_complete_add_ifma_8x(&accum[g], &doubled, &doubled);

            /* Gather this window's digits for the 8 lanes of this group. */
            unsigned int abs_digit[8] = {0};
            __mmask8 neg_mask = 0;
            for (size_t k = 0; k < 8; k++)
            {
                size_t point_idx = g * 8 + k;
                int8_t d = (point_idx < n) ? digits[64 * point_idx + (size_t)w] : (int8_t)0;
                unsigned int sign_bit = ((uint8_t)d >> 7) & 1u;
                abs_digit[k] = (unsigned int)((d ^ (int8_t)(-(int)sign_bit)) + (int)sign_bit);
                if (sign_bit)
                {
                    neg_mask |= (__mmask8)((unsigned)1 << k);
                }
            }

            /* Per-lane table lookup via 8 mmasks (one per table entry 1..8).
             * Start with identity in all lanes; for each j ∈ [1..8], build
             * the mmask of lanes whose abs_digit == j and cmov in packed
             * table entry j-1. The loop touches all 8 table entries
             * unconditionally; data-independence on secret scalar bits. */
            ran_projective_8x selected;
            ran_proj_identity_8x(&selected);
            for (unsigned int j = 1; j <= 8; j++)
            {
                __mmask8 mask = 0;
                for (size_t k = 0; k < 8; k++)
                {
                    if (abs_digit[k] == j)
                    {
                        mask |= (__mmask8)((unsigned)1 << k);
                    }
                }
                ran_proj_cmov_8x(&selected, &packed_tables[g * 8 + (j - 1)], mask);
            }

            ran_proj_cneg_8x(&selected, neg_mask);

            ran_projective_8x new_acc;
            ran_complete_add_ifma_8x(&new_acc, &accum[g], &selected);
            ran_proj_copy_8x(&accum[g], &new_acc);
        }
    }

    /* Reduce: unpack each group's 8 lanes to scalar projective points, then
     * sum them all via scalar RCB. Padded lanes (those beyond the original
     * n) are identity and contribute nothing. */
    ran_projective total;
    ran_proj_identity(&total);
    for (size_t g = 0; g < num_groups; g++)
    {
        ran_projective lane[8];
        ran_unpack_proj_8x(&lane[0], &lane[1], &lane[2], &lane[3], &lane[4], &lane[5], &lane[6], &lane[7], &accum[g]);
        for (size_t k = 0; k < 8; k++)
        {
            size_t point_idx = g * 8 + k;
            if (point_idx >= n)
            {
                break;
            }
            ran_projective new_total;
            ran_complete_add(&new_total, &total, &lane[k]);
            total = new_total;
        }
    }

    ran_proj_to_jac(result, &total);

    ranshaw_secure_erase(digits.data(), digits.size() * sizeof(int8_t));
    ranshaw_secure_erase(&total, sizeof(total));
}
