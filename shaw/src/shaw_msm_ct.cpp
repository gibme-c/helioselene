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

#include "shaw_msm_ct.h"

#include "ranshaw_ct_barrier.h"
#include "ranshaw_secure_erase.h"
#include "shaw_complete_add.h"
#include "shaw_ops.h"
#include "shaw_projective.h"

#include <cstdint>
#include <vector>

/*
 * Shaw mirror of ran_msm_ct.cpp; see that file for the full commentary on
 * the signed-4 Straus driver and its timing guarantees.
 */

namespace
{

    inline unsigned int ct_eq_u32(uint32_t a, uint32_t b)
    {
        uint32_t x = ranshaw_ct_barrier_u32(a ^ b);
        return 1u ^ ((x | (0u - x)) >> 31);
    }

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

    void ct_table_lookup(shaw_projective *selected, const shaw_projective *table8, unsigned int abs_digit)
    {
        shaw_proj_identity(selected);
        for (unsigned int k = 1; k <= 8; k++)
        {
            shaw_proj_cmov(selected, &table8[k - 1], ct_eq_u32(abs_digit, k));
        }
    }

} // namespace

void shaw_msm_ct_scalar(shaw_jacobian *result, const unsigned char *scalars, const shaw_jacobian *points, size_t n)
{
    if (n == 0)
    {
        shaw_identity(result);
        return;
    }

    std::vector<shaw_projective> table(8 * n);
    for (size_t i = 0; i < n; i++)
    {
        shaw_projective *row = &table[8 * i];
        shaw_jac_to_proj(&row[0], &points[i]);
        shaw_complete_add(&row[1], &row[0], &row[0]);
        shaw_complete_add(&row[2], &row[0], &row[1]);
        shaw_complete_add(&row[3], &row[0], &row[2]);
        shaw_complete_add(&row[4], &row[0], &row[3]);
        shaw_complete_add(&row[5], &row[0], &row[4]);
        shaw_complete_add(&row[6], &row[0], &row[5]);
        shaw_complete_add(&row[7], &row[0], &row[6]);
    }

    std::vector<int8_t> digits(64 * n);
    for (size_t i = 0; i < n; i++)
    {
        scalar_recode_signed4(&digits[64 * i], &scalars[32 * i]);
    }

    shaw_projective acc;
    shaw_proj_identity(&acc);

    for (int w = 63; w >= 0; w--)
    {
        for (int d = 0; d < 4; d++)
        {
            shaw_projective doubled;
            shaw_complete_add(&doubled, &acc, &acc);
            acc = doubled;
        }

        for (size_t i = 0; i < n; i++)
        {
            int8_t digit = digits[64 * i + (size_t)w];
            unsigned int sign_bit = ((uint8_t)digit >> 7) & 1u;
            unsigned int abs_digit = (unsigned int)((digit ^ (int8_t)(-(int)sign_bit)) + (int)sign_bit);

            shaw_projective selected;
            ct_table_lookup(&selected, &table[8 * i], abs_digit);
            shaw_proj_cneg(&selected, sign_bit);

            shaw_projective new_acc;
            shaw_complete_add(&new_acc, &acc, &selected);
            acc = new_acc;
        }
    }

    shaw_proj_to_jac(result, &acc);

    ranshaw_secure_erase(digits.data(), digits.size() * sizeof(int8_t));
    ranshaw_secure_erase(&acc, sizeof(acc));
}
