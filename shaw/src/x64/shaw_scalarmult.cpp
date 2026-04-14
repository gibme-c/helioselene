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
#include "x64/fq51_chain.h"
#include "x64/shaw_inner_packed.h"

#include <vector>

/*
 * Constant-time scalar multiplication for Shaw (over F_q).
 * Same algorithm as ran_scalarmult but using fq_* field ops.
 *
 * cmov_4x64 and unpack_and_normalize helpers live in shaw_inner_packed.h
 * so scalarmult_vartime and msm_vartime can share them.
 */

static void scalar_recode_signed4(int8_t digits[64], const unsigned char scalar[32])
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

#if !defined(FQ51_HAVE_ADX_MUL)
static void batch_to_affine(shaw_affine *out, const shaw_jacobian *in, size_t n)
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
#endif /* !FQ51_HAVE_ADX_MUL */

#if defined(FQ51_HAVE_ADX_MUL)
namespace
{

    /* Non-inline wrappers around fq64_mul / fq64_sq. The inline asm in those
     * helpers clobbers almost every general-purpose register, so inlining
     * them ~38 times in a single function body (as batch_to_affine_packed
     * does) exceeds what the register allocator can resolve and triggers
     * an "'asm' operand has impossible constraints" error. Going through
     * non-inline wrappers isolates each asm block into its own stack frame
     * with a clean register allocator, at a cost of one function-call
     * overhead per fq64 op (~5 cycles), which is far cheaper than the
     * per-call pack/unpack tax we are eliminating. */
    __attribute__((noinline)) static void fq64_mul_ni(uint64_t r[4], const uint64_t a[4], const uint64_t b[4])
    {
        fq64_mul(r, a, b);
    }

    __attribute__((noinline)) static void fq64_sq_ni(uint64_t r[4], const uint64_t a[4])
    {
        fq64_sq(r, a);
    }

    /*
     * Packed batch affine conversion using Montgomery's trick on n=8
     * Jacobian points held in 4x64. The cascade chain + back-substitution
     * runs entirely in 4x64 (7 + 1 + 23 fq64_muls + 8 fq64_sqs); fq_invert
     * has no packed entry point, so it bridges through a single 4x64->5x51
     * unpack of products[n-1] followed by one 5x51->4x64 repack of the
     * inverse. The resulting shaw_affine output is in 5x51 to keep the CT
     * lookup / cmov sweep in shaw_scalarmult_x64 unchanged.
     */
    __attribute__((noinline)) static void batch_to_affine_packed(shaw_affine *out, const packed_jac *in, size_t n)
    {
        if (n == 0)
            return;

        struct packed_fq
        {
            uint64_t v[4];
        };
        std::vector<packed_fq> z_vals(n);
        std::vector<packed_fq> products(n);

        for (size_t i = 0; i < n; i++)
            std::memcpy(z_vals[i].v, in[i].Z, sizeof(z_vals[i].v));

        std::memcpy(products[0].v, z_vals[0].v, sizeof(products[0].v));
        for (size_t i = 1; i < n; i++)
            fq64_mul_ni(products[i].v, products[i - 1].v, z_vals[i].v);

        /* Bridge to fq_invert through 5x51: one unpack of products[n-1]
         * and one repack of the result. This replaces what would otherwise
         * be n pack/unpack pairs per call. */
        fq_fe products_last_5x51, inv_5x51;
        unpack_and_normalize(products_last_5x51, products[n - 1].v);
        fq_invert(inv_5x51, products_last_5x51);
        uint64_t inv[4];
        fq51_normalize_and_pack(inv, inv_5x51);

        for (size_t i = n - 1; i > 0; i--)
        {
            uint64_t z_inv[4];
            fq64_mul_ni(z_inv, inv, products[i - 1].v);
            fq64_mul_ni(inv, inv, z_vals[i].v);

            uint64_t z_inv2[4], z_inv3[4];
            fq64_sq_ni(z_inv2, z_inv);
            fq64_mul_ni(z_inv3, z_inv2, z_inv);

            uint64_t ox[4], oy[4];
            fq64_mul_ni(ox, in[i].X, z_inv2);
            fq64_mul_ni(oy, in[i].Y, z_inv3);
            unpack_and_normalize(out[i].x, ox);
            unpack_and_normalize(out[i].y, oy);
        }

        {
            uint64_t z_inv2[4], z_inv3[4];
            fq64_sq_ni(z_inv2, inv);
            fq64_mul_ni(z_inv3, z_inv2, inv);

            uint64_t ox[4], oy[4];
            fq64_mul_ni(ox, in[0].X, z_inv2);
            fq64_mul_ni(oy, in[0].Y, z_inv3);
            unpack_and_normalize(out[0].x, ox);
            unpack_and_normalize(out[0].y, oy);
        }

        ranshaw_secure_erase(products_last_5x51, sizeof(products_last_5x51));
        ranshaw_secure_erase(inv_5x51, sizeof(inv_5x51));
        ranshaw_secure_erase(inv, sizeof(inv));
        ranshaw_secure_erase(z_vals.data(), n * sizeof(packed_fq));
        ranshaw_secure_erase(products.data(), n * sizeof(packed_fq));
    }

} /* anonymous namespace */
#endif /* FQ51_HAVE_ADX_MUL */

void shaw_scalarmult_x64(shaw_jacobian *r, const unsigned char scalar[32], const shaw_jacobian *p)
{
#if defined(FQ51_HAVE_ADX_MUL)
    packed_jac table_jac[8];
    pack_jac(&table_jac[0], p);
    shaw_dbl_x64_packed(table_jac[1].X, table_jac[1].Y, table_jac[1].Z, table_jac[0].X, table_jac[0].Y, table_jac[0].Z);
    shaw_add_x64_packed(
        table_jac[2].X,
        table_jac[2].Y,
        table_jac[2].Z,
        table_jac[1].X,
        table_jac[1].Y,
        table_jac[1].Z,
        table_jac[0].X,
        table_jac[0].Y,
        table_jac[0].Z);
    shaw_dbl_x64_packed(table_jac[3].X, table_jac[3].Y, table_jac[3].Z, table_jac[1].X, table_jac[1].Y, table_jac[1].Z);
    shaw_add_x64_packed(
        table_jac[4].X,
        table_jac[4].Y,
        table_jac[4].Z,
        table_jac[3].X,
        table_jac[3].Y,
        table_jac[3].Z,
        table_jac[0].X,
        table_jac[0].Y,
        table_jac[0].Z);
    shaw_dbl_x64_packed(table_jac[5].X, table_jac[5].Y, table_jac[5].Z, table_jac[2].X, table_jac[2].Y, table_jac[2].Z);
    shaw_add_x64_packed(
        table_jac[6].X,
        table_jac[6].Y,
        table_jac[6].Z,
        table_jac[5].X,
        table_jac[5].Y,
        table_jac[5].Z,
        table_jac[0].X,
        table_jac[0].Y,
        table_jac[0].Z);
    shaw_dbl_x64_packed(table_jac[7].X, table_jac[7].Y, table_jac[7].Z, table_jac[3].X, table_jac[3].Y, table_jac[3].Z);

    shaw_affine table[8];
    batch_to_affine_packed(table, table_jac, 8);
#else
    shaw_jacobian table_jac[8];
    shaw_copy(&table_jac[0], p);
    shaw_dbl(&table_jac[1], p);
    shaw_add(&table_jac[2], &table_jac[1], p);
    shaw_dbl(&table_jac[3], &table_jac[1]);
    shaw_add(&table_jac[4], &table_jac[3], p);
    shaw_dbl(&table_jac[5], &table_jac[2]);
    shaw_add(&table_jac[6], &table_jac[5], p);
    shaw_dbl(&table_jac[7], &table_jac[3]);

    shaw_affine table[8];
    batch_to_affine(table, table_jac, 8);
#endif

    int8_t digits[64];
    scalar_recode_signed4(digits, scalar);

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

    /* CT conditional negate + select identity or table point */
    shaw_affine_cneg(&selected, neg);

    shaw_jacobian from_table;
    shaw_from_affine(&from_table, &selected);

    shaw_jacobian ident;
    shaw_identity(&ident);

    unsigned int nonzero = 1u ^ ((abs_d - 1u) >> 31);
    shaw_copy(r, &ident);
    shaw_cmov(r, &from_table, nonzero);

#if defined(FQ51_HAVE_ADX_MUL)
    /*
     * Packed inner loop: keep the Jacobian accumulator in 4×64 across all
     * 63 × (dbl×4 + madd) iterations, avoiding pack/unpack at each op boundary.
     * The affine `selected` is still selected into a 5×51 local (so the cmov
     * sweep can reuse shaw_affine_cmov unchanged), then packed once per
     * iteration into selX/selY for the madd + fresh handling.
     */
    uint64_t accX[4], accY[4], accZ[4];
    fq51_normalize_and_pack(accX, r->X);
    fq51_normalize_and_pack(accY, r->Y);
    fq51_normalize_and_pack(accZ, r->Z);

    static const uint64_t FQ64_ONE[4] = {1, 0, 0, 0};

    uint64_t tmpX[4], tmpY[4], tmpZ[4];
    uint64_t selX[4], selY[4];

    RANSHAW_NO_VECTOR
    for (int i = 62; i >= 0; i--)
    {
        shaw_dbl_x64_packed(accX, accY, accZ, accX, accY, accZ);
        shaw_dbl_x64_packed(accX, accY, accZ, accX, accY, accZ);
        shaw_dbl_x64_packed(accX, accY, accZ, accX, accY, accZ);
        shaw_dbl_x64_packed(accX, accY, accZ, accX, accY, accZ);

        d = (int32_t)digits[i];
        sign_mask = -(int32_t)((uint32_t)d >> 31);
        abs_d = (unsigned int)((d ^ sign_mask) - sign_mask);
        neg = (unsigned int)(sign_mask & 1);

        /* Valid-curve-point init (cosmetic; see non-packed branch rationale). */
        shaw_affine_copy(&selected, &table[0]);
        for (unsigned int j = 0; j < 8; j++)
        {
            unsigned int eq = ((abs_d ^ (j + 1)) - 1u) >> 31;
            shaw_affine_cmov(&selected, &table[j], eq);
        }

        shaw_affine_cneg(&selected, neg);

        nonzero = 1u ^ ((abs_d - 1u) >> 31);
        unsigned int z_nonzero = (unsigned int)fq64_isnonzero(accZ);

        fq51_normalize_and_pack(selX, selected.x);
        fq51_normalize_and_pack(selY, selected.y);

        shaw_madd_x64_packed(tmpX, tmpY, tmpZ, accX, accY, accZ, selX, selY);

        /* fresh = (selX, selY, 1) — the Jacobian lift of the selected affine,
         * used when the accumulator is identity (Z=0) and madd's formula is
         * not applicable. */
        cmov_4x64(accX, tmpX, nonzero & z_nonzero);
        cmov_4x64(accY, tmpY, nonzero & z_nonzero);
        cmov_4x64(accZ, tmpZ, nonzero & z_nonzero);

        cmov_4x64(accX, selX, nonzero & (1u - z_nonzero));
        cmov_4x64(accY, selY, nonzero & (1u - z_nonzero));
        cmov_4x64(accZ, FQ64_ONE, nonzero & (1u - z_nonzero));
    }

    unpack_and_normalize(r->X, accX);
    unpack_and_normalize(r->Y, accY);
    unpack_and_normalize(r->Z, accZ);

    ranshaw_secure_erase(accX, sizeof(accX));
    ranshaw_secure_erase(accY, sizeof(accY));
    ranshaw_secure_erase(accZ, sizeof(accZ));
    ranshaw_secure_erase(tmpX, sizeof(tmpX));
    ranshaw_secure_erase(tmpY, sizeof(tmpY));
    ranshaw_secure_erase(tmpZ, sizeof(tmpZ));
    ranshaw_secure_erase(selX, sizeof(selX));
    ranshaw_secure_erase(selY, sizeof(selY));
#else
    shaw_jacobian tmp, fresh;
    RANSHAW_NO_VECTOR
    for (int i = 62; i >= 0; i--)
    {
        shaw_dbl(r, r);
        shaw_dbl(r, r);
        shaw_dbl(r, r);
        shaw_dbl(r, r);

        d = (int32_t)digits[i];
        sign_mask = -(int32_t)((uint32_t)d >> 31);
        abs_d = (unsigned int)((d ^ sign_mask) - sign_mask);
        neg = (unsigned int)(sign_mask & 1);

        /* CT table lookup: init to table[0] (a valid on-curve point, 1*P).
         * Cosmetic: the subsequent cmov sweep overwrites `selected` whenever
         * |digit| != 0, and when digit == 0 the madd result is discarded by
         * the identity-accumulator cmov below. Using a valid curve point here
         * avoids having off-curve intermediates ever appear in memory. */
        shaw_affine_copy(&selected, &table[0]);
        for (unsigned int j = 0; j < 8; j++)
        {
            unsigned int eq = ((abs_d ^ (j + 1)) - 1u) >> 31;
            shaw_affine_cmov(&selected, &table[j], eq);
        }

        shaw_affine_cneg(&selected, neg);

        nonzero = 1u ^ ((abs_d - 1u) >> 31);
        unsigned int z_nonzero = (unsigned int)fq_isnonzero(r->Z);

        shaw_madd(&tmp, r, &selected);

        shaw_from_affine(&fresh, &selected);

        shaw_cmov(r, &tmp, nonzero & z_nonzero);
        shaw_cmov(r, &fresh, nonzero & (1u - z_nonzero));
    }

    ranshaw_secure_erase(&tmp, sizeof(tmp));
    ranshaw_secure_erase(&fresh, sizeof(fresh));
#endif /* FQ51_HAVE_ADX_MUL */

    ranshaw_secure_erase(&selected, sizeof(selected));
    ranshaw_secure_erase(&from_table, sizeof(from_table));
    ranshaw_secure_erase(&ident, sizeof(ident));
    ranshaw_secure_erase(table_jac, sizeof(table_jac));
    ranshaw_secure_erase(table, sizeof(table));
    ranshaw_secure_erase(digits, sizeof(digits));
}
