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

#include "ranshaw_secure_erase.h"
#include "shaw_complete_add.h"
#include "shaw_ops.h"
#include "shaw_projective.h"
#include "x64/ifma/shaw_complete_add_ifma.h"
#include "x64/ifma/shaw_msm_ct_ifma.h"
#include "x64/ifma/shaw_projective_8x.h"

#include <cstdint>
#include <vector>

/*
 * Shaw mirror of ran_msm_ct_ifma; see ran/src/x64/ifma/ran_msm_ct.cpp for
 * full driver commentary.
 */

namespace
{

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

void shaw_msm_ct_ifma(shaw_jacobian *result, const unsigned char *scalars, const shaw_jacobian *points, size_t n)
{
    if (n < 4)
    {
        /* Avoid dispatch recursion: see Ran mirror for rationale. */
        shaw_msm_ct_scalar(result, scalars, points, n);
        return;
    }

    const size_t num_groups = (n + 7) / 8;

    std::vector<shaw_projective> scalar_tables(8 * n);
    for (size_t i = 0; i < n; i++)
    {
        shaw_projective *row = &scalar_tables[8 * i];
        shaw_jac_to_proj(&row[0], &points[i]);
        shaw_complete_add(&row[1], &row[0], &row[0]);
        shaw_complete_add(&row[2], &row[0], &row[1]);
        shaw_complete_add(&row[3], &row[0], &row[2]);
        shaw_complete_add(&row[4], &row[0], &row[3]);
        shaw_complete_add(&row[5], &row[0], &row[4]);
        shaw_complete_add(&row[6], &row[0], &row[5]);
        shaw_complete_add(&row[7], &row[0], &row[6]);
    }

    std::vector<shaw_projective_8x> packed_tables(num_groups * 8);
    for (size_t g = 0; g < num_groups; g++)
    {
        for (size_t j = 0; j < 8; j++)
        {
            const shaw_projective *slots[8];
            for (size_t k = 0; k < 8; k++)
            {
                size_t point_idx = g * 8 + k;
                slots[k] = (point_idx < n) ? &scalar_tables[8 * point_idx + j] : nullptr;
            }
            shaw_pack_proj_8x(
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

    std::vector<int8_t> digits(64 * n);
    for (size_t i = 0; i < n; i++)
    {
        scalar_recode_signed4(&digits[64 * i], &scalars[32 * i]);
    }

    std::vector<shaw_projective_8x> accum(num_groups);
    for (size_t g = 0; g < num_groups; g++)
    {
        shaw_proj_identity_8x(&accum[g]);
    }

    for (int w = 63; w >= 0; w--)
    {
        for (size_t g = 0; g < num_groups; g++)
        {
            shaw_projective_8x doubled;
            shaw_complete_add_ifma_8x(&doubled, &accum[g], &accum[g]);
            shaw_complete_add_ifma_8x(&accum[g], &doubled, &doubled);
            shaw_complete_add_ifma_8x(&doubled, &accum[g], &accum[g]);
            shaw_complete_add_ifma_8x(&accum[g], &doubled, &doubled);

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

            shaw_projective_8x selected;
            shaw_proj_identity_8x(&selected);
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
                shaw_proj_cmov_8x(&selected, &packed_tables[g * 8 + (j - 1)], mask);
            }

            shaw_proj_cneg_8x(&selected, neg_mask);

            shaw_projective_8x new_acc;
            shaw_complete_add_ifma_8x(&new_acc, &accum[g], &selected);
            shaw_proj_copy_8x(&accum[g], &new_acc);
        }
    }

    shaw_projective total;
    shaw_proj_identity(&total);
    for (size_t g = 0; g < num_groups; g++)
    {
        shaw_projective lane[8];
        shaw_unpack_proj_8x(&lane[0], &lane[1], &lane[2], &lane[3], &lane[4], &lane[5], &lane[6], &lane[7], &accum[g]);
        for (size_t k = 0; k < 8; k++)
        {
            size_t point_idx = g * 8 + k;
            if (point_idx >= n)
            {
                break;
            }
            shaw_projective new_total;
            shaw_complete_add(&new_total, &total, &lane[k]);
            total = new_total;
        }
    }

    shaw_proj_to_jac(result, &total);

    ranshaw_secure_erase(digits.data(), digits.size() * sizeof(int8_t));
    ranshaw_secure_erase(&total, sizeof(total));
}
