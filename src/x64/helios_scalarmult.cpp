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

#include "helios_scalarmult.h"

#include "fp_invert.h"
#include "fp_mul.h"
#include "fp_ops.h"
#include "fp_sq.h"
#include "helios.h"
#include "helios_add.h"
#include "helios_dbl.h"
#include "helios_madd.h"
#include "helios_ops.h"
#include "helioselene_secure_erase.h"

/*
 * Constant-time scalar multiplication using signed 4-bit fixed-window (radix-16).
 *
 * Algorithm:
 *   1. Recode 256-bit scalar to 64 signed digits in {-8,...,8}
 *   2. Precompute table: [P, 2P, 3P, 4P, 5P, 6P, 7P, 8P] in affine
 *   3. Main loop (i = 63 down to 0):
 *      - 4 doublings
 *      - CT table lookup
 *      - CT conditional negate
 *      - Mixed addition
 *   4. Secure erase intermediates
 */

/*
 * Recode scalar into signed 4-bit digits.
 * Input: 256-bit scalar as 32 bytes LE
 * Output: 64 signed digits in [-8, 8], with carry absorbed
 *
 * Each digit d[i] represents bits [4i, 4i+3] with a borrow/carry scheme
 * such that scalar = sum(d[i] * 16^i).
 */
static void scalar_recode_signed4(int8_t digits[64], const unsigned char scalar[32])
{
    /* Extract 4-bit nibbles */
    uint8_t nibbles[64];
    for (int i = 0; i < 32; i++)
    {
        nibbles[2 * i] = scalar[i] & 0x0f;
        nibbles[2 * i + 1] = (scalar[i] >> 4) & 0x0f;
    }

    /* Convert to signed: if nibble > 8, subtract 16 and carry +1 to next */
    int carry = 0;
    for (int i = 0; i < 63; i++)
    {
        int val = nibbles[i] + carry;
        carry = 0;
        if (val > 8)
        {
            val -= 16;
            carry = 1;
        }
        digits[i] = (int8_t)val;
    }
    digits[63] = (int8_t)(nibbles[63] + carry);
}

/*
 * Batch affine conversion using Montgomery's trick.
 * Converts n Jacobian points to affine using a single inversion.
 */
static void batch_to_affine(helios_affine *out, const helios_jacobian *in, int n)
{
    if (n == 0)
        return;

    fp_fe *z_vals = new fp_fe[n];
    fp_fe *products = new fp_fe[n];

    /* Collect Z values */
    for (int i = 0; i < n; i++)
        fp_copy(z_vals[i], in[i].Z);

    /* Compute cumulative products: products[i] = z[0] * z[1] * ... * z[i] */
    fp_copy(products[0], z_vals[0]);
    for (int i = 1; i < n; i++)
        fp_mul(products[i], products[i - 1], z_vals[i]);

    /* Invert the cumulative product */
    fp_fe inv;
    fp_invert(inv, products[n - 1]);

    /* Work backwards to get individual inverses */
    for (int i = n - 1; i > 0; i--)
    {
        fp_fe z_inv;
        fp_mul(z_inv, inv, products[i - 1]); /* z_inv = inv * products[i-1] = 1/z[i] */
        fp_mul(inv, inv, z_vals[i]); /* inv = inv * z[i] = 1/(z[0]*...*z[i-1]) */

        fp_fe z_inv2, z_inv3;
        fp_sq(z_inv2, z_inv);
        fp_mul(z_inv3, z_inv2, z_inv);
        fp_mul(out[i].x, in[i].X, z_inv2);
        fp_mul(out[i].y, in[i].Y, z_inv3);
    }

    /* First element: inv is now 1/z[0] */
    {
        fp_fe z_inv2, z_inv3;
        fp_sq(z_inv2, inv);
        fp_mul(z_inv3, z_inv2, inv);
        fp_mul(out[0].x, in[0].X, z_inv2);
        fp_mul(out[0].y, in[0].Y, z_inv3);
    }

    helioselene_secure_erase(z_vals, n * sizeof(fp_fe));
    helioselene_secure_erase(products, n * sizeof(fp_fe));
    delete[] z_vals;
    delete[] products;
}

void helios_scalarmult_x64(helios_jacobian *r, const unsigned char scalar[32], const helios_jacobian *p)
{
    /* Step 1: Precompute table [P, 2P, 3P, 4P, 5P, 6P, 7P, 8P] */
    helios_jacobian table_jac[8];
    helios_copy(&table_jac[0], p); /* 1P */
    helios_dbl(&table_jac[1], p); /* 2P */
    helios_add(&table_jac[2], &table_jac[1], p); /* 3P */
    helios_dbl(&table_jac[3], &table_jac[1]); /* 4P */
    helios_add(&table_jac[4], &table_jac[3], p); /* 5P */
    helios_dbl(&table_jac[5], &table_jac[2]); /* 6P */
    helios_add(&table_jac[6], &table_jac[5], p); /* 7P */
    helios_dbl(&table_jac[7], &table_jac[3]); /* 8P */

    /* Convert to affine (single inversion) */
    helios_affine table[8];
    batch_to_affine(table, table_jac, 8);

    /* Step 2: Recode scalar */
    int8_t digits[64];
    scalar_recode_signed4(digits, scalar);

    /* Step 3: Main loop — start from the top digit */
    /* Initialize with the top digit (branchless abs + sign extraction) */
    int32_t d = (int32_t)digits[63];
    int32_t sign_mask = d >> 31;
    unsigned int abs_d = (unsigned int)((d ^ sign_mask) - sign_mask);
    unsigned int neg = (unsigned int)(sign_mask & 1);

    /* CT table lookup for initial value */
    helios_affine selected;
    fp_0(selected.x);
    fp_0(selected.y);

    for (unsigned int j = 0; j < 8; j++)
    {
        unsigned int eq = ((abs_d ^ (j + 1)) - 1u) >> 31;
        /* eq = 1 if abs_d == j+1, 0 otherwise */
        helios_affine_cmov(&selected, &table[j], eq);
    }

    /* CT conditional negate + select identity or table point */
    helios_affine_cneg(&selected, neg);

    helios_jacobian from_table;
    helios_from_affine(&from_table, &selected);

    helios_jacobian ident;
    helios_identity(&ident);

    unsigned int nonzero = 1u ^ ((abs_d - 1u) >> 31);
    helios_copy(r, &ident);
    helios_cmov(r, &from_table, nonzero);

    /* Main loop: digits[62] down to digits[0] */
    for (int i = 62; i >= 0; i--)
    {
        /* 4 doublings */
        helios_dbl(r, r);
        helios_dbl(r, r);
        helios_dbl(r, r);
        helios_dbl(r, r);

        /* Extract digit (branchless) */
        d = (int32_t)digits[i];
        sign_mask = d >> 31;
        abs_d = (unsigned int)((d ^ sign_mask) - sign_mask);
        neg = (unsigned int)(sign_mask & 1);

        /* CT table lookup */
        fp_1(selected.x);
        fp_1(selected.y);
        for (unsigned int j = 0; j < 8; j++)
        {
            unsigned int eq = ((abs_d ^ (j + 1)) - 1u) >> 31;
            helios_affine_cmov(&selected, &table[j], eq);
        }

        /* CT conditional negate */
        helios_affine_cneg(&selected, neg);

        /* Mixed addition if digit != 0 */
        nonzero = 1u ^ ((abs_d - 1u) >> 31);

        /* Handle identity accumulator: madd(identity, P) is degenerate.
         * If Z==0 (identity), use from_affine instead. */
        unsigned int z_nonzero = (unsigned int)fp_isnonzero(r->Z);

        helios_jacobian tmp;
        helios_madd(&tmp, r, &selected);

        helios_jacobian fresh;
        helios_from_affine(&fresh, &selected);

        /* If digit nonzero and accumulator is valid (Z!=0): use madd result */
        helios_cmov(r, &tmp, nonzero & z_nonzero);
        /* If digit nonzero and accumulator is identity (Z==0): use from_affine */
        helios_cmov(r, &fresh, nonzero & (1u - z_nonzero));
    }

    /* Secure erase */
    helioselene_secure_erase(table_jac, sizeof(table_jac));
    helioselene_secure_erase(table, sizeof(table));
    helioselene_secure_erase(digits, sizeof(digits));
}
