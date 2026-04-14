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
 * @file fq51.h
 * @brief x64 (radix-2^51) implementation of F_q core type and operations with Crandall reduction.
 */

#ifndef RANSHAW_X64_FQ51_H
#define RANSHAW_X64_FQ51_H

#include "fq.h" /* RANSHAW_FQ_NATIVE64, fq_fe */

#include <cstdint>

static const uint64_t FQ51_MASK = (1ULL << 51) - 1;

/*
 * q = 2^255 - gamma, where gamma = 239666463199878229209741112730228557729
 * gamma is 128 bits, fitting in 3 radix-2^51 limbs.
 *
 * gamma in radix-2^51:
 *   GAMMA_51[0] = 0x7B9BA138F07A1
 *   GAMMA_51[1] = 0x638D19E0B11D2
 *   GAMMA_51[2] = 0x2D13853
 *
 * 2*gamma in radix-2^51 (129 bits, 3 limbs):
 *   TWO_GAMMA_51[0] = 0x77374271E0F42
 *   TWO_GAMMA_51[1] = 0x471A33C1623A5
 *   TWO_GAMMA_51[2] = 0x5A270A7
 */
#define GAMMA_51_LIMBS 3
static const uint64_t GAMMA_51[5] = {0x7B9BA138F07A1ULL, 0x638D19E0B11D2ULL, 0x2D13853ULL, 0, 0};

#define TWO_GAMMA_51_LIMBS 3
static const uint64_t TWO_GAMMA_51[5] = {0x77374271E0F42ULL, 0x471A33C1623A5ULL, 0x5A270A7ULL, 0, 0};

/*
 * 2*gamma in radix-2^64 (4 limbs, full 256-bit width).
 * Used by the 4×64 MULX+ADCX+ADOX multiplication path.
 * 2^256 ≡ 2*gamma (mod q), so the fold multiplies by TWO_GAMMA_64.
 */
#define TWO_GAMMA_64_LIMBS 3
static const uint64_t TWO_GAMMA_64[4] = {0x1D2F7374271E0F42ULL, 0x689C29E38D19E0B1ULL, 0x1ULL, 0};

/*
 * q and gamma in radix-2^64 (4 limbs), for the native fq_fe = uint64_t[4]
 * representation. Derived from the radix-2^51 Q_51 / GAMMA_51 below and
 * cross-checked against the canonical q = 2^255 - gamma. GAMMA_64 occupies
 * the low 2 limbs (gamma < 2^128); Q_64 is the full 4-limb modulus used by
 * the canonical-reduction conditional subtraction in fq64_tobytes.
 */
static const uint64_t Q_64[4] =
    {0x71684645EC70F85FULL, 0x4BB1EB0E39730FA7ULL, 0xFFFFFFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFFFFULL};
static const uint64_t GAMMA_64[4] = {0x8E97B9BA138F07A1ULL, 0xB44E14F1C68CF058ULL, 0x0ULL, 0x0ULL};

/*
 * Constant-time canonical reduction for the native 4x64 representation.
 * Input a < 2^256 (< 3q, the closed fq64 invariant); output the unique
 * representative in [0, q). Two masked conditional subtractions of q suffice
 * because 2^256 = 2q + 2*gamma < 3q. Branchless: subtract-with-borrow uses
 * relational ops (setcc, no jumps) and a mask-select, so no secret-dependent
 * control flow.
 */
static inline void fq64_reduce_canonical(uint64_t r[4], const uint64_t a[4])
{
    uint64_t t[4] = {a[0], a[1], a[2], a[3]};
    for (int k = 0; k < 2; k++)
    {
        uint64_t u[4];
        uint64_t borrow = 0;
        for (int i = 0; i < 4; i++)
        {
            uint64_t s = Q_64[i] + borrow;
            uint64_t b1 = (uint64_t)(s < Q_64[i]); /* overflow of q_i + borrow */
            uint64_t b2 = (uint64_t)(t[i] < s);
            u[i] = t[i] - s;
            borrow = b1 | b2;
        }
        /* borrow==0 means t >= q: take u (= t - q). borrow==1 means t < q: keep t. */
        uint64_t mask = borrow - 1; /* 0xFFFF.. if borrow==0, else 0 */
        for (int i = 0; i < 4; i++)
            t[i] = (t[i] & ~mask) | (u[i] & mask);
    }
    r[0] = t[0];
    r[1] = t[1];
    r[2] = t[2];
    r[3] = t[3];
}

/*
 * SIMD-boundary repack helpers. The AVX2 (fq10, radix-2^25.5) and IFMA
 * (fq51x2 / fq51x8, radix-2^51) backends work internally in radix-2^51, so
 * their fq_fe<->SIMD converters need to expand the native 4x64 fq_fe into
 * 5x51 limbs on entry and re-pack 5x51 -> 4x64 on exit. Pure uint64 (no
 * intrinsics): compiles on every toolchain.
 */
static inline void fq64_expand_5x51(uint64_t h[5], const uint64_t r[4])
{
    const uint64_t M = FQ51_MASK;
    h[0] = r[0] & M;
    h[1] = ((r[0] >> 51) | (r[1] << 13)) & M;
    h[2] = ((r[1] >> 38) | (r[2] << 26)) & M;
    h[3] = ((r[2] >> 25) | (r[3] << 39)) & M;
    h[4] = r[3] >> 12;
}

/* Normalize a 5x51 value (limbs may be up to ~2^53 from lazy SIMD output)
 * with two gamma folds, then pack to 4x64. Mirrors the original
 * fq51_normalize_and_pack body. */
static inline void fq51_normalize5_pack4(uint64_t r[4], const uint64_t f[5])
{
    const uint64_t M = FQ51_MASK;
    uint64_t f0, f1, f2, f3, f4, c;
    c = f[0] >> 51;
    f0 = f[0] & M;
    f1 = f[1] + c;
    c = f1 >> 51;
    f1 &= M;
    f2 = f[2] + c;
    c = f2 >> 51;
    f2 &= M;
    f3 = f[3] + c;
    c = f3 >> 51;
    f3 &= M;
    f4 = f[4] + c;
    c = f4 >> 51;
    f4 &= M;
    {
        uint64_t *fs[] = {&f0, &f1, &f2, &f3, &f4};
        for (int j = 0; j < GAMMA_51_LIMBS; j++)
            *fs[j] += c * GAMMA_51[j];
    }
    c = f0 >> 51;
    f0 &= M;
    f1 += c;
    c = f1 >> 51;
    f1 &= M;
    f2 += c;
    c = f2 >> 51;
    f2 &= M;
    f3 += c;
    c = f3 >> 51;
    f3 &= M;
    f4 += c;
    c = f4 >> 51;
    f4 &= M;
    {
        uint64_t *fs[] = {&f0, &f1, &f2, &f3, &f4};
        for (int j = 0; j < GAMMA_51_LIMBS; j++)
            *fs[j] += c * GAMMA_51[j];
    }
    c = f0 >> 51;
    f0 &= M;
    f1 += c;
    c = f1 >> 51;
    f1 &= M;
    f2 += c;
    c = f2 >> 51;
    f2 &= M;
    f3 += c;
    c = f3 >> 51;
    f3 &= M;
    f4 += c;
    r[0] = f0 | (f1 << 51);
    r[1] = (f1 >> 13) | (f2 << 38);
    r[2] = (f2 >> 26) | (f3 << 25);
    r[3] = (f3 >> 39) | (f4 << 12);
}

/*
 * fq_fe <-> 5x51 working-limb shims for the IFMA SIMD lane converters
 * (fq51x2 from_pair/to_pair/broadcast, fq51x8 insert_lane/extract_lane/
 * broadcast_fe). Those backends compute in radix-2^51, so their fq_fe<->lane
 * glue must move between fq_fe and 5x51 limbs.
 *
 *   - NATIVE64 (fq_fe = uint64_t[4]): expand on load, normalize+pack on store.
 *   - radix-2^51 fq_fe (uint64_t[5]; MSVC, non-BMI2, forced-5x51): fq_fe IS
 *     already 5x51, so these are straight 5-limb copies — byte-identical to
 *     the pre-native-migration shim behavior (limbs left lazy < 2^52; callers
 *     canonicalize downstream, exactly as the baseline relied on). Routing the
 *     raw fq64_expand_5x51 / fq51_normalize5_pack4 here unconditionally would
 *     misread the radix and read/write fq_fe[4] out of bounds on this path.
 */
#if RANSHAW_FQ_NATIVE64
static inline void fq_fe_to_5x51(uint64_t h5[5], const fq_fe a)
{
    fq64_expand_5x51(h5, a);
}
static inline void fq_fe_from_5x51(fq_fe r, const uint64_t f5[5])
{
    fq51_normalize5_pack4(r, f5);
}
#else
static inline void fq_fe_to_5x51(uint64_t h5[5], const fq_fe a)
{
    h5[0] = a[0];
    h5[1] = a[1];
    h5[2] = a[2];
    h5[3] = a[3];
    h5[4] = a[4];
}
static inline void fq_fe_from_5x51(fq_fe r, const uint64_t f5[5])
{
    r[0] = f5[0];
    r[1] = f5[1];
    r[2] = f5[2];
    r[3] = f5[3];
    r[4] = f5[4];
}
#endif

/*
 * Single-curve assumption: the 4×64 ADX inline assembly in fq51_inline.h is
 * hand-derived for TWO_GAMMA_64_LIMBS == 3 (i.e. gamma ≤ 128 bits). A swap
 * to a smaller-gamma curve (TWO_GAMMA_64_LIMBS < 3) works with the existing
 * asm correctly — the unused gamma limbs contribute zero to the convolution,
 * so tightening for a smaller gamma is a purely subtractive edit (remove
 * the MULX block for the zeroed limb). A swap to LARGER gamma requires
 * re-deriving the asm from scratch; this static_assert flags it at compile
 * time rather than silently falling through to the C reference.
 *
 * GAMMA_51_LIMBS has the same invariant: the 5×51 fallback paths and all
 * hand-unrolled post-normalize chains in fq51_inline.h assume it equals 3.
 */
static_assert(
    TWO_GAMMA_64_LIMBS == 3,
    "Ran/Shaw asm is hand-derived for 3-limb 2*gamma (128-bit gamma); "
    "re-derive fq51_inline.h before changing this.");
static_assert(
    GAMMA_51_LIMBS == 3,
    "Ran/Shaw 5×51 fold chains assume GAMMA_51_LIMBS == 3; "
    "re-derive fq51_inline.h before changing this.");

/*
 * q in radix-2^51:
 *   Q_51[0] = 0x4645EC70F85F
 *   Q_51[1] = 0x1C72E61F4EE2D
 *   Q_51[2] = 0x7FFFFFD2EC7AC
 *   Q_51[3] = 0x7FFFFFFFFFFFF
 *   Q_51[4] = 0x7FFFFFFFFFFFF
 */
static const uint64_t Q_51[5] =
    {0x4645EC70F85FULL, 0x1C72E61F4EE2DULL, 0x7FFFFFD2EC7ACULL, 0x7FFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFULL};

/*
 * 128*q bias in radix-2^51, used in fq_sub to prevent underflow.
 * BIAS_Q_51[i] = 128 * Q_51[i], all fit in 58 bits and all >= 2^53.
 *
 * Fp uses 4p bias (all 4p limbs ≈ 2^53) because p = 2^255 − 19 has all limbs
 * near 2^51. For Fq = 2^255 − gamma (gamma ≈ 2^128), the lower limbs of q are
 * significantly less than 2^51, so even 8q limbs can fall below 2^53. We use
 * 128q to ensure all bias limbs comfortably exceed 2^53, handling up to 53-bit
 * input limbs (two chained lazy additions before subtraction).
 */
static const uint64_t BIAS_Q_51[5] =
    {0x2322F6387C2F80ULL, 0xE39730FA771680ULL, 0x3FFFFFE9763D600ULL, 0x3FFFFFFFFFFFF80ULL, 0x3FFFFFFFFFFFF80ULL};

#endif // RANSHAW_X64_FQ51_H
