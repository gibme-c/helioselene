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
 * @file fp_divsteps.h
 * @brief Portable (32-bit, radix-2^25.5) implementation of F_p divsteps inversion.
 *
 * Bernstein-Yang safegcd/divsteps modular inversion for F_p, p = 2^255 - 19
 * (Curve25519 base field). Mirrors the x64 implementation in
 * fp/include/x64/fp_divsteps.h but operates entirely in int32_t/int64_t so
 * it builds on platforms without __int128 or _umul128. Structurally
 * identical to the sibling fq_divsteps.h; only the modulus constants change.
 *
 * Representation: 9 x int32_t limbs in radix-2^30 ("signed30"). 9 * 30 = 270
 * bits leaves 14 bits of headroom over the 256-bit field.
 *
 * Algorithm: 25 outer iterations of fp_divsteps_30 (30 inner divsteps each)
 * gives 750 total iterations, comfortably above the Bernstein-Yang
 * theoretical bound of ceil((49*255+57)/17) = 738 for a 255-bit modulus.
 *
 * Constant-time: fixed iteration count, no secret-dependent branches or
 * memory access.
 */

#ifndef RANSHAW_PORTABLE_FP_DIVSTEPS_H
#define RANSHAW_PORTABLE_FP_DIVSTEPS_H

#include "fp.h"
#include "portable/fp_frombytes.h"
#include "portable/fp_tobytes.h"
#include "ranshaw_platform.h"

#include <cstdint>

/* ------------------------------------------------------------------ */
/* Types                                                              */
/* ------------------------------------------------------------------ */

struct fp_signed30
{
    int32_t v[9]; /* 9 x 30-bit signed limbs, radix 2^30 */
};

struct fp_trans2x2_30
{
    int32_t u, v, q, r; /* 2x2 transition matrix, entries bounded by 2^30 */
};

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

static constexpr uint32_t FP_M30 = (uint32_t(1) << 30) - 1;

/*
 * p = 2^255 - 19 in signed30 representation.
 *   v[0] = (1<<30) - 19 = 0x3FFFFFED (low 30 bits of p)
 *   v[1..7] = 0x3FFFFFFF (all 30-bit limbs between, fully set)
 *   v[8] = 0x7FFF (top 15 bits; p has its MSB at bit 254)
 */
static constexpr fp_signed30 FP_MODULUS_S30 = {{
    (int32_t)0x3FFFFFED,
    (int32_t)0x3FFFFFFF,
    (int32_t)0x3FFFFFFF,
    (int32_t)0x3FFFFFFF,
    (int32_t)0x3FFFFFFF,
    (int32_t)0x3FFFFFFF,
    (int32_t)0x3FFFFFFF,
    (int32_t)0x3FFFFFFF,
    (int32_t)0x00007FFF,
}};

/*
 * -p[0]^{-1} mod 2^30, for the modular update step.
 * Computed via Newton's method (Hensel lifting).
 */
static inline constexpr uint32_t fp_compute_modinv30(uint32_t x)
{
    uint32_t inv = 1;
    inv *= 2 - x * inv; /* mod 4 */
    inv *= 2 - x * inv; /* mod 16 */
    inv *= 2 - x * inv; /* mod 256 */
    inv *= 2 - x * inv; /* mod 2^16 */
    inv *= 2 - x * inv; /* mod 2^32 */
    return inv;
}

static constexpr int32_t FP_NEG_PINV30 = (int32_t)((0u - fp_compute_modinv30((uint32_t)FP_MODULUS_S30.v[0])) & FP_M30);

/* Sanity: p[0] * (-p[0])^{-1} == 1 (mod 2^30). */
static_assert(
    (((uint32_t)FP_MODULUS_S30.v[0] * (0u - (uint32_t)FP_NEG_PINV30)) & FP_M30) == 1u,
    "FP_NEG_PINV30 Hensel lift inconsistent with FP_MODULUS_S30.v[0]");

/* ------------------------------------------------------------------ */
/* Inner loop: 30 divsteps on low bits                                */
/* ------------------------------------------------------------------ */

static inline int32_t fp_divsteps_30(int32_t delta, uint32_t f0, uint32_t g0, fp_trans2x2_30 *t)
{
    int32_t u = 1, v = 0, q = 0, r = 1;
    uint32_t f = f0, g = g0;

    for (int i = 0; i < 30; i++)
    {
        int32_t cpos = ~((delta - 1) >> 31);
        int32_t codd = -(int32_t)(g & 1);
        int32_t cond = cpos & codd;

        uint32_t xfg = (f ^ g) & (uint32_t)cond;
        f ^= xfg;
        g ^= xfg;

        int32_t xu = (u ^ q) & cond;
        u ^= xu;
        q ^= xu;
        int32_t xv = (v ^ r) & cond;
        v ^= xv;
        r ^= xv;

        delta = (delta ^ cond) - cond;
        g = (g ^ (uint32_t)cond) - (uint32_t)cond;
        q = (q ^ cond) - cond;
        r = (r ^ cond) - cond;

        delta++;

        int32_t c2 = -(int32_t)(g & 1);
        g += f & (uint32_t)c2;
        q += u & c2;
        r += v & c2;

        g >>= 1;
        u = ranshaw_shl_i32(u, 1);
        v = ranshaw_shl_i32(v, 1);
    }

    t->u = u;
    t->v = v;
    t->q = q;
    t->r = r;
    return delta;
}

/* ------------------------------------------------------------------ */
/* Outer loop: apply transition matrix to full-width f, g             */
/* ------------------------------------------------------------------ */

static inline void fp_update_fg_30(fp_signed30 *f, fp_signed30 *g, const fp_trans2x2_30 *t)
{
    const int64_t u = t->u, v = t->v, q = t->q, r = t->r;

    int64_t af = (int64_t)u * f->v[0] + (int64_t)v * g->v[0];
    int64_t ag = (int64_t)q * f->v[0] + (int64_t)r * g->v[0];
    int64_t cf = af >> 30;
    int64_t cg = ag >> 30;

    int32_t fi[9], gi[9];
    for (int i = 1; i < 9; i++)
    {
        af = cf + (int64_t)u * f->v[i] + (int64_t)v * g->v[i];
        ag = cg + (int64_t)q * f->v[i] + (int64_t)r * g->v[i];
        fi[i - 1] = (int32_t)((uint64_t)af & FP_M30);
        gi[i - 1] = (int32_t)((uint64_t)ag & FP_M30);
        cf = af >> 30;
        cg = ag >> 30;
    }
    fi[8] = (int32_t)cf;
    gi[8] = (int32_t)cg;

    for (int i = 0; i < 9; i++)
    {
        f->v[i] = fi[i];
        g->v[i] = gi[i];
    }
}

/* ------------------------------------------------------------------ */
/* Outer loop: apply transition matrix to d, e (mod p)                */
/* ------------------------------------------------------------------ */

static inline void fp_update_de_30(fp_signed30 *d, fp_signed30 *e, const fp_trans2x2_30 *t)
{
    const int64_t u = t->u, v = t->v, q = t->q, r = t->r;
    int32_t di[9], ei[9];

    uint32_t md = (uint32_t)u * (uint32_t)d->v[0] + (uint32_t)v * (uint32_t)e->v[0];
    uint32_t me = (uint32_t)q * (uint32_t)d->v[0] + (uint32_t)r * (uint32_t)e->v[0];

    int32_t cd = (int32_t)((md * (uint32_t)FP_NEG_PINV30) & FP_M30);
    int32_t ce = (int32_t)((me * (uint32_t)FP_NEG_PINV30) & FP_M30);

    cd = ranshaw_shl_i32(cd, 2) >> 2;
    ce = ranshaw_shl_i32(ce, 2) >> 2;

    int64_t ad = (int64_t)u * d->v[0] + (int64_t)v * e->v[0] + (int64_t)cd * FP_MODULUS_S30.v[0];
    int64_t ae = (int64_t)q * d->v[0] + (int64_t)r * e->v[0] + (int64_t)ce * FP_MODULUS_S30.v[0];
    int64_t cf = ad >> 30;
    int64_t cg = ae >> 30;

    for (int i = 1; i < 9; i++)
    {
        ad = cf + (int64_t)u * d->v[i] + (int64_t)v * e->v[i] + (int64_t)cd * FP_MODULUS_S30.v[i];
        ae = cg + (int64_t)q * d->v[i] + (int64_t)r * e->v[i] + (int64_t)ce * FP_MODULUS_S30.v[i];
        di[i - 1] = (int32_t)((uint64_t)ad & FP_M30);
        ei[i - 1] = (int32_t)((uint64_t)ae & FP_M30);
        cf = ad >> 30;
        cg = ae >> 30;
    }
    di[8] = (int32_t)cf;
    ei[8] = (int32_t)cg;

    for (int i = 0; i < 9; i++)
    {
        d->v[i] = di[i];
        e->v[i] = ei[i];
    }
}

/* ------------------------------------------------------------------ */
/* Normalization: reduce d to [0, p) and convert to fp_fe             */
/* ------------------------------------------------------------------ */

static inline void fp_divsteps_normalize_30(fp_fe out, fp_signed30 *d, const fp_signed30 *f)
{
    int32_t f_neg = f->v[8] >> 31;

    for (int i = 0; i < 9; i++)
        d->v[i] = (d->v[i] ^ f_neg) - f_neg;

    int32_t carry = 0;
    for (int i = 0; i < 8; i++)
    {
        d->v[i] += carry;
        carry = d->v[i] >> 30;
        d->v[i] -= ranshaw_shl_i32(carry, 30);
    }
    d->v[8] += carry;

    int32_t neg_mask = d->v[8] >> 31;
    carry = 0;
    for (int i = 0; i < 9; i++)
    {
        d->v[i] += FP_MODULUS_S30.v[i] & neg_mask;
        carry = d->v[i] >> 30;
        if (i < 8)
        {
            d->v[i] -= ranshaw_shl_i32(carry, 30);
            d->v[i + 1] += carry;
        }
    }

    int32_t tmp[9];
    int32_t borrow = 0;
    for (int i = 0; i < 9; i++)
    {
        tmp[i] = d->v[i] - FP_MODULUS_S30.v[i] - borrow;
        borrow = (tmp[i] >> 31) & 1;
        if (i < 8)
            tmp[i] = (int32_t)((uint32_t)tmp[i] & FP_M30);
    }
    int32_t ge_mask = ~(tmp[8] >> 31);
    for (int i = 0; i < 9; i++)
        d->v[i] = (d->v[i] & ~ge_mask) | (tmp[i] & ge_mask);

    uint32_t w[9];
    for (int i = 0; i < 9; i++)
        w[i] = (uint32_t)d->v[i];

    unsigned char b[32];
    auto pack_byte = [&](int byte_idx) -> unsigned char
    {
        const int bit_lo = byte_idx * 8;
        const int limb_lo = bit_lo / 30;
        const int shift_lo = bit_lo - limb_lo * 30;
        uint32_t lo = w[limb_lo] >> shift_lo;
        if (shift_lo > 22 && limb_lo + 1 < 9)
            lo |= w[limb_lo + 1] << (30 - shift_lo);
        return (unsigned char)(lo & 0xFFu);
    };
    for (int k = 0; k < 32; k++)
        b[k] = pack_byte(k);

    fp_frombytes_portable(out, b);
}

/* ------------------------------------------------------------------ */
/* Conversion: fp_fe (radix-2^25.5) -> signed30                       */
/* ------------------------------------------------------------------ */

static inline void fp_fe_to_signed30(fp_signed30 *s, const fp_fe z)
{
    unsigned char b[32];
    fp_tobytes_portable(b, z);

    auto load32 = [&](int off) -> uint32_t
    {
        return (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8) | ((uint32_t)b[off + 2] << 16)
               | ((uint32_t)b[off + 3] << 24);
    };
    uint32_t w0 = load32(0);
    uint32_t w1 = load32(4);
    uint32_t w2 = load32(8);
    uint32_t w3 = load32(12);
    uint32_t w4 = load32(16);
    uint32_t w5 = load32(20);
    uint32_t w6 = load32(24);
    uint32_t w7 = load32(28);

    s->v[0] = (int32_t)(w0 & FP_M30);
    s->v[1] = (int32_t)(((w0 >> 30) | (w1 << 2)) & FP_M30);
    s->v[2] = (int32_t)(((w1 >> 28) | (w2 << 4)) & FP_M30);
    s->v[3] = (int32_t)(((w2 >> 26) | (w3 << 6)) & FP_M30);
    s->v[4] = (int32_t)(((w3 >> 24) | (w4 << 8)) & FP_M30);
    s->v[5] = (int32_t)(((w4 >> 22) | (w5 << 10)) & FP_M30);
    s->v[6] = (int32_t)(((w5 >> 20) | (w6 << 12)) & FP_M30);
    s->v[7] = (int32_t)(((w6 >> 18) | (w7 << 14)) & FP_M30);
    s->v[8] = (int32_t)(w7 >> 16);
}

#endif // RANSHAW_PORTABLE_FP_DIVSTEPS_H
