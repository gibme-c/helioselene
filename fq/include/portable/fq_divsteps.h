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
 * @file fq_divsteps.h
 * @brief Portable (32-bit, radix-2^25.5) implementation of F_q divsteps inversion.
 *
 * Bernstein-Yang safegcd/divsteps modular inversion for F_q, q = 2^255 - gamma
 * (Crandall prime, gamma ~ 2^127). Mirrors the x64 implementation in
 * fq/include/x64/fq_divsteps.h but operates entirely in int32_t/int64_t so it
 * builds on platforms without __int128 or _umul128.
 *
 * Representation: 9 x int32_t limbs in radix-2^30 ("signed30"). 9 * 30 = 270
 * bits leaves 14 bits of headroom over the 256-bit field. Each limb is
 * nominally in [0, 2^30) but intermediate values may be signed.
 *
 * Algorithm: 25 outer iterations of fq_divsteps_30 (30 inner divsteps each)
 * gives 750 total iterations, comfortably above the Bernstein-Yang theoretical
 * bound of ceil((49*255+57)/17) = 738 for a 255-bit modulus and matching the
 * 744 bound used by the x64 path.
 *
 * Constant-time: fixed iteration count, no secret-dependent branches or
 * memory access. Conditional swaps and negates use XOR / sub-mask idioms,
 * routed through ranshaw_shl_i32 to avoid signed-shift undefined behavior.
 */

#ifndef RANSHAW_PORTABLE_FQ_DIVSTEPS_H
#define RANSHAW_PORTABLE_FQ_DIVSTEPS_H

#include "fq.h"
#include "portable/fq25.h"
#include "portable/fq_frombytes.h"
#include "portable/fq_tobytes.h"
#include "ranshaw_platform.h"

#include <cstdint>

/* ------------------------------------------------------------------ */
/* Types                                                              */
/* ------------------------------------------------------------------ */

struct fq_signed30
{
    int32_t v[9]; /* 9 x 30-bit signed limbs, radix 2^30 */
};

struct fq_trans2x2_30
{
    int32_t u, v, q, r; /* 2x2 transition matrix, entries bounded by 2^30 */
};

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

static constexpr uint32_t FQ_M30 = (uint32_t(1) << 30) - 1;

/*
 * q = 2^255 - gamma in signed30 representation (q-dependent).
 *
 * q bytes (LE) per fq25.h:
 *   5f f8 70 ec 45 46 68 71 a7 0f 73 39 0e eb b1 4b
 *   ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff 7f
 *
 * Repacked into 9 x 30-bit limbs (LE):
 *   v[i] = bits [30*i .. 30*(i+1)-1] of q, with v[8] holding only the top 15
 *   bits since q < 2^255.
 */
static constexpr fq_signed30 FQ_MODULUS_S30 = {{
    (int32_t)0x2C70F85F,
    (int32_t)0x05A11917,
    (int32_t)0x1730FA77,
    (int32_t)0x2C7AC38E,
    (int32_t)0x3FFFFF4B,
    (int32_t)0x3FFFFFFF,
    (int32_t)0x3FFFFFFF,
    (int32_t)0x3FFFFFFF,
    (int32_t)0x00007FFF,
}};

/*
 * -q[0]^{-1} mod 2^30, for the modular update step.
 * Computed via Newton's method (Hensel lifting): starting from inv=1
 * (correct mod 2), each doubling step doubles the precision. 5 steps cover
 * 32 bits of precision; the result is masked back to 30 bits.
 */
static inline constexpr uint32_t fq_compute_modinv30(uint32_t x)
{
    uint32_t inv = 1;
    inv *= 2 - x * inv; /* mod 4 */
    inv *= 2 - x * inv; /* mod 16 */
    inv *= 2 - x * inv; /* mod 256 */
    inv *= 2 - x * inv; /* mod 2^16 */
    inv *= 2 - x * inv; /* mod 2^32 */
    return inv;
}

static constexpr int32_t FQ_NEG_QINV30 = (int32_t)((0u - fq_compute_modinv30((uint32_t)FQ_MODULUS_S30.v[0])) & FQ_M30);

/* Sanity: q[0] * (-q[0])^{-1} == 1 (mod 2^30). */
static_assert(
    (((uint32_t)FQ_MODULUS_S30.v[0] * (0u - (uint32_t)FQ_NEG_QINV30)) & FQ_M30) == 1u,
    "FQ_NEG_QINV30 Hensel lift inconsistent with FQ_MODULUS_S30.v[0]");

/* ------------------------------------------------------------------ */
/* Inner loop: 30 divsteps on low bits                                */
/* ------------------------------------------------------------------ */

/*
 * Perform 30 iterations of the Bernstein-Yang divstep on the low bits
 * of f and g. Returns the new delta and fills in the 2x2 transition
 * matrix t such that:
 *   [new_f]         [t.u  t.v] [old_f]
 *   [new_g] * 2^30 = [t.q  t.r] [old_g]
 *
 * Matrix entries u, v, q, r are bounded by 2^30 (one bit of headroom
 * within int32_t after 30 doublings).
 *
 * All operations are constant-time.
 */
static inline int32_t fq_divsteps_30(int32_t delta, uint32_t f0, uint32_t g0, fq_trans2x2_30 *t)
{
    int32_t u = 1, v = 0, q = 0, r = 1;
    uint32_t f = f0, g = g0;

    for (int i = 0; i < 30; i++)
    {
        /* cond = -1 if (delta > 0 AND g is odd), else 0 */
        int32_t cpos = ~((delta - 1) >> 31); /* -1 if delta > 0, 0 otherwise */
        int32_t codd = -(int32_t)(g & 1); /* -1 if g odd, 0 if even */
        int32_t cond = cpos & codd;

        /* Conditional swap f <-> g */
        uint32_t xfg = (f ^ g) & (uint32_t)cond;
        f ^= xfg;
        g ^= xfg;

        /* Conditional swap matrix rows */
        int32_t xu = (u ^ q) & cond;
        u ^= xu;
        q ^= xu;
        int32_t xv = (v ^ r) & cond;
        v ^= xv;
        r ^= xv;

        /* Conditional negate delta, g, q, r */
        delta = (delta ^ cond) - cond;
        g = (g ^ (uint32_t)cond) - (uint32_t)cond;
        q = (q ^ cond) - cond;
        r = (r ^ cond) - cond;

        /* delta += 1 */
        delta++;

        /* If g is odd: g += f, q += u, r += v */
        int32_t c2 = -(int32_t)(g & 1);
        g += f & (uint32_t)c2;
        q += u & c2;
        r += v & c2;

        /* g >>= 1 (logical, on uint32); double f's row to compensate */
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

/*
 * Apply the 2x2 transition matrix to (f, g):
 *   new_f = (u*f + v*g) / 2^30
 *   new_g = (q*f + r*g) / 2^30
 *
 * Multiplications are 30b * 30b -> 60b, fitting in int64_t with no
 * __int128 dependency.
 */
static inline void fq_update_fg_30(fq_signed30 *f, fq_signed30 *g, const fq_trans2x2_30 *t)
{
    const int64_t u = t->u, v = t->v, q = t->q, r = t->r;

    /* Limb 0: low 30 bits are zero (division by 2^30 is exact), extract carry only */
    int64_t af = (int64_t)u * f->v[0] + (int64_t)v * g->v[0];
    int64_t ag = (int64_t)q * f->v[0] + (int64_t)r * g->v[0];
    int64_t cf = af >> 30;
    int64_t cg = ag >> 30;

    /* Limbs 1-8 of numerator become limbs 0-7 of result (shifted down by 2^30) */
    int32_t fi[9], gi[9];
    for (int i = 1; i < 9; i++)
    {
        af = cf + (int64_t)u * f->v[i] + (int64_t)v * g->v[i];
        ag = cg + (int64_t)q * f->v[i] + (int64_t)r * g->v[i];
        fi[i - 1] = (int32_t)((uint64_t)af & FQ_M30);
        gi[i - 1] = (int32_t)((uint64_t)ag & FQ_M30);
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
/* Outer loop: apply transition matrix to d, e (mod q)                */
/* ------------------------------------------------------------------ */

/*
 * Update d, e: new_d = (u*d + v*e + cd*q) / 2^30 (mod q)
 * where cd is chosen to make the numerator divisible by 2^30.
 */
static inline void fq_update_de_30(fq_signed30 *d, fq_signed30 *e, const fq_trans2x2_30 *t)
{
    const int64_t u = t->u, v = t->v, q = t->q, r = t->r;
    int32_t di[9], ei[9];

    /* Compute cd, ce to ensure divisibility by 2^30.
     * Only the low 30 bits of u*d[0]+v*e[0] matter (higher limbs contribute
     * multiples of 2^30 already). */
    uint32_t md = (uint32_t)u * (uint32_t)d->v[0] + (uint32_t)v * (uint32_t)e->v[0];
    uint32_t me = (uint32_t)q * (uint32_t)d->v[0] + (uint32_t)r * (uint32_t)e->v[0];

    /* cd = -(u*d + v*e) * q[0]^{-1} (mod 2^30) */
    int32_t cd = (int32_t)((md * (uint32_t)FQ_NEG_QINV30) & FQ_M30);
    int32_t ce = (int32_t)((me * (uint32_t)FQ_NEG_QINV30) & FQ_M30);

    /* Sign-extend cd, ce from 30 bits to int32_t so they live in [-2^29, 2^29). */
    cd = ranshaw_shl_i32(cd, 2) >> 2;
    ce = ranshaw_shl_i32(ce, 2) >> 2;

    /* Compute (u*d + v*e + cd*q) / 2^30, limb by limb. Limb 0 of the
     * numerator is zero by construction (that's the point of cd/ce); we
     * extract only the carry from limb 0, then limbs 1-8 become result 0-7. */

    /* Limb 0: extract carry only (low 30 bits are zero by construction) */
    int64_t ad = (int64_t)u * d->v[0] + (int64_t)v * e->v[0] + (int64_t)cd * FQ_MODULUS_S30.v[0];
    int64_t ae = (int64_t)q * d->v[0] + (int64_t)r * e->v[0] + (int64_t)ce * FQ_MODULUS_S30.v[0];
    int64_t cf = ad >> 30;
    int64_t cg = ae >> 30;

    /* Limbs 1-8 of numerator become result limbs 0-7 */
    for (int i = 1; i < 9; i++)
    {
        ad = cf + (int64_t)u * d->v[i] + (int64_t)v * e->v[i] + (int64_t)cd * FQ_MODULUS_S30.v[i];
        ae = cg + (int64_t)q * d->v[i] + (int64_t)r * e->v[i] + (int64_t)ce * FQ_MODULUS_S30.v[i];
        di[i - 1] = (int32_t)((uint64_t)ad & FQ_M30);
        ei[i - 1] = (int32_t)((uint64_t)ae & FQ_M30);
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
/* Normalization: reduce d to [0, q) and convert to fq_fe             */
/* ------------------------------------------------------------------ */

/*
 * After all divstep iterations, f = +/-1 and g = 0. d contains the modular
 * inverse, negated if f = -1. This routine normalizes d to [0, q) and
 * repacks into the radix-2^25.5 fq_fe format via the canonical 32-byte
 * intermediary so the existing (well-tested) frombytes path performs the
 * final repacking.
 */
static inline void fq_divsteps_normalize_30(fq_fe out, fq_signed30 *d, const fq_signed30 *f)
{
    /* Determine sign of f. After convergence f = +/-1, so the high limb's
     * sign bit tells us which. */
    int32_t f_neg = f->v[8] >> 31; /* -1 if f < 0, 0 if f > 0 */

    /* Conditionally negate d if f < 0 */
    for (int i = 0; i < 9; i++)
        d->v[i] = (d->v[i] ^ f_neg) - f_neg;

    /* Carry-normalize d so all limbs are in [0, 2^30) (limb 8 may still be
     * negative or > 2^30 after this; subsequent passes correct it). */
    int32_t carry = 0;
    for (int i = 0; i < 8; i++)
    {
        d->v[i] += carry;
        carry = d->v[i] >> 30;
        d->v[i] -= ranshaw_shl_i32(carry, 30);
    }
    d->v[8] += carry;

    /* If d < 0, add q to make it positive */
    int32_t neg_mask = d->v[8] >> 31;
    carry = 0;
    for (int i = 0; i < 9; i++)
    {
        d->v[i] += FQ_MODULUS_S30.v[i] & neg_mask;
        carry = d->v[i] >> 30;
        if (i < 8)
        {
            d->v[i] -= ranshaw_shl_i32(carry, 30);
            d->v[i + 1] += carry;
        }
    }

    /* If d >= q, subtract q (at most once needed) */
    int32_t tmp[9];
    int32_t borrow = 0;
    for (int i = 0; i < 9; i++)
    {
        tmp[i] = d->v[i] - FQ_MODULUS_S30.v[i] - borrow;
        borrow = (tmp[i] >> 31) & 1;
        if (i < 8)
            tmp[i] = (int32_t)((uint32_t)tmp[i] & FQ_M30);
    }
    /* If no borrow (tmp[8] >= 0), d >= q, use tmp; else keep d */
    int32_t ge_mask = ~(tmp[8] >> 31); /* -1 if d >= q, 0 otherwise */
    for (int i = 0; i < 9; i++)
        d->v[i] = (d->v[i] & ~ge_mask) | (tmp[i] & ge_mask);

    /* Pack 9 x 30-bit limbs into 32-byte LE canonical form, then deserialize
     * via the existing frombytes path to obtain a valid fq_fe (radix-2^25.5,
     * 10 limbs). The bytes intermediary keeps the bit-stitching logic
     * symmetric with fq_fe_to_signed30 below. */
    uint32_t w[9];
    for (int i = 0; i < 9; i++)
        w[i] = (uint32_t)d->v[i];

    unsigned char b[32];
    /* Serialize bit-by-bit windows. Each output byte b[k] holds bits
     * [8k .. 8k+7] of the integer; some windows straddle two limbs. */
    auto pack_byte = [&](int byte_idx) -> unsigned char
    {
        const int bit_lo = byte_idx * 8;
        const int limb_lo = bit_lo / 30;
        const int shift_lo = bit_lo - limb_lo * 30; /* 0..29 */
        uint32_t lo = w[limb_lo] >> shift_lo;
        if (shift_lo > 22 && limb_lo + 1 < 9)
            lo |= w[limb_lo + 1] << (30 - shift_lo);
        return (unsigned char)(lo & 0xFFu);
    };
    for (int k = 0; k < 32; k++)
        b[k] = pack_byte(k);

    fq_frombytes_portable(out, b);
}

/* ------------------------------------------------------------------ */
/* Conversion: fq_fe (radix-2^25.5) -> signed30                       */
/* ------------------------------------------------------------------ */

/*
 * Convert a (possibly non-canonical) fq_fe into a canonical signed30
 * representation. Goes via the canonical 32-byte intermediary so the
 * existing fq_tobytes_portable does the canonicalization (carry chain +
 * gamma-fold + final >= q check).
 */
static inline void fq_fe_to_signed30(fq_signed30 *s, const fq_fe z)
{
    unsigned char b[32];
    fq_tobytes_portable(b, z);

    /* Stitch 32 bytes (256 bits LE) into 9 x 30-bit limbs. */
    auto load32 = [&](int off) -> uint32_t
    {
        return (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8) | ((uint32_t)b[off + 2] << 16)
               | ((uint32_t)b[off + 3] << 24);
    };
    uint32_t w0 = load32(0); /* bits 0..31 */
    uint32_t w1 = load32(4); /* bits 32..63 */
    uint32_t w2 = load32(8); /* bits 64..95 */
    uint32_t w3 = load32(12); /* bits 96..127 */
    uint32_t w4 = load32(16); /* bits 128..159 */
    uint32_t w5 = load32(20); /* bits 160..191 */
    uint32_t w6 = load32(24); /* bits 192..223 */
    uint32_t w7 = load32(28); /* bits 224..255 */

    s->v[0] = (int32_t)(w0 & FQ_M30);
    s->v[1] = (int32_t)(((w0 >> 30) | (w1 << 2)) & FQ_M30);
    s->v[2] = (int32_t)(((w1 >> 28) | (w2 << 4)) & FQ_M30);
    s->v[3] = (int32_t)(((w2 >> 26) | (w3 << 6)) & FQ_M30);
    s->v[4] = (int32_t)(((w3 >> 24) | (w4 << 8)) & FQ_M30);
    s->v[5] = (int32_t)(((w4 >> 22) | (w5 << 10)) & FQ_M30);
    s->v[6] = (int32_t)(((w5 >> 20) | (w6 << 12)) & FQ_M30);
    s->v[7] = (int32_t)(((w6 >> 18) | (w7 << 14)) & FQ_M30);
    s->v[8] = (int32_t)(w7 >> 16); /* 15 bits since q < 2^255 */
}

#endif // RANSHAW_PORTABLE_FQ_DIVSTEPS_H
