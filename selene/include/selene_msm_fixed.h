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

#ifndef HELIOSELENE_SELENE_MSM_FIXED_H
#define HELIOSELENE_SELENE_MSM_FIXED_H

/**
 * @file selene_msm_fixed.h
 * @brief Fixed-base multi-scalar multiplication for Selene.
 *
 * Interleaved w=5 fixed-window MSM: processes all scalars simultaneously,
 * sharing the 255 doublings across all n points. Cost is 255 doublings +
 * 52*n mixed additions, saving (n-1)*255 doublings vs individual scalarmults.
 */

#include "helioselene_secure_erase.h"
#include "selene.h"
#include "selene_add.h"
#include "selene_dbl.h"
#include "selene_madd.h"
#include "selene_ops.h"
#include "selene_scalarmult_fixed.h"

#include <vector>

/**
 * Fixed-base MSM: r = sum(scalars[i] * points_from_tables[i]) for i = 0..n-1.
 *
 * Each table[i] is a 16-entry affine table [1P_i, 2P_i, ..., 16P_i] precomputed
 * via selene_scalarmult_fixed_precompute().
 *
 * @param r       Output point (Jacobian)
 * @param scalars n * 32 bytes of scalars (packed, little-endian)
 * @param tables  Array of n pointers to 16-entry affine tables
 * @param n       Number of scalar-table pairs
 */
static inline void
    selene_msm_fixed(selene_jacobian *r, const unsigned char *scalars, const selene_affine *const *tables, size_t n)
{
    if (n == 0)
    {
        selene_identity(r);
        return;
    }

    if (n == 1)
    {
        selene_scalarmult_fixed(r, scalars, tables[0]);
        return;
    }

    /* Recode all scalars */
    std::vector<int8_t> all_digits(52 * n);
    for (size_t j = 0; j < n; j++)
        selene_scalar_recode_signed5(all_digits.data() + j * 52, scalars + j * 32);

    /* Start from top window (51) */
    selene_identity(r);

    for (size_t j = 0; j < n; j++)
    {
        int32_t d = (int32_t)all_digits[j * 52 + 51];
        int32_t sign_mask = d >> 31;
        unsigned int abs_d = (unsigned int)((d ^ sign_mask) - sign_mask);
        unsigned int neg = (unsigned int)(sign_mask & 1);

        selene_affine selected;
        fq_0(selected.x);
        fq_0(selected.y);
        for (unsigned int k = 0; k < 16; k++)
        {
            unsigned int eq = ((abs_d ^ (k + 1)) - 1u) >> 31;
            selene_affine_cmov(&selected, &tables[j][k], eq);
        }
        selene_affine_cneg(&selected, neg);

        unsigned int nonzero = 1u ^ ((abs_d - 1u) >> 31);
        unsigned int z_nonzero = (unsigned int)fq_isnonzero(r->Z);

        selene_jacobian tmp;
        selene_madd(&tmp, r, &selected);

        selene_jacobian fresh;
        selene_from_affine(&fresh, &selected);

        selene_cmov(r, &tmp, nonzero & z_nonzero);
        selene_cmov(r, &fresh, nonzero & (1u - z_nonzero));
    }

    /* Main loop: windows 50 down to 0 */
    for (int i = 50; i >= 0; i--)
    {
        /* 5 shared doublings */
        selene_dbl(r, r);
        selene_dbl(r, r);
        selene_dbl(r, r);
        selene_dbl(r, r);
        selene_dbl(r, r);

        /* Add contribution from each point */
        for (size_t j = 0; j < n; j++)
        {
            int32_t d = (int32_t)all_digits[j * 52 + i];
            int32_t sign_mask = d >> 31;
            unsigned int abs_d = (unsigned int)((d ^ sign_mask) - sign_mask);
            unsigned int neg = (unsigned int)(sign_mask & 1);

            selene_affine selected;
            fq_1(selected.x);
            fq_1(selected.y);
            for (unsigned int k = 0; k < 16; k++)
            {
                unsigned int eq = ((abs_d ^ (k + 1)) - 1u) >> 31;
                selene_affine_cmov(&selected, &tables[j][k], eq);
            }
            selene_affine_cneg(&selected, neg);

            unsigned int nonzero = 1u ^ ((abs_d - 1u) >> 31);
            unsigned int z_nonzero = (unsigned int)fq_isnonzero(r->Z);

            selene_jacobian tmp;
            selene_madd(&tmp, r, &selected);

            selene_jacobian fresh;
            selene_from_affine(&fresh, &selected);

            selene_cmov(r, &tmp, nonzero & z_nonzero);
            selene_cmov(r, &fresh, nonzero & (1u - z_nonzero));
        }
    }

    /* Secure erase */
    helioselene_secure_erase(all_digits.data(), all_digits.size());
}

#endif // HELIOSELENE_SELENE_MSM_FIXED_H
