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

/*
 * AVX-512 IFMA constant-time Shaw scalar multiplication.
 *
 * The inner loop uses shaw_dbl_ifma and shaw_madd_ifma (see
 * shaw/include/x64/ifma/shaw_point_ops_ifma.h) which are built on the 2-way
 * lane-packed fq51x2 primitives in fq/include/x64/ifma/fq51x2_ifma.h. The
 * accumulator flows as fq_fe across ops (Option A from plan
 * radiant-soaring-pavilion.md); each point op packs into fq51x2_t on entry
 * and unpacks on exit, amortising the pack tax across the 5–7 SIMD ops that
 * each issues. MSVC and non-AVX512IFMA builds keep the scalar x64 path via
 * the fallback at the bottom.
 */

#include "shaw_scalarmult.h"

#include "fq_invert.h"
#include "fq_mul.h"
#include "fq_ops.h"
#include "fq_sq.h"
#include "ranshaw_ct_barrier.h"
#include "ranshaw_platform.h"
#include "ranshaw_secure_erase.h"
#include "shaw.h"
#include "shaw_add.h"
#include "shaw_dbl.h"
#include "shaw_madd.h"
#include "shaw_ops.h"

/*
 * The 2-way fq51x2 IFMA constant-time scalarmult is retired: it is slower than
 * the x64 ADX scalarmult (autotune never selected it; IFMA's parallelism does
 * not help a single-scalar multiply) and was not ported to the native 4x64
 * fq_fe representation. shaw_scalarmult_ifma therefore delegates to the x64
 * scalarmult on all targets. The fq51x2 field primitives remain available for
 * the 2-way unit tests; only this single-scalar driver is retired.
 */
#define RANSHAW_SHAW_IFMA_HAVE_FQ51X2 0

#include <vector>

/* ------------------------------------------------------------------ */
/* Scalar recoding (signed 4-bit, 64 digits)                          */
/* ------------------------------------------------------------------ */

[[maybe_unused]] static void scalar_recode_signed4(int8_t digits[64], const unsigned char scalar[32])
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
        int val = nibbles[i] + carry;
        carry = (val + 8) >> 4;
        digits[i] = (int8_t)(val - (ranshaw_shl_i32(carry, 4)));
    }
    digits[63] = (int8_t)(nibbles[63] + carry);
    ranshaw_secure_erase(nibbles, sizeof(nibbles));
}

/* ------------------------------------------------------------------ */
/* Batch affine conversion (fq51, single inversion)                   */
/* ------------------------------------------------------------------ */

[[maybe_unused]] static void batch_to_affine(shaw_affine *out, const shaw_jacobian *in, size_t n)
{
    if (n == 0)
        return;

    struct fq_fe_s
    {
        fq_fe v;
    };
    std::vector<fq_fe_s> z_vals(n);
    std::vector<fq_fe_s> products(n);

    for (size_t i = 0; i < n; i++)
        fq_copy(z_vals[i].v, in[i].Z);

    fq_copy(products[0].v, z_vals[0].v);
    for (size_t i = 1; i < n; i++)
        fq_mul(products[i].v, products[i - 1].v, z_vals[i].v);

    fq_fe inv;
    fq_invert(inv, products[n - 1].v);

    for (size_t i = n - 1; i > 0; i--)
    {
        fq_fe z_inv;
        fq_mul(z_inv, inv, products[i - 1].v);
        fq_mul(inv, inv, z_vals[i].v);

        fq_fe z_inv2, z_inv3;
        fq_sq(z_inv2, z_inv);
        fq_mul(z_inv3, z_inv2, z_inv);
        fq_mul(out[i].x, in[i].X, z_inv2);
        fq_mul(out[i].y, in[i].Y, z_inv3);
    }

    {
        fq_fe z_inv2, z_inv3;
        fq_sq(z_inv2, inv);
        fq_mul(z_inv3, z_inv2, inv);
        fq_mul(out[0].x, in[0].X, z_inv2);
        fq_mul(out[0].y, in[0].Y, z_inv3);
    }

    ranshaw_secure_erase(&inv, sizeof(inv));
    ranshaw_secure_erase(z_vals.data(), n * sizeof(fq_fe_s));
    ranshaw_secure_erase(products.data(), n * sizeof(fq_fe_s));
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

void shaw_scalarmult_ifma(shaw_jacobian *r, const unsigned char scalar[32], const shaw_jacobian *p)
{
#if RANSHAW_SHAW_IFMA_HAVE_FQ51X2
    /* Step 1: Precompute Jacobian table [1P, 2P, ..., 8P] using the x64
     * scalar helpers — this is off the hot path (8 ops vs 63*5 = 315 in the
     * main loop), so an IFMA port of shaw_add isn't worthwhile here. */
    shaw_jacobian table_jac[8];
    shaw_copy(&table_jac[0], p);
    shaw_dbl_x64(&table_jac[1], p);
    shaw_add_x64(&table_jac[2], &table_jac[1], p);
    shaw_dbl_x64(&table_jac[3], &table_jac[1]);
    shaw_add_x64(&table_jac[4], &table_jac[3], p);
    shaw_dbl_x64(&table_jac[5], &table_jac[2]);
    shaw_add_x64(&table_jac[6], &table_jac[5], p);
    shaw_dbl_x64(&table_jac[7], &table_jac[3]);

    /* Step 2: Batch-convert the table to affine (single inversion). */
    shaw_affine table[8];
    batch_to_affine(table, table_jac, 8);

    /* Step 3: Recode the scalar into signed 4-bit digits. */
    int8_t digits[64];
    scalar_recode_signed4(digits, scalar);

    /* Step 4: Initialise r from the top digit. */
    int32_t d = (int32_t)digits[63];
    int32_t sign_mask = -(int32_t)((uint32_t)d >> 31);
    unsigned int abs_d = (unsigned int)((d ^ sign_mask) - sign_mask);
    unsigned int neg = (unsigned int)(sign_mask & 1);

    shaw_affine selected;
    fq_0(selected.x);
    fq_0(selected.y);

    for (unsigned int j = 0; j < 8; j++)
    {
        unsigned int eq = ((abs_d ^ (j + 1)) - 1u) >> 31;
        shaw_affine_cmov(&selected, &table[j], eq);
    }

    shaw_affine_cneg(&selected, neg);

    shaw_jacobian from_table;
    shaw_from_affine(&from_table, &selected);

    shaw_jacobian ident;
    shaw_identity(&ident);

    unsigned int nonzero = 1u ^ ((abs_d - 1u) >> 31);
    shaw_copy(r, &ident);
    shaw_cmov(r, &from_table, nonzero);

    /* Step 5: Packed inner loop — accumulator stays in fq51x2_t across the
     * whole 63 × (4·dbl + madd) block, eliminating per-op pack/unpack. Both
     * lanes hold the same value (broadcast invariant) throughout. */
    fq51x2_t accX, accY, accZ;
    fq51x2_broadcast(accX, r->X);
    fq51x2_broadcast(accY, r->Y);
    fq51x2_broadcast(accZ, r->Z);

    fq51x2_t tmpX, tmpY, tmpZ;
    fq51x2_t selX, selY;

    static const fq_fe ONE_FE = {1, 0, 0, 0};
    fq51x2_t one_packed;
    fq51x2_broadcast(one_packed, ONE_FE);

    RANSHAW_NO_VECTOR
    for (int i = 62; i >= 0; i--)
    {
        shaw_dbl_ifma_packed(accX, accY, accZ, accX, accY, accZ);
        shaw_dbl_ifma_packed(accX, accY, accZ, accX, accY, accZ);
        shaw_dbl_ifma_packed(accX, accY, accZ, accX, accY, accZ);
        shaw_dbl_ifma_packed(accX, accY, accZ, accX, accY, accZ);

        d = (int32_t)digits[i];
        sign_mask = -(int32_t)((uint32_t)d >> 31);
        abs_d = (unsigned int)((d ^ sign_mask) - sign_mask);
        neg = (unsigned int)(sign_mask & 1);

        /* Init to a valid on-curve point so off-curve intermediates never
         * appear in memory (cmov sweep overwrites when |digit|!=0). */
        shaw_affine_copy(&selected, &table[0]);
        for (unsigned int j = 0; j < 8; j++)
        {
            unsigned int eq = ((abs_d ^ (j + 1)) - 1u) >> 31;
            shaw_affine_cmov(&selected, &table[j], eq);
        }

        shaw_affine_cneg(&selected, neg);

        nonzero = 1u ^ ((abs_d - 1u) >> 31);

        /* CT isnonzero on Z: unpack lane 0 once and call fq_isnonzero. */
        fq_fe accZ_unpacked, discard;
        fq51x2_to_pair(accZ_unpacked, discard, accZ);
        unsigned int z_nonzero = (unsigned int)fq_isnonzero(accZ_unpacked);

        fq51x2_broadcast(selX, selected.x);
        fq51x2_broadcast(selY, selected.y);

        shaw_madd_ifma_packed(tmpX, tmpY, tmpZ, accX, accY, accZ, selX, selY);

        unsigned int c_madd = nonzero & z_nonzero;
        unsigned int c_fresh = nonzero & (1u - z_nonzero);

        fq51x2_cmov(accX, tmpX, c_madd);
        fq51x2_cmov(accY, tmpY, c_madd);
        fq51x2_cmov(accZ, tmpZ, c_madd);

        fq51x2_cmov(accX, selX, c_fresh);
        fq51x2_cmov(accY, selY, c_fresh);
        fq51x2_cmov(accZ, one_packed, c_fresh);
    }

    /* Unpack accumulator back to r (lane 0 holds the result). */
    {
        fq_fe d_unpack;
        fq51x2_to_pair(r->X, d_unpack, accX);
        fq51x2_to_pair(r->Y, d_unpack, accY);
        fq51x2_to_pair(r->Z, d_unpack, accZ);
    }

    ranshaw_secure_erase(accX, sizeof(accX));
    ranshaw_secure_erase(accY, sizeof(accY));
    ranshaw_secure_erase(accZ, sizeof(accZ));
    ranshaw_secure_erase(tmpX, sizeof(tmpX));
    ranshaw_secure_erase(tmpY, sizeof(tmpY));
    ranshaw_secure_erase(tmpZ, sizeof(tmpZ));
    ranshaw_secure_erase(selX, sizeof(selX));
    ranshaw_secure_erase(selY, sizeof(selY));
    ranshaw_secure_erase(&selected, sizeof(selected));
    ranshaw_secure_erase(one_packed, sizeof(one_packed));
    ranshaw_secure_erase(&from_table, sizeof(from_table));
    ranshaw_secure_erase(&ident, sizeof(ident));
    ranshaw_secure_erase(table_jac, sizeof(table_jac));
    ranshaw_secure_erase(table, sizeof(table));
    ranshaw_secure_erase(digits, sizeof(digits));
#else
    /* No IFMA compile support — fall back to the x64 scalar path so this
     * TU stays linkable. Dispatch normally selects x64 for this CPU config
     * anyway; this branch is purely a safety net for toolchains that
     * compile the IFMA TU without IFMA macros set. */
    extern void shaw_scalarmult_x64(shaw_jacobian *, const unsigned char[32], const shaw_jacobian *);
    shaw_scalarmult_x64(r, scalar, p);
#endif
}
