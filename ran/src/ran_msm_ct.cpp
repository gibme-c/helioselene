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
#include "ranshaw_ct_barrier.h"
#include "ranshaw_secure_erase.h"

#include <cstdint>
#include <vector>

namespace
{

    /* Constant-time equality on 32-bit integers: returns 1 if a == b, else 0.
     * The ct_barrier prevents compilers from proving (a ^ b) != 0 via upstream
     * data-flow analysis and short-circuiting the mask. */
    inline unsigned int ct_eq_u32(uint32_t a, uint32_t b)
    {
        uint32_t x = ranshaw_ct_barrier_u32(a ^ b);
        /* (x | -x) >> 31 is 1 iff x != 0; XOR with 1 inverts to "equal". */
        return 1u ^ ((x | (0u - x)) >> 31);
    }

    /* Recode a 32-byte LE scalar into 64 signed-4 digits in [-8, +8], with carry
     * absorbed. Branchless; mirrors the helper that lives file-static inside each
     * backend's ran_scalarmult.cpp, duplicated here to avoid cross-TU coupling. */
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

    /* CT scan-and-select from an 8-entry half-table {P, 2P, ..., 8P}.
     *
     * For abs_digit in [0, 8]:
     *   abs_digit == 0 → selected = identity (all cmovs miss)
     *   abs_digit == k in [1, 8] → selected = table[k - 1]
     *
     * The 8-entry size matches the range produced by signed-4 recoding, which is
     * [-8, +7] (digit = -8 ↔ abs_digit = 8 is reachable only on the low end).
     * The loop is data-independent: iterations always touch all 8 slots in order,
     * using fp_cmov to merge the matching entry. */
    void ct_table_lookup(ran_projective *selected, const ran_projective *table8, unsigned int abs_digit)
    {
        ran_proj_identity(selected);
        for (unsigned int k = 1; k <= 8; k++)
        {
            ran_proj_cmov(selected, &table8[k - 1], ct_eq_u32(abs_digit, k));
        }
    }

} // namespace

/* Signed-4 windowed Straus over Renes-Costello-Batina 2016 Algorithm 4.
 *
 * Window width w = 4, 64 windows per 32-byte scalar, digits in [-8, +8].
 * Per point, precompute a 7-entry half-table {P, 2P, ..., 7P}; on each
 * window, lookup & conditionally-negate the entry for each point's digit
 * and fold into the shared accumulator. Doubling uses ran_complete_add on
 * (acc, acc) because RCB is correct for P = Q.
 *
 * Handling of n = 0: public-value branch, safe to take early. Identity is
 * the multiplicative identity of the group, i.e. the empty sum.
 *
 * Time cost per (scalars, points, n) call is a function of n alone:
 *   table build:  n × 7 complete-adds (8-entry table {P, 2P, ..., 8P})
 *   main loop:    64 × (4 doublings + n × (lookup + add)) complete-adds
 *                + 64 × n × 8 fp_cmov triples (from ct_table_lookup)
 */
void ran_msm_ct_scalar(ran_jacobian *result, const unsigned char *scalars, const ran_jacobian *points, size_t n)
{
    if (n == 0)
    {
        ran_identity(result);
        return;
    }

    /* Table: 8 projective entries per point, laid out [P0*1..P0*8, P1*1..P1*8, ...]. */
    std::vector<ran_projective> table(8 * n);
    for (size_t i = 0; i < n; i++)
    {
        ran_projective *row = &table[8 * i];
        ran_jac_to_proj(&row[0], &points[i]); /* 1P */
        ran_complete_add(&row[1], &row[0], &row[0]); /* 2P = P + P (RCB handles P=Q) */
        ran_complete_add(&row[2], &row[0], &row[1]); /* 3P */
        ran_complete_add(&row[3], &row[0], &row[2]); /* 4P */
        ran_complete_add(&row[4], &row[0], &row[3]); /* 5P */
        ran_complete_add(&row[5], &row[0], &row[4]); /* 6P */
        ran_complete_add(&row[6], &row[0], &row[5]); /* 7P */
        ran_complete_add(&row[7], &row[0], &row[6]); /* 8P */
    }

    /* Signed-4 digits: 64 per scalar, packed [s0_d0..s0_d63, s1_d0..s1_d63, ...]. */
    std::vector<int8_t> digits(64 * n);
    for (size_t i = 0; i < n; i++)
    {
        scalar_recode_signed4(&digits[64 * i], &scalars[32 * i]);
    }

    ran_projective acc;
    ran_proj_identity(&acc);

    /* 64 windows MSB→LSB. Each window: 4 doublings of the accumulator (because
     * w=4 → multiply by 2^4 between consecutive windows), then for each point
     * fold in its signed-4 digit via lookup + cneg + add. */
    for (int w = 63; w >= 0; w--)
    {
        for (int d = 0; d < 4; d++)
        {
            ran_projective doubled;
            ran_complete_add(&doubled, &acc, &acc);
            acc = doubled;
        }

        for (size_t i = 0; i < n; i++)
        {
            int8_t digit = digits[64 * i + (size_t)w];
            /* Sign / magnitude split, branchless. */
            unsigned int sign_bit = ((uint8_t)digit >> 7) & 1u;
            unsigned int abs_digit = (unsigned int)((digit ^ (int8_t)(-(int)sign_bit)) + (int)sign_bit);

            ran_projective selected;
            ct_table_lookup(&selected, &table[8 * i], abs_digit);
            ran_proj_cneg(&selected, sign_bit);

            ran_projective new_acc;
            ran_complete_add(&new_acc, &acc, &selected);
            acc = new_acc;
        }
    }

    ran_proj_to_jac(result, &acc);

    /* Secure erase: digits carry scalar-derived bits; the accumulator carried
     * intermediate scalar-multiple state; the table entries are non-secret
     * (points are public) but zero out anyway for hygiene. */
    ranshaw_secure_erase(digits.data(), digits.size() * sizeof(int8_t));
    ranshaw_secure_erase(&acc, sizeof(acc));
}
