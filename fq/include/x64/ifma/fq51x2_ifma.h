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
 * @file fq51x2_ifma.h
 * @brief 2-way lane-packed F_q primitives using AVX-512 IFMA (vpmadd52{lu,hu}q).
 *
 * Each field element is stored as __m512i[5], one vector per radix-2^51 limb.
 * Lanes 0 and 1 hold the limb of element A and element B respectively.
 * Lanes 2..7 are zero-padded at pack time and MUST remain zero through every op.
 * Debug builds assert this invariant via fq51x2_assert_zero_pad.
 *
 * IFMA encoding of radix-2^51:
 *   vpmadd52lo_epu64(c, a, b)[lane] = (c + ((a & mask52) * (b & mask52)) & mask52) mod 2^64
 *   vpmadd52hi_epu64(c, a, b)[lane] = (c + ((a & mask52) * (b & mask52)) >> 52) mod 2^64
 * For 51-bit inputs the product fits in 102 bits. In radix-2^51 the product at
 * column k contributes: lo(a*b) * 2^(51k) + hi(a*b) * 2^(51k+52)
 *                     = lo(a*b) * 2^(51k) + (2 * hi(a*b)) * 2^(51(k+1))
 * So column k accumulates lo from (i+j)==k and 2*hi from (i+j)==k-1.
 */

#ifndef RANSHAW_X64_IFMA_FQ51X2_IFMA_H
#define RANSHAW_X64_IFMA_FQ51X2_IFMA_H

#include "fq.h" // fq_fe
#include "x64/fq51.h" // FQ51_MASK, GAMMA_51, Q_51, BIAS_Q_51

#include <immintrin.h>
#include <stdint.h>

#if defined(__AVX512F__) && defined(__AVX512IFMA__) && !defined(_MSC_VER)

typedef __m512i fq51x2_t[5];

/* ==========================================================================
 * Zero-pad invariant (debug only)
 * ========================================================================== */
#ifndef NDEBUG
#include <cassert>
static inline void fq51x2_assert_zero_pad_vec(__m512i v)
{
    alignas(64) uint64_t buf[8];
    _mm512_store_si512((__m512i *)buf, v);
    assert(buf[2] == 0 && buf[3] == 0 && buf[4] == 0 && buf[5] == 0 && buf[6] == 0 && buf[7] == 0);
}
static inline void fq51x2_assert_zero_pad(const fq51x2_t a)
{
    for (int i = 0; i < 5; i++)
        fq51x2_assert_zero_pad_vec(a[i]);
}
#define FQ51X2_ASSERT_ZERO_PAD(a) fq51x2_assert_zero_pad(a)
#else
#define FQ51X2_ASSERT_ZERO_PAD(a) ((void)0)
#endif

/* ==========================================================================
 * Pack / unpack / basic moves
 * ========================================================================== */

/* Pack two fq_fe elements (radix-2^51) into an fq51x2_t.
 * Lane 0 = a, lane 1 = b, lanes 2..7 = 0. */
static inline void fq51x2_from_pair(fq51x2_t out, const fq_fe a, const fq_fe b)
{
    uint64_t a5[5], b5[5];
    fq_fe_to_5x51(a5, a);
    fq_fe_to_5x51(b5, b);
    for (int i = 0; i < 5; i++)
    {
        out[i] = _mm512_set_epi64(0, 0, 0, 0, 0, 0, (long long)b5[i], (long long)a5[i]);
    }
}

/* Unpack to two fq_fe. fq51x2_mul / _sq produce 5x51 outputs with limbs
 * < 2^52; fq_fe_from_5x51 normalizes+packs to 4x64 on the native build, or
 * copies the lazy 5x51 limbs straight into the radix-2^51 fq_fe otherwise. */
static inline void fq51x2_to_pair(fq_fe a, fq_fe b, const fq51x2_t in)
{
    alignas(64) uint64_t buf[8];
    uint64_t a5[5], b5[5];
    for (int i = 0; i < 5; i++)
    {
        _mm512_store_si512((__m512i *)buf, in[i]);
        a5[i] = buf[0];
        b5[i] = buf[1];
    }
    fq_fe_from_5x51(a, a5);
    fq_fe_from_5x51(b, b5);
}

/* Broadcast a single fq_fe into both lanes (lane 0 = lane 1 = a). */
static inline void fq51x2_broadcast(fq51x2_t out, const fq_fe a)
{
    uint64_t a5[5];
    fq_fe_to_5x51(a5, a);
    for (int i = 0; i < 5; i++)
    {
        out[i] = _mm512_set_epi64(0, 0, 0, 0, 0, 0, (long long)a5[i], (long long)a5[i]);
    }
}

static inline void fq51x2_copy(fq51x2_t r, const fq51x2_t a)
{
    r[0] = a[0];
    r[1] = a[1];
    r[2] = a[2];
    r[3] = a[3];
    r[4] = a[4];
}

static inline void fq51x2_zero(fq51x2_t r)
{
    const __m512i z = _mm512_setzero_si512();
    r[0] = z;
    r[1] = z;
    r[2] = z;
    r[3] = z;
    r[4] = z;
}

/* ==========================================================================
 * Lazy add / sub / neg
 * ========================================================================== */

/* Lane-wise add, no carry propagation (lazy). Output limbs bounded by
 * (a-limb + b-limb). Mirror of scalar fq_add. */
static inline void fq51x2_add(fq51x2_t r, const fq51x2_t a, const fq51x2_t b)
{
    r[0] = _mm512_add_epi64(a[0], b[0]);
    r[1] = _mm512_add_epi64(a[1], b[1]);
    r[2] = _mm512_add_epi64(a[2], b[2]);
    r[3] = _mm512_add_epi64(a[3], b[3]);
    r[4] = _mm512_add_epi64(a[4], b[4]);
}

/* Subtraction with 128q bias + carry chain + 1-limb gamma fold.
 * Mirrors scalar fq_sub bit-for-bit per lane. Output limbs < 2^52. */
static inline void fq51x2_sub(fq51x2_t r, const fq51x2_t a, const fq51x2_t b)
{
    const __m512i mask51 = _mm512_set1_epi64((long long)FQ51_MASK);
    /* Bias only in lanes 0..1; lanes 2..7 stay zero to preserve the
     * zero-pad invariant. */
    const __m512i bias0 = _mm512_set_epi64(0, 0, 0, 0, 0, 0, (long long)BIAS_Q_51[0], (long long)BIAS_Q_51[0]);
    const __m512i bias1 = _mm512_set_epi64(0, 0, 0, 0, 0, 0, (long long)BIAS_Q_51[1], (long long)BIAS_Q_51[1]);
    const __m512i bias2 = _mm512_set_epi64(0, 0, 0, 0, 0, 0, (long long)BIAS_Q_51[2], (long long)BIAS_Q_51[2]);
    const __m512i bias3 = _mm512_set_epi64(0, 0, 0, 0, 0, 0, (long long)BIAS_Q_51[3], (long long)BIAS_Q_51[3]);
    const __m512i bias4 = _mm512_set_epi64(0, 0, 0, 0, 0, 0, (long long)BIAS_Q_51[4], (long long)BIAS_Q_51[4]);

    __m512i h0 = _mm512_add_epi64(a[0], bias0);
    h0 = _mm512_sub_epi64(h0, b[0]);
    __m512i c = _mm512_srli_epi64(h0, 51);
    h0 = _mm512_and_si512(h0, mask51);

    __m512i h1 = _mm512_add_epi64(a[1], bias1);
    h1 = _mm512_sub_epi64(h1, b[1]);
    h1 = _mm512_add_epi64(h1, c);
    c = _mm512_srli_epi64(h1, 51);
    h1 = _mm512_and_si512(h1, mask51);

    __m512i h2 = _mm512_add_epi64(a[2], bias2);
    h2 = _mm512_sub_epi64(h2, b[2]);
    h2 = _mm512_add_epi64(h2, c);
    c = _mm512_srli_epi64(h2, 51);
    h2 = _mm512_and_si512(h2, mask51);

    __m512i h3 = _mm512_add_epi64(a[3], bias3);
    h3 = _mm512_sub_epi64(h3, b[3]);
    h3 = _mm512_add_epi64(h3, c);
    c = _mm512_srli_epi64(h3, 51);
    h3 = _mm512_and_si512(h3, mask51);

    __m512i h4 = _mm512_add_epi64(a[4], bias4);
    h4 = _mm512_sub_epi64(h4, b[4]);
    h4 = _mm512_add_epi64(h4, c);
    c = _mm512_srli_epi64(h4, 51);
    h4 = _mm512_and_si512(h4, mask51);

    /* Gamma fold: top carry c * 2^255 ≡ c * gamma (mod q).
     * c comes from the bias-subtract carry chain; BIAS_Q_51[4] is 58 bits, so
     * c ≤ ~2^7 per lane. GAMMA_51[j] < 2^52, so c*GAMMA_51[j] < 2^59 fits in
     * a 64-bit lane. We avoid _mm512_mullo_epi64 (AVX512DQ) by combining
     * vpmadd52 lo/hi: lo gives bits 0..51, hi gives bits 52..103, and the
     * product reconstructs as lo + (hi << 52). */
    const __m512i zero = _mm512_setzero_si512();
    const __m512i g0 = _mm512_set1_epi64((long long)GAMMA_51[0]);
    const __m512i g1 = _mm512_set1_epi64((long long)GAMMA_51[1]);
    const __m512i g2 = _mm512_set1_epi64((long long)GAMMA_51[2]);

    __m512i lo0 = _mm512_madd52lo_epu64(zero, c, g0);
    __m512i hi0 = _mm512_madd52hi_epu64(zero, c, g0);
    h0 = _mm512_add_epi64(h0, _mm512_add_epi64(lo0, _mm512_slli_epi64(hi0, 52)));
    __m512i lo1 = _mm512_madd52lo_epu64(zero, c, g1);
    __m512i hi1 = _mm512_madd52hi_epu64(zero, c, g1);
    h1 = _mm512_add_epi64(h1, _mm512_add_epi64(lo1, _mm512_slli_epi64(hi1, 52)));
    __m512i lo2 = _mm512_madd52lo_epu64(zero, c, g2);
    __m512i hi2 = _mm512_madd52hi_epu64(zero, c, g2);
    h2 = _mm512_add_epi64(h2, _mm512_add_epi64(lo2, _mm512_slli_epi64(hi2, 52)));

    /* Re-carry through limb 3→4 (matching scalar: GAMMA_51_LIMBS+1 = 4 passes). */
    c = _mm512_srli_epi64(h0, 51);
    h0 = _mm512_and_si512(h0, mask51);
    h1 = _mm512_add_epi64(h1, c);
    c = _mm512_srli_epi64(h1, 51);
    h1 = _mm512_and_si512(h1, mask51);
    h2 = _mm512_add_epi64(h2, c);
    c = _mm512_srli_epi64(h2, 51);
    h2 = _mm512_and_si512(h2, mask51);
    h3 = _mm512_add_epi64(h3, c);
    c = _mm512_srli_epi64(h3, 51);
    h3 = _mm512_and_si512(h3, mask51);
    h4 = _mm512_add_epi64(h4, c);

    r[0] = h0;
    r[1] = h1;
    r[2] = h2;
    r[3] = h3;
    r[4] = h4;
}

/* Carry-propagate an fq51x2 value with potentially non-canonical limbs
 * (from lazy add or scale_small) down to < 2^52 per limb so it can be used
 * as an IFMA multiplication operand. Mirrors the carry+gamma-fold+re-carry
 * pipeline from fq_sub without the bias subtract.
 *
 * Precondition: input limbs ≤ ~2^58 (plenty of headroom for k*a with k ≤ 8
 * starting from < 2^52 limbs). Output: lanes 0..1 canonical (< 2^52), lanes
 * 2..7 remain zero (input zero-pad preserved). */
static inline void fq51x2_canonicalize(fq51x2_t r, const fq51x2_t a)
{
    const __m512i mask51 = _mm512_set1_epi64((long long)FQ51_MASK);

    __m512i h0 = a[0], h1 = a[1], h2 = a[2], h3 = a[3], h4 = a[4];
    __m512i c;

    c = _mm512_srli_epi64(h0, 51);
    h0 = _mm512_and_si512(h0, mask51);
    h1 = _mm512_add_epi64(h1, c);
    c = _mm512_srli_epi64(h1, 51);
    h1 = _mm512_and_si512(h1, mask51);
    h2 = _mm512_add_epi64(h2, c);
    c = _mm512_srli_epi64(h2, 51);
    h2 = _mm512_and_si512(h2, mask51);
    h3 = _mm512_add_epi64(h3, c);
    c = _mm512_srli_epi64(h3, 51);
    h3 = _mm512_and_si512(h3, mask51);
    h4 = _mm512_add_epi64(h4, c);
    c = _mm512_srli_epi64(h4, 51);
    h4 = _mm512_and_si512(h4, mask51);

    /* Top-carry gamma fold: c * 2^255 ≡ c * gamma (mod q). */
    const __m512i zero = _mm512_setzero_si512();
    const __m512i g0 = _mm512_set1_epi64((long long)GAMMA_51[0]);
    const __m512i g1 = _mm512_set1_epi64((long long)GAMMA_51[1]);
    const __m512i g2 = _mm512_set1_epi64((long long)GAMMA_51[2]);
    __m512i lo0 = _mm512_madd52lo_epu64(zero, c, g0);
    __m512i hi0 = _mm512_madd52hi_epu64(zero, c, g0);
    h0 = _mm512_add_epi64(h0, _mm512_add_epi64(lo0, _mm512_slli_epi64(hi0, 52)));
    __m512i lo1 = _mm512_madd52lo_epu64(zero, c, g1);
    __m512i hi1 = _mm512_madd52hi_epu64(zero, c, g1);
    h1 = _mm512_add_epi64(h1, _mm512_add_epi64(lo1, _mm512_slli_epi64(hi1, 52)));
    __m512i lo2 = _mm512_madd52lo_epu64(zero, c, g2);
    __m512i hi2 = _mm512_madd52hi_epu64(zero, c, g2);
    h2 = _mm512_add_epi64(h2, _mm512_add_epi64(lo2, _mm512_slli_epi64(hi2, 52)));

    /* Re-carry through limb 3→4. */
    c = _mm512_srli_epi64(h0, 51);
    h0 = _mm512_and_si512(h0, mask51);
    h1 = _mm512_add_epi64(h1, c);
    c = _mm512_srli_epi64(h1, 51);
    h1 = _mm512_and_si512(h1, mask51);
    h2 = _mm512_add_epi64(h2, c);
    c = _mm512_srli_epi64(h2, 51);
    h2 = _mm512_and_si512(h2, mask51);
    h3 = _mm512_add_epi64(h3, c);
    c = _mm512_srli_epi64(h3, 51);
    h3 = _mm512_and_si512(h3, mask51);
    h4 = _mm512_add_epi64(h4, c);

    r[0] = h0;
    r[1] = h1;
    r[2] = h2;
    r[3] = h3;
    r[4] = h4;
}

static inline void fq51x2_neg(fq51x2_t r, const fq51x2_t a)
{
    fq51x2_t zero;
    fq51x2_zero(zero);
    fq51x2_sub(r, zero, a);
}

/* ==========================================================================
 * Constant-time conditional move
 * ========================================================================== */

/* r <- b ? s : r, where b ∈ {0,1}. Applied to both lanes identically. */
static inline void fq51x2_cmov(fq51x2_t r, const fq51x2_t s, unsigned int b)
{
    const uint64_t m = (uint64_t)0 - (uint64_t)(b & 1u);
    const __m512i mask = _mm512_set1_epi64((long long)m);
    for (int i = 0; i < 5; i++)
    {
        __m512i diff = _mm512_xor_si512(r[i], s[i]);
        diff = _mm512_and_si512(diff, mask);
        r[i] = _mm512_xor_si512(r[i], diff);
    }
}

/* r = (lane0: a_lane0, lane1: b_lane0). Lanes 2..7 zero. Avoids memory
 * round-trip when we want to pair two fq51x2_t values (both with payloads
 * in lane 0) into a single fq51x2_t for a pair mul/sq. */
static inline void fq51x2_mix_lane1(fq51x2_t r, const fq51x2_t a, const fq51x2_t b)
{
    for (int i = 0; i < 5; i++)
        r[i] = _mm512_mask_blend_epi64(0x02, a[i], b[i]);
    FQ51X2_ASSERT_ZERO_PAD(r);
}

/* Broadcast lane 0 of a into lane 1 as well (so both lanes equal). Lanes
 * 2..7 stay zero. */
static inline void fq51x2_duplicate_lane0(fq51x2_t r, const fq51x2_t a)
{
    /* Permute index: lanes 0,1 pull from source lane 0; lanes 2..7 from
     * source lanes 2..7 (which are zero). */
    const __m512i idx = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 0, 0);
    for (int i = 0; i < 5; i++)
        r[i] = _mm512_permutexvar_epi64(idx, a[i]);
    FQ51X2_ASSERT_ZERO_PAD(r);
}

/* Broadcast lane 1 of a into lane 0 (so both lanes equal a's lane 1).
 * Used after pair sq/mul when we want to "select" the lane-1 result as an
 * operand with the broadcast invariant. */
static inline void fq51x2_duplicate_lane1(fq51x2_t r, const fq51x2_t a)
{
    const __m512i idx = _mm512_set_epi64(7, 6, 5, 4, 3, 2, 1, 1);
    for (int i = 0; i < 5; i++)
        r[i] = _mm512_permutexvar_epi64(idx, a[i]);
    FQ51X2_ASSERT_ZERO_PAD(r);
}

/* Subtract with 256q bias (≈ 2× the bias of fq51x2_sub), so input limbs up
 * to ~2^54 are absorbed without wrap. Output limbs < 2^52. Correctness
 * depends on BIAS_Q_51 being shiftable by 1 without overflowing 2^63 (it is:
 * BIAS_Q_51[4] is 58-bit so 2× → 59-bit). */
static inline void fq51x2_sub_wide(fq51x2_t r, const fq51x2_t a, const fq51x2_t b)
{
    const __m512i mask51 = _mm512_set1_epi64((long long)FQ51_MASK);
    const __m512i bias0 =
        _mm512_set_epi64(0, 0, 0, 0, 0, 0, (long long)(BIAS_Q_51[0] << 1), (long long)(BIAS_Q_51[0] << 1));
    const __m512i bias1 =
        _mm512_set_epi64(0, 0, 0, 0, 0, 0, (long long)(BIAS_Q_51[1] << 1), (long long)(BIAS_Q_51[1] << 1));
    const __m512i bias2 =
        _mm512_set_epi64(0, 0, 0, 0, 0, 0, (long long)(BIAS_Q_51[2] << 1), (long long)(BIAS_Q_51[2] << 1));
    const __m512i bias3 =
        _mm512_set_epi64(0, 0, 0, 0, 0, 0, (long long)(BIAS_Q_51[3] << 1), (long long)(BIAS_Q_51[3] << 1));
    const __m512i bias4 =
        _mm512_set_epi64(0, 0, 0, 0, 0, 0, (long long)(BIAS_Q_51[4] << 1), (long long)(BIAS_Q_51[4] << 1));

    __m512i h0 = _mm512_add_epi64(a[0], bias0);
    h0 = _mm512_sub_epi64(h0, b[0]);
    __m512i c = _mm512_srli_epi64(h0, 51);
    h0 = _mm512_and_si512(h0, mask51);

    __m512i h1 = _mm512_add_epi64(a[1], bias1);
    h1 = _mm512_sub_epi64(h1, b[1]);
    h1 = _mm512_add_epi64(h1, c);
    c = _mm512_srli_epi64(h1, 51);
    h1 = _mm512_and_si512(h1, mask51);

    __m512i h2 = _mm512_add_epi64(a[2], bias2);
    h2 = _mm512_sub_epi64(h2, b[2]);
    h2 = _mm512_add_epi64(h2, c);
    c = _mm512_srli_epi64(h2, 51);
    h2 = _mm512_and_si512(h2, mask51);

    __m512i h3 = _mm512_add_epi64(a[3], bias3);
    h3 = _mm512_sub_epi64(h3, b[3]);
    h3 = _mm512_add_epi64(h3, c);
    c = _mm512_srli_epi64(h3, 51);
    h3 = _mm512_and_si512(h3, mask51);

    __m512i h4 = _mm512_add_epi64(a[4], bias4);
    h4 = _mm512_sub_epi64(h4, b[4]);
    h4 = _mm512_add_epi64(h4, c);
    c = _mm512_srli_epi64(h4, 51);
    h4 = _mm512_and_si512(h4, mask51);

    const __m512i zero = _mm512_setzero_si512();
    const __m512i g0 = _mm512_set1_epi64((long long)GAMMA_51[0]);
    const __m512i g1 = _mm512_set1_epi64((long long)GAMMA_51[1]);
    const __m512i g2 = _mm512_set1_epi64((long long)GAMMA_51[2]);

    __m512i lo0 = _mm512_madd52lo_epu64(zero, c, g0);
    __m512i hi0 = _mm512_madd52hi_epu64(zero, c, g0);
    h0 = _mm512_add_epi64(h0, _mm512_add_epi64(lo0, _mm512_slli_epi64(hi0, 52)));
    __m512i lo1 = _mm512_madd52lo_epu64(zero, c, g1);
    __m512i hi1 = _mm512_madd52hi_epu64(zero, c, g1);
    h1 = _mm512_add_epi64(h1, _mm512_add_epi64(lo1, _mm512_slli_epi64(hi1, 52)));
    __m512i lo2 = _mm512_madd52lo_epu64(zero, c, g2);
    __m512i hi2 = _mm512_madd52hi_epu64(zero, c, g2);
    h2 = _mm512_add_epi64(h2, _mm512_add_epi64(lo2, _mm512_slli_epi64(hi2, 52)));

    c = _mm512_srli_epi64(h0, 51);
    h0 = _mm512_and_si512(h0, mask51);
    h1 = _mm512_add_epi64(h1, c);
    c = _mm512_srli_epi64(h1, 51);
    h1 = _mm512_and_si512(h1, mask51);
    h2 = _mm512_add_epi64(h2, c);
    c = _mm512_srli_epi64(h2, 51);
    h2 = _mm512_and_si512(h2, mask51);
    h3 = _mm512_add_epi64(h3, c);
    c = _mm512_srli_epi64(h3, 51);
    h3 = _mm512_and_si512(h3, mask51);
    h4 = _mm512_add_epi64(h4, c);

    r[0] = h0;
    r[1] = h1;
    r[2] = h2;
    r[3] = h3;
    r[4] = h4;
}

/* ==========================================================================
 * Small-integer scaling (chained lazy adds)
 * ========================================================================== */

/* r = k * a, via chained lane-wise adds. Output limbs bounded by k * max(a-limb).
 * For canonical input (limbs < 2^51), k ≤ 8 yields limbs < 2^54 which the mul
 * path's column accumulator comfortably absorbs. */
static inline void fq51x2_scale_small(fq51x2_t r, const fq51x2_t a, unsigned int k)
{
    /* Unrolled for the supported set {2,3,4,8}; default path works generally. */
    if (k == 2)
    {
        fq51x2_add(r, a, a);
    }
    else if (k == 3)
    {
        fq51x2_t d;
        fq51x2_add(d, a, a);
        fq51x2_add(r, d, a);
    }
    else if (k == 4)
    {
        fq51x2_t d;
        fq51x2_add(d, a, a);
        fq51x2_add(r, d, d);
    }
    else if (k == 8)
    {
        fq51x2_t d, q;
        fq51x2_add(d, a, a);
        fq51x2_add(q, d, d);
        fq51x2_add(r, q, q);
    }
    else
    {
        /* Generic: build up with repeated add. */
        fq51x2_copy(r, a);
        for (unsigned int i = 1; i < k; i++)
            fq51x2_add(r, r, a);
    }
}

/* ==========================================================================
 * Reduction tail: t[0..9] (pre-merge lo/hi) → canonical r[0..4]
 * ========================================================================== */

/* Shared tail for mul and sq. Takes column accumulators tlo[0..9] and
 * thi[0..8] (where column k's value = tlo[k] + 2*thi[k-1]) and produces
 * the canonical 5-limb result. All of the carry/fold pipeline lives here. */
static inline void fq51x2_reduce_tail(fq51x2_t r, const __m512i tlo[10], const __m512i thi[9])
{
    const __m512i zero = _mm512_setzero_si512();
    const __m512i mask51 = _mm512_set1_epi64((long long)FQ51_MASK);

    /* Merge: t[0] = tlo[0]; t[k] = tlo[k] + 2*thi[k-1] for k=1..9. */
    __m512i t[10];
    t[0] = tlo[0];
    for (int k = 1; k <= 9; k++)
    {
        __m512i hi_shifted = _mm512_slli_epi64(thi[k - 1], 1);
        t[k] = _mm512_add_epi64(tlo[k], hi_shifted);
    }

    /* Carry chain t[0..9] at bit 51. */
    __m512i r0, r1, r2, r3, r4, r5, r6, r7, r8, r9;
    __m512i c;

    c = _mm512_srli_epi64(t[0], 51);
    r0 = _mm512_and_si512(t[0], mask51);
    t[1] = _mm512_add_epi64(t[1], c);
    c = _mm512_srli_epi64(t[1], 51);
    r1 = _mm512_and_si512(t[1], mask51);
    t[2] = _mm512_add_epi64(t[2], c);
    c = _mm512_srli_epi64(t[2], 51);
    r2 = _mm512_and_si512(t[2], mask51);
    t[3] = _mm512_add_epi64(t[3], c);
    c = _mm512_srli_epi64(t[3], 51);
    r3 = _mm512_and_si512(t[3], mask51);
    t[4] = _mm512_add_epi64(t[4], c);
    c = _mm512_srli_epi64(t[4], 51);
    r4 = _mm512_and_si512(t[4], mask51);
    t[5] = _mm512_add_epi64(t[5], c);
    c = _mm512_srli_epi64(t[5], 51);
    r5 = _mm512_and_si512(t[5], mask51);
    t[6] = _mm512_add_epi64(t[6], c);
    c = _mm512_srli_epi64(t[6], 51);
    r6 = _mm512_and_si512(t[6], mask51);
    t[7] = _mm512_add_epi64(t[7], c);
    c = _mm512_srli_epi64(t[7], 51);
    r7 = _mm512_and_si512(t[7], mask51);
    t[8] = _mm512_add_epi64(t[8], c);
    c = _mm512_srli_epi64(t[8], 51);
    r8 = _mm512_and_si512(t[8], mask51);
    t[9] = _mm512_add_epi64(t[9], c);
    __m512i c9 = _mm512_srli_epi64(t[9], 51);
    r9 = _mm512_and_si512(t[9], mask51);

    /* First Crandall fold: [r5..r9, c9] × GAMMA_51[0..2] → p[0..7]. */
    __m512i plo[8], phi[8];
    for (int i = 0; i < 8; i++)
    {
        plo[i] = zero;
        phi[i] = zero;
    }

    __m512i overflow[6] = {r5, r6, r7, r8, r9, c9};
    const uint64_t gamma_scalars[3] = {GAMMA_51[0], GAMMA_51[1], GAMMA_51[2]};
    for (int k = 0; k < 6; k++)
    {
        for (int j = 0; j < 3; j++)
        {
            __m512i gj = _mm512_set1_epi64((long long)gamma_scalars[j]);
            plo[k + j] = _mm512_madd52lo_epu64(plo[k + j], overflow[k], gj);
            phi[k + j] = _mm512_madd52hi_epu64(phi[k + j], overflow[k], gj);
        }
    }

    __m512i p[8];
    p[0] = plo[0];
    for (int k = 1; k <= 7; k++)
    {
        __m512i hi_shifted = _mm512_slli_epi64(phi[k - 1], 1);
        p[k] = _mm512_add_epi64(plo[k], hi_shifted);
    }

    p[0] = _mm512_add_epi64(p[0], r0);
    p[1] = _mm512_add_epi64(p[1], r1);
    p[2] = _mm512_add_epi64(p[2], r2);
    p[3] = _mm512_add_epi64(p[3], r3);
    p[4] = _mm512_add_epi64(p[4], r4);

    __m512i q0, q1, q2, q3, q4, q5, q6, q7;
    c = _mm512_srli_epi64(p[0], 51);
    q0 = _mm512_and_si512(p[0], mask51);
    p[1] = _mm512_add_epi64(p[1], c);
    c = _mm512_srli_epi64(p[1], 51);
    q1 = _mm512_and_si512(p[1], mask51);
    p[2] = _mm512_add_epi64(p[2], c);
    c = _mm512_srli_epi64(p[2], 51);
    q2 = _mm512_and_si512(p[2], mask51);
    p[3] = _mm512_add_epi64(p[3], c);
    c = _mm512_srli_epi64(p[3], 51);
    q3 = _mm512_and_si512(p[3], mask51);
    p[4] = _mm512_add_epi64(p[4], c);
    c = _mm512_srli_epi64(p[4], 51);
    q4 = _mm512_and_si512(p[4], mask51);
    p[5] = _mm512_add_epi64(p[5], c);
    c = _mm512_srli_epi64(p[5], 51);
    q5 = _mm512_and_si512(p[5], mask51);
    p[6] = _mm512_add_epi64(p[6], c);
    c = _mm512_srli_epi64(p[6], 51);
    q6 = _mm512_and_si512(p[6], mask51);
    p[7] = _mm512_add_epi64(p[7], c);
    q7 = p[7];

    /* Second Crandall fold: [q5, q6, q7] × GAMMA_51 → s[0..4]. */
    __m512i s_lo[5], s_hi[5];
    for (int i = 0; i < 5; i++)
    {
        s_lo[i] = zero;
        s_hi[i] = zero;
    }

    __m512i overflow2[3] = {q5, q6, q7};
    for (int k = 0; k < 3; k++)
    {
        for (int j = 0; j < 3; j++)
        {
            __m512i gj = _mm512_set1_epi64((long long)gamma_scalars[j]);
            s_lo[k + j] = _mm512_madd52lo_epu64(s_lo[k + j], overflow2[k], gj);
            s_hi[k + j] = _mm512_madd52hi_epu64(s_hi[k + j], overflow2[k], gj);
        }
    }

    __m512i s[5];
    s[0] = s_lo[0];
    for (int k = 1; k <= 4; k++)
    {
        __m512i hi_shifted = _mm512_slli_epi64(s_hi[k - 1], 1);
        s[k] = _mm512_add_epi64(s_lo[k], hi_shifted);
    }

    s[0] = _mm512_add_epi64(s[0], q0);
    s[1] = _mm512_add_epi64(s[1], q1);
    s[2] = _mm512_add_epi64(s[2], q2);
    s[3] = _mm512_add_epi64(s[3], q3);
    s[4] = _mm512_add_epi64(s[4], q4);

    /* Final carry + tiny third fold. */
    __m512i h0, h1, h2, h3, h4;
    c = _mm512_srli_epi64(s[0], 51);
    h0 = _mm512_and_si512(s[0], mask51);
    s[1] = _mm512_add_epi64(s[1], c);
    c = _mm512_srli_epi64(s[1], 51);
    h1 = _mm512_and_si512(s[1], mask51);
    s[2] = _mm512_add_epi64(s[2], c);
    c = _mm512_srli_epi64(s[2], 51);
    h2 = _mm512_and_si512(s[2], mask51);
    s[3] = _mm512_add_epi64(s[3], c);
    c = _mm512_srli_epi64(s[3], 51);
    h3 = _mm512_and_si512(s[3], mask51);
    s[4] = _mm512_add_epi64(s[4], c);
    __m512i c_top = _mm512_srli_epi64(s[4], 51);
    h4 = _mm512_and_si512(s[4], mask51);

    __m512i g0v = _mm512_set1_epi64((long long)GAMMA_51[0]);
    __m512i g1v = _mm512_set1_epi64((long long)GAMMA_51[1]);
    __m512i g2v = _mm512_set1_epi64((long long)GAMMA_51[2]);
    h0 = _mm512_madd52lo_epu64(h0, c_top, g0v);
    __m512i htmp_hi0 = _mm512_madd52hi_epu64(zero, c_top, g0v);
    h1 = _mm512_add_epi64(h1, _mm512_slli_epi64(htmp_hi0, 1));
    h1 = _mm512_madd52lo_epu64(h1, c_top, g1v);
    __m512i htmp_hi1 = _mm512_madd52hi_epu64(zero, c_top, g1v);
    h2 = _mm512_add_epi64(h2, _mm512_slli_epi64(htmp_hi1, 1));
    h2 = _mm512_madd52lo_epu64(h2, c_top, g2v);
    __m512i htmp_hi2 = _mm512_madd52hi_epu64(zero, c_top, g2v);
    h3 = _mm512_add_epi64(h3, _mm512_slli_epi64(htmp_hi2, 1));

    c = _mm512_srli_epi64(h0, 51);
    h0 = _mm512_and_si512(h0, mask51);
    h1 = _mm512_add_epi64(h1, c);
    c = _mm512_srli_epi64(h1, 51);
    h1 = _mm512_and_si512(h1, mask51);
    h2 = _mm512_add_epi64(h2, c);
    c = _mm512_srli_epi64(h2, 51);
    h2 = _mm512_and_si512(h2, mask51);
    h3 = _mm512_add_epi64(h3, c);
    c = _mm512_srli_epi64(h3, 51);
    h3 = _mm512_and_si512(h3, mask51);
    h4 = _mm512_add_epi64(h4, c);

    r[0] = h0;
    r[1] = h1;
    r[2] = h2;
    r[3] = h3;
    r[4] = h4;
}

/* ==========================================================================
 * Multiplication: 5×5 schoolbook + shared reduction tail
 * ========================================================================== */

static inline void fq51x2_mul(fq51x2_t r, const fq51x2_t a, const fq51x2_t b)
{
    const __m512i zero = _mm512_setzero_si512();

    __m512i tlo[10], thi[9];
    for (int i = 0; i < 10; i++)
        tlo[i] = zero;
    for (int i = 0; i < 9; i++)
        thi[i] = zero;

    for (int i = 0; i < 5; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            int k = i + j;
            tlo[k] = _mm512_madd52lo_epu64(tlo[k], a[i], b[j]);
            thi[k] = _mm512_madd52hi_epu64(thi[k], a[i], b[j]);
        }
    }

    fq51x2_reduce_tail(r, tlo, thi);
    FQ51X2_ASSERT_ZERO_PAD(r);
}

/* ==========================================================================
 * Squaring: hand-tuned symmetric schoolbook (15 products vs 25 in mul)
 *
 * For i == j: t[2i] += a[i] * a[i]      (5 diagonal products)
 * For i <  j: t[i+j] += 2*a[i] * a[j]   (10 off-diagonal products)
 *
 * Precondition: input limbs a[i] < 2^51, so a_dbl[i] = 2*a[i] < 2^52 fits the
 * IFMA operand width. Violating this breaks correctness silently.
 * ========================================================================== */

static inline void fq51x2_sq(fq51x2_t r, const fq51x2_t a)
{
    const __m512i zero = _mm512_setzero_si512();

    /* Double-limb vector for off-diagonal products. */
    __m512i a_dbl[5];
    for (int i = 0; i < 5; i++)
        a_dbl[i] = _mm512_slli_epi64(a[i], 1);

    __m512i tlo[10], thi[9];
    for (int i = 0; i < 10; i++)
        tlo[i] = zero;
    for (int i = 0; i < 9; i++)
        thi[i] = zero;

    /* Diagonal: t[2i] += a[i] * a[i] */
    for (int i = 0; i < 5; i++)
    {
        int k = 2 * i;
        tlo[k] = _mm512_madd52lo_epu64(tlo[k], a[i], a[i]);
        thi[k] = _mm512_madd52hi_epu64(thi[k], a[i], a[i]);
    }

    /* Off-diagonal: t[i+j] += a_dbl[i] * a[j], i < j */
    for (int i = 0; i < 5; i++)
    {
        for (int j = i + 1; j < 5; j++)
        {
            int k = i + j;
            tlo[k] = _mm512_madd52lo_epu64(tlo[k], a_dbl[i], a[j]);
            thi[k] = _mm512_madd52hi_epu64(thi[k], a_dbl[i], a[j]);
        }
    }

    fq51x2_reduce_tail(r, tlo, thi);
    FQ51X2_ASSERT_ZERO_PAD(r);
}

#endif /* __AVX512F__ && __AVX512IFMA__ && !_MSC_VER */

#endif /* RANSHAW_X64_IFMA_FQ51X2_IFMA_H */
