#ifndef HELIOSELENE_X64_FQ51_INLINE_H
#define HELIOSELENE_X64_FQ51_INLINE_H

#include "helioselene_platform.h"
#include "fq.h"
#include "x64/fq51.h"
#include "x64/mul128.h"

#if !defined(HELIOSELENE_FORCE_INLINE)
#if defined(_MSC_VER)
#define HELIOSELENE_FORCE_INLINE __forceinline
#else
#define HELIOSELENE_FORCE_INLINE inline __attribute__((always_inline))
#endif
#endif

/*
 * Crandall reduction for F_q = GF(2^255 - gamma).
 *
 * After a 5x5 schoolbook multiply producing 10 limbs (representing a ~510-bit value),
 * we split at 255 bits into lo (limbs 0-4) and hi (limbs 5-9), then compute:
 *   result = lo + hi * gamma (mod q)
 *
 * Since gamma is 127 bits (3 limbs), the wide multiply hi * gamma is 5-limb x 3-limb.
 * The result fits in ~383 bits, so a second Crandall round handles any remaining overflow.
 *
 * Key difference from ed25519: we do NOT fold during the schoolbook phase.
 * The per-limb fold-back (carry * 19) works for ed25519 because 19 is 5 bits.
 * For gamma (~127 bits), per-limb fold would overflow 128-bit intermediates.
 * Instead, we produce the full product first, then reduce separately.
 */

/*
 * Crandall reduction: takes 10 limbs (each ≤ 51 bits after carry) representing
 * a value up to ~510 bits, reduces mod q = 2^255 - gamma.
 */
static HELIOSELENE_FORCE_INLINE void fq51_crandall_reduce(fq_fe h, const uint64_t l[10])
{
    /* lo = l[0..4], hi = l[5..9] */
    /* Compute hi * gamma and add to lo */
    /* gamma has 3 limbs: GAMMA_51[0..2] */

#if HELIOSELENE_HAVE_INT128
    /* Wide multiply: hi[0..4] * gamma[0..2] -> 8 limbs (indices 0..7) */
    /* Each product pair: hi[i] * gamma[j] at position (i+j) */
    helioselene_uint128 r0 = (helioselene_uint128)l[0] + mul64(l[5], GAMMA_51[0]);
    helioselene_uint128 r1 = (helioselene_uint128)l[1] + mul64(l[5], GAMMA_51[1]) + mul64(l[6], GAMMA_51[0]);
    helioselene_uint128 r2 = (helioselene_uint128)l[2] + mul64(l[5], GAMMA_51[2]) + mul64(l[6], GAMMA_51[1])
                             + mul64(l[7], GAMMA_51[0]);
    helioselene_uint128 r3 = (helioselene_uint128)l[3] + mul64(l[6], GAMMA_51[2]) + mul64(l[7], GAMMA_51[1])
                             + mul64(l[8], GAMMA_51[0]);
    helioselene_uint128 r4 = (helioselene_uint128)l[4] + mul64(l[7], GAMMA_51[2]) + mul64(l[8], GAMMA_51[1])
                             + mul64(l[9], GAMMA_51[0]);
    helioselene_uint128 r5 = mul64(l[8], GAMMA_51[2]) + mul64(l[9], GAMMA_51[1]);
    helioselene_uint128 r6 = mul64(l[9], GAMMA_51[2]);

    /* Carry-propagate r0..r6 into 7+ clean limbs */
    uint64_t carry;
    carry = (uint64_t)(r0 >> 51);
    r1 += carry;
    r0 &= FQ51_MASK;
    carry = (uint64_t)(r1 >> 51);
    r2 += carry;
    r1 &= FQ51_MASK;
    carry = (uint64_t)(r2 >> 51);
    r3 += carry;
    r2 &= FQ51_MASK;
    carry = (uint64_t)(r3 >> 51);
    r4 += carry;
    r3 &= FQ51_MASK;
    carry = (uint64_t)(r4 >> 51);
    r5 += carry;
    r4 &= FQ51_MASK;
    carry = (uint64_t)(r5 >> 51);
    r6 += carry;
    r5 &= FQ51_MASK;
    /*
     * r5 is 51 bits (masked). r6 can be up to ~74 bits (exceeds uint64_t!).
     * Split r6 into 51-bit limbs to get 3 hi limbs that all fit in uint64_t.
     */
    uint64_t hi2_0 = (uint64_t)r5;
    uint64_t hi2_1 = (uint64_t)r6 & FQ51_MASK;
    uint64_t hi2_2 = (uint64_t)(r6 >> 51);  /* at most ~23 bits */

    /*
     * Second Crandall round: fold hi2[0..2] * gamma[0..2] back into lo.
     * All limbs are ≤ 51 bits, so products fit in 102 bits (safe for mul64).
     * hi2_value = hi2_0 + hi2_1*2^51 + hi2_2*2^102
     * Result positions: hi2[i] * gamma[j] -> position (i+j)
     */
    helioselene_uint128 s0 = (helioselene_uint128)(uint64_t)r0 + mul64(hi2_0, GAMMA_51[0]);
    helioselene_uint128 s1 = (helioselene_uint128)(uint64_t)r1 + mul64(hi2_0, GAMMA_51[1])
                             + mul64(hi2_1, GAMMA_51[0]);
    helioselene_uint128 s2 = (helioselene_uint128)(uint64_t)r2 + mul64(hi2_0, GAMMA_51[2])
                             + mul64(hi2_1, GAMMA_51[1]) + mul64(hi2_2, GAMMA_51[0]);
    helioselene_uint128 s3 = (helioselene_uint128)(uint64_t)r3 + mul64(hi2_1, GAMMA_51[2])
                             + mul64(hi2_2, GAMMA_51[1]);
    helioselene_uint128 s4 = (helioselene_uint128)(uint64_t)r4 + mul64(hi2_2, GAMMA_51[2]);

    /* Final carry chain */
    carry = (uint64_t)(s0 >> 51);
    s1 += carry;
    uint64_t o0 = (uint64_t)s0 & FQ51_MASK;
    carry = (uint64_t)(s1 >> 51);
    s2 += carry;
    uint64_t o1 = (uint64_t)s1 & FQ51_MASK;
    carry = (uint64_t)(s2 >> 51);
    s3 += carry;
    uint64_t o2 = (uint64_t)s2 & FQ51_MASK;
    carry = (uint64_t)(s3 >> 51);
    s4 += carry;
    uint64_t o3 = (uint64_t)s3 & FQ51_MASK;
    carry = (uint64_t)(s4 >> 51);
    uint64_t o4 = (uint64_t)s4 & FQ51_MASK;
    /* Third round (tiny): carry is at most a few bits */
    o0 += carry * GAMMA_51[0];
    o1 += carry * GAMMA_51[1];
    o2 += carry * GAMMA_51[2];
    carry = o0 >> 51;
    o1 += carry;
    o0 &= FQ51_MASK;
    carry = o1 >> 51;
    o2 += carry;
    o1 &= FQ51_MASK;

    h[0] = o0;
    h[1] = o1;
    h[2] = o2;
    h[3] = o3;
    h[4] = o4;

#elif HELIOSELENE_HAVE_UMUL128
    helioselene_uint128_emu r0 = {l[0], 0};
    r0 += mul64(l[5], GAMMA_51[0]);
    helioselene_uint128_emu r1 = {l[1], 0};
    r1 += mul64(l[5], GAMMA_51[1]);
    r1 += mul64(l[6], GAMMA_51[0]);
    helioselene_uint128_emu r2 = {l[2], 0};
    r2 += mul64(l[5], GAMMA_51[2]);
    r2 += mul64(l[6], GAMMA_51[1]);
    r2 += mul64(l[7], GAMMA_51[0]);
    helioselene_uint128_emu r3 = {l[3], 0};
    r3 += mul64(l[6], GAMMA_51[2]);
    r3 += mul64(l[7], GAMMA_51[1]);
    r3 += mul64(l[8], GAMMA_51[0]);
    helioselene_uint128_emu r4 = {l[4], 0};
    r4 += mul64(l[7], GAMMA_51[2]);
    r4 += mul64(l[8], GAMMA_51[1]);
    r4 += mul64(l[9], GAMMA_51[0]);
    helioselene_uint128_emu r5 = mul64(l[8], GAMMA_51[2]);
    r5 += mul64(l[9], GAMMA_51[1]);
    helioselene_uint128_emu r6 = mul64(l[9], GAMMA_51[2]);

    uint64_t carry;
    carry = shr128(r0, 51);
    r1 += carry;
    uint64_t v0 = lo128(r0) & FQ51_MASK;
    carry = shr128(r1, 51);
    r2 += carry;
    uint64_t v1 = lo128(r1) & FQ51_MASK;
    carry = shr128(r2, 51);
    r3 += carry;
    uint64_t v2 = lo128(r2) & FQ51_MASK;
    carry = shr128(r3, 51);
    r4 += carry;
    uint64_t v3 = lo128(r3) & FQ51_MASK;
    carry = shr128(r4, 51);
    r5 += carry;
    uint64_t v4 = lo128(r4) & FQ51_MASK;
    carry = shr128(r5, 51);
    r6 += carry;
    uint64_t hi2_0 = lo128(r5) & FQ51_MASK;
    uint64_t hi2_1 = lo128(r6) & FQ51_MASK;
    uint64_t hi2_2 = shr128(r6, 51);

    helioselene_uint128_emu s0 = {v0, 0};
    s0 += mul64(hi2_0, GAMMA_51[0]);
    helioselene_uint128_emu s1 = {v1, 0};
    s1 += mul64(hi2_0, GAMMA_51[1]);
    s1 += mul64(hi2_1, GAMMA_51[0]);
    helioselene_uint128_emu s2 = {v2, 0};
    s2 += mul64(hi2_0, GAMMA_51[2]);
    s2 += mul64(hi2_1, GAMMA_51[1]);
    s2 += mul64(hi2_2, GAMMA_51[0]);
    helioselene_uint128_emu s3 = {v3, 0};
    s3 += mul64(hi2_1, GAMMA_51[2]);
    s3 += mul64(hi2_2, GAMMA_51[1]);
    helioselene_uint128_emu s4 = {v4, 0};
    s4 += mul64(hi2_2, GAMMA_51[2]);

    carry = shr128(s0, 51);
    s1 += carry;
    uint64_t o0 = lo128(s0) & FQ51_MASK;
    carry = shr128(s1, 51);
    s2 += carry;
    uint64_t o1 = lo128(s1) & FQ51_MASK;
    carry = shr128(s2, 51);
    s3 += carry;
    uint64_t o2 = lo128(s2) & FQ51_MASK;
    carry = shr128(s3, 51);
    s4 += carry;
    uint64_t o3 = lo128(s3) & FQ51_MASK;
    carry = shr128(s4, 51);
    uint64_t o4 = lo128(s4) & FQ51_MASK;
    o0 += carry * GAMMA_51[0];
    o1 += carry * GAMMA_51[1];
    o2 += carry * GAMMA_51[2];
    carry = o0 >> 51;
    o1 += carry;
    o0 &= FQ51_MASK;
    carry = o1 >> 51;
    o2 += carry;
    o1 &= FQ51_MASK;

    h[0] = o0;
    h[1] = o1;
    h[2] = o2;
    h[3] = o3;
    h[4] = o4;
#endif
}

static HELIOSELENE_FORCE_INLINE void fq51_mul_inline(fq_fe h, const fq_fe f, const fq_fe g)
{
    uint64_t f0 = f[0], f1 = f[1], f2 = f[2], f3 = f[3], f4 = f[4];
    uint64_t g0 = g[0], g1 = g[1], g2 = g[2], g3 = g[3], g4 = g[4];

    /*
     * Full 5x5 schoolbook → 9 accumulators h0..h8 (NO inline fold).
     * Product limb k = sum of f[i]*g[j] where i+j=k.
     */
#if HELIOSELENE_HAVE_INT128
    helioselene_uint128 h0 = mul64(f0, g0);
    helioselene_uint128 h1 = mul64(f0, g1) + mul64(f1, g0);
    helioselene_uint128 h2 = mul64(f0, g2) + mul64(f1, g1) + mul64(f2, g0);
    helioselene_uint128 h3 = mul64(f0, g3) + mul64(f1, g2) + mul64(f2, g1) + mul64(f3, g0);
    helioselene_uint128 h4 = mul64(f0, g4) + mul64(f1, g3) + mul64(f2, g2) + mul64(f3, g1) + mul64(f4, g0);
    helioselene_uint128 h5 = mul64(f1, g4) + mul64(f2, g3) + mul64(f3, g2) + mul64(f4, g1);
    helioselene_uint128 h6 = mul64(f2, g4) + mul64(f3, g3) + mul64(f4, g2);
    helioselene_uint128 h7 = mul64(f3, g4) + mul64(f4, g3);
    helioselene_uint128 h8 = mul64(f4, g4);

    /* Carry-propagate to 10 clean limbs */
    uint64_t carry, l[10];
    carry = (uint64_t)(h0 >> 51);
    h1 += carry;
    l[0] = (uint64_t)h0 & FQ51_MASK;
    carry = (uint64_t)(h1 >> 51);
    h2 += carry;
    l[1] = (uint64_t)h1 & FQ51_MASK;
    carry = (uint64_t)(h2 >> 51);
    h3 += carry;
    l[2] = (uint64_t)h2 & FQ51_MASK;
    carry = (uint64_t)(h3 >> 51);
    h4 += carry;
    l[3] = (uint64_t)h3 & FQ51_MASK;
    carry = (uint64_t)(h4 >> 51);
    h5 += carry;
    l[4] = (uint64_t)h4 & FQ51_MASK;
    carry = (uint64_t)(h5 >> 51);
    h6 += carry;
    l[5] = (uint64_t)h5 & FQ51_MASK;
    carry = (uint64_t)(h6 >> 51);
    h7 += carry;
    l[6] = (uint64_t)h6 & FQ51_MASK;
    carry = (uint64_t)(h7 >> 51);
    h8 += carry;
    l[7] = (uint64_t)h7 & FQ51_MASK;
    l[8] = (uint64_t)h8 & FQ51_MASK;
    l[9] = (uint64_t)(h8 >> 51);

#elif HELIOSELENE_HAVE_UMUL128
    helioselene_uint128_emu h0 = mul64(f0, g0);
    helioselene_uint128_emu h1 = mul64(f0, g1);
    h1 += mul64(f1, g0);
    helioselene_uint128_emu h2 = mul64(f0, g2);
    h2 += mul64(f1, g1);
    h2 += mul64(f2, g0);
    helioselene_uint128_emu h3 = mul64(f0, g3);
    h3 += mul64(f1, g2);
    h3 += mul64(f2, g1);
    h3 += mul64(f3, g0);
    helioselene_uint128_emu h4 = mul64(f0, g4);
    h4 += mul64(f1, g3);
    h4 += mul64(f2, g2);
    h4 += mul64(f3, g1);
    h4 += mul64(f4, g0);
    helioselene_uint128_emu h5 = mul64(f1, g4);
    h5 += mul64(f2, g3);
    h5 += mul64(f3, g2);
    h5 += mul64(f4, g1);
    helioselene_uint128_emu h6 = mul64(f2, g4);
    h6 += mul64(f3, g3);
    h6 += mul64(f4, g2);
    helioselene_uint128_emu h7 = mul64(f3, g4);
    h7 += mul64(f4, g3);
    helioselene_uint128_emu h8 = mul64(f4, g4);

    uint64_t carry, l[10];
    carry = shr128(h0, 51);
    h1 += carry;
    l[0] = lo128(h0) & FQ51_MASK;
    carry = shr128(h1, 51);
    h2 += carry;
    l[1] = lo128(h1) & FQ51_MASK;
    carry = shr128(h2, 51);
    h3 += carry;
    l[2] = lo128(h2) & FQ51_MASK;
    carry = shr128(h3, 51);
    h4 += carry;
    l[3] = lo128(h3) & FQ51_MASK;
    carry = shr128(h4, 51);
    h5 += carry;
    l[4] = lo128(h4) & FQ51_MASK;
    carry = shr128(h5, 51);
    h6 += carry;
    l[5] = lo128(h5) & FQ51_MASK;
    carry = shr128(h6, 51);
    h7 += carry;
    l[6] = lo128(h6) & FQ51_MASK;
    carry = shr128(h7, 51);
    h8 += carry;
    l[7] = lo128(h7) & FQ51_MASK;
    l[8] = lo128(h8) & FQ51_MASK;
    l[9] = shr128(h8, 51);
#endif

    fq51_crandall_reduce(h, l);
}

static HELIOSELENE_FORCE_INLINE void fq51_sq_inline(fq_fe h, const fq_fe f)
{
    uint64_t f0 = f[0], f1 = f[1], f2 = f[2], f3 = f[3], f4 = f[4];

    uint64_t f0_2 = 2 * f0;
    uint64_t f1_2 = 2 * f1;
    uint64_t f2_2 = 2 * f2;
    uint64_t f3_2 = 2 * f3;

#if HELIOSELENE_HAVE_INT128
    helioselene_uint128 h0 = mul64(f0, f0);
    helioselene_uint128 h1 = mul64(f0_2, f1);
    helioselene_uint128 h2 = mul64(f0_2, f2) + mul64(f1, f1);
    helioselene_uint128 h3 = mul64(f0_2, f3) + mul64(f1_2, f2);
    helioselene_uint128 h4 = mul64(f0_2, f4) + mul64(f1_2, f3) + mul64(f2, f2);
    helioselene_uint128 h5 = mul64(f1_2, f4) + mul64(f2_2, f3);
    helioselene_uint128 h6 = mul64(f2_2, f4) + mul64(f3, f3);
    helioselene_uint128 h7 = mul64(f3_2, f4);
    helioselene_uint128 h8 = mul64(f4, f4);

    uint64_t carry, l[10];
    carry = (uint64_t)(h0 >> 51);
    h1 += carry;
    l[0] = (uint64_t)h0 & FQ51_MASK;
    carry = (uint64_t)(h1 >> 51);
    h2 += carry;
    l[1] = (uint64_t)h1 & FQ51_MASK;
    carry = (uint64_t)(h2 >> 51);
    h3 += carry;
    l[2] = (uint64_t)h2 & FQ51_MASK;
    carry = (uint64_t)(h3 >> 51);
    h4 += carry;
    l[3] = (uint64_t)h3 & FQ51_MASK;
    carry = (uint64_t)(h4 >> 51);
    h5 += carry;
    l[4] = (uint64_t)h4 & FQ51_MASK;
    carry = (uint64_t)(h5 >> 51);
    h6 += carry;
    l[5] = (uint64_t)h5 & FQ51_MASK;
    carry = (uint64_t)(h6 >> 51);
    h7 += carry;
    l[6] = (uint64_t)h6 & FQ51_MASK;
    carry = (uint64_t)(h7 >> 51);
    h8 += carry;
    l[7] = (uint64_t)h7 & FQ51_MASK;
    l[8] = (uint64_t)h8 & FQ51_MASK;
    l[9] = (uint64_t)(h8 >> 51);

#elif HELIOSELENE_HAVE_UMUL128
    helioselene_uint128_emu h0 = mul64(f0, f0);
    helioselene_uint128_emu h1 = mul64(f0_2, f1);
    helioselene_uint128_emu h2 = mul64(f0_2, f2);
    h2 += mul64(f1, f1);
    helioselene_uint128_emu h3 = mul64(f0_2, f3);
    h3 += mul64(f1_2, f2);
    helioselene_uint128_emu h4 = mul64(f0_2, f4);
    h4 += mul64(f1_2, f3);
    h4 += mul64(f2, f2);
    helioselene_uint128_emu h5 = mul64(f1_2, f4);
    h5 += mul64(f2_2, f3);
    helioselene_uint128_emu h6 = mul64(f2_2, f4);
    h6 += mul64(f3, f3);
    helioselene_uint128_emu h7 = mul64(f3_2, f4);
    helioselene_uint128_emu h8 = mul64(f4, f4);

    uint64_t carry, l[10];
    carry = shr128(h0, 51);
    h1 += carry;
    l[0] = lo128(h0) & FQ51_MASK;
    carry = shr128(h1, 51);
    h2 += carry;
    l[1] = lo128(h1) & FQ51_MASK;
    carry = shr128(h2, 51);
    h3 += carry;
    l[2] = lo128(h2) & FQ51_MASK;
    carry = shr128(h3, 51);
    h4 += carry;
    l[3] = lo128(h3) & FQ51_MASK;
    carry = shr128(h4, 51);
    h5 += carry;
    l[4] = lo128(h4) & FQ51_MASK;
    carry = shr128(h5, 51);
    h6 += carry;
    l[5] = lo128(h5) & FQ51_MASK;
    carry = shr128(h6, 51);
    h7 += carry;
    l[6] = lo128(h6) & FQ51_MASK;
    carry = shr128(h7, 51);
    h8 += carry;
    l[7] = lo128(h7) & FQ51_MASK;
    l[8] = lo128(h8) & FQ51_MASK;
    l[9] = shr128(h8, 51);
#endif

    fq51_crandall_reduce(h, l);
}

#endif // HELIOSELENE_X64_FQ51_INLINE_H
