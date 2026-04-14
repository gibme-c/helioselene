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
 * @file shaw_point_ops_ifma.h
 * @brief Shaw (Fq) Jacobian dbl and mixed-add built on fq51x2 2-way IFMA.
 *
 * Two flavors:
 *   shaw_dbl_ifma_packed / shaw_madd_ifma_packed — accumulator crosses the
 *     call boundary as fq51x2_t (Option B). Callers maintain the broadcast
 *     invariant: both lanes hold the same value on entry and exit.
 *   shaw_dbl_ifma / shaw_madd_ifma — thin wrappers that pack fq_fe → packed
 *     → unpack, kept for the existing correctness test harness.
 *
 * Pairing strategy (dbl): 3 paired ops + 1 standalone sq
 *   Pair (Z,Y) -> sq         -> (δ, γ)
 *   Pair (X, X−δ) × (γ, X+δ) -> mul -> (β, α)
 *   Pair (α', Y+Z) -> sq     -> (α'², (Y+Z)²)
 *   Pair (α', γ) × (4β−X₃, γ) -> mul -> (α'·(4β−X₃), γ²)
 *
 * Pairing strategy (madd): 4 paired + 3 standalone
 *   Standalone sq:  Z1Z1 = Z1²
 *   Pair (qx,Z1) × (Z1Z1,Z1Z1) -> mul -> (U2, t0)
 *   Standalone mul: S2 = qy * t0
 *   Pair (H, Z1+H) -> sq     -> (HH, (Z1+H)²)
 *   Pair (H, X1) × (I, I) -> mul -> (J, V)
 *   Standalone sq:  rr²
 *   Pair (rr, Y1) × (V−X3, J) -> mul -> (rr·(V−X3), Y1·J)
 */

#ifndef RANSHAW_SHAW_X64_IFMA_POINT_OPS_H
#define RANSHAW_SHAW_X64_IFMA_POINT_OPS_H

#include "fq_ops.h"
#include "shaw.h"
#include "x64/fq51.h"
#include "x64/ifma/fq51x2_ifma.h"

#if defined(__AVX512F__) && defined(__AVX512IFMA__) && !defined(_MSC_VER)

/* ------------------------------------------------------------------
 * Packed Jacobian doubling (dbl-2001-b, a = -3)
 *
 * Both input lanes must hold the same value (broadcast invariant);
 * both output lanes hold the same result.
 * ------------------------------------------------------------------ */
static inline void
    shaw_dbl_ifma_packed(fq51x2_t rX, fq51x2_t rY, fq51x2_t rZ, const fq51x2_t pX, const fq51x2_t pY, const fq51x2_t pZ)
{
    /* Step 1: δ = Z², γ = Y² — pair via mix_lane1. */
    fq51x2_t pair_ZY, DG;
    fq51x2_mix_lane1(pair_ZY, pZ, pY); /* lane0=Z, lane1=Y */
    fq51x2_sq(DG, pair_ZY); /* lane0=δ, lane1=γ */

    /* Split: pDelta (broadcast δ), pGamma (broadcast γ). */
    fq51x2_t pDelta, pGamma;
    fq51x2_duplicate_lane0(pDelta, DG);
    fq51x2_duplicate_lane1(pGamma, DG);

    /* Step 2: X−δ, X+δ. */
    fq51x2_t pXmD, pXpD;
    fq51x2_sub(pXmD, pX, pDelta); /* canonical, < 2^52 */
    fq51x2_add(pXpD, pX, pDelta); /* lazy: X(<2^51) + δ(<2^52) < 2^53 */
    fq51x2_canonicalize(pXpD, pXpD); /* required: IFMA masks to 52 bits */

    /* β = X·γ, α = (X−δ)·(X+δ) — pair. */
    fq51x2_t pair_L, pair_R, BA;
    fq51x2_mix_lane1(pair_L, pX, pXmD); /* lane0=X, lane1=X−δ */
    fq51x2_mix_lane1(pair_R, pGamma, pXpD); /* lane0=γ, lane1=X+δ */
    fq51x2_mul(BA, pair_L, pair_R); /* lane0=β, lane1=α */

    fq51x2_t pBeta, pAlpha;
    fq51x2_duplicate_lane0(pBeta, BA);
    fq51x2_duplicate_lane1(pAlpha, BA);

    /* Step 3: α' = 3α, Y+Z. */
    fq51x2_t pAlphaP, pYpZ;
    fq51x2_scale_small(pAlphaP, pAlpha, 3); /* 3·(<2^52) ≈ 2^53.5 */
    fq51x2_canonicalize(pAlphaP, pAlphaP);
    fq51x2_add(pYpZ, pY, pZ); /* lazy, < 2^52 */
    fq51x2_canonicalize(pYpZ, pYpZ);

    /* Step 4: α'² and (Y+Z)² — pair. */
    fq51x2_t pair_sq, SQS;
    fq51x2_mix_lane1(pair_sq, pAlphaP, pYpZ);
    fq51x2_sq(SQS, pair_sq); /* lane0=α'², lane1=(Y+Z)² */

    fq51x2_t pApSq, pYpZSq;
    fq51x2_duplicate_lane0(pApSq, SQS);
    fq51x2_duplicate_lane1(pYpZSq, SQS);

    /* Step 5: X₃ = α'² − 8β, Z₃ = (Y+Z)² − γ − δ. */
    fq51x2_t pBeta8, pX3;
    fq51x2_scale_small(pBeta8, pBeta, 8);
    fq51x2_canonicalize(pBeta8, pBeta8);
    fq51x2_sub(pX3, pApSq, pBeta8);

    fq51x2_t pZ3_tmp, pZ3;
    fq51x2_sub(pZ3_tmp, pYpZSq, pGamma);
    fq51x2_sub(pZ3, pZ3_tmp, pDelta);

    /* Step 6: pair (α', γ) × (4β−X₃, γ) -> (α'·(4β−X₃), γ²). */
    fq51x2_t pBeta4, p4bmx;
    fq51x2_scale_small(pBeta4, pBeta, 4);
    fq51x2_canonicalize(pBeta4, pBeta4);
    fq51x2_sub(p4bmx, pBeta4, pX3);

    fq51x2_t pair2_L, pair2_R, Y12;
    fq51x2_mix_lane1(pair2_L, pAlphaP, pGamma);
    fq51x2_mix_lane1(pair2_R, p4bmx, pGamma);
    fq51x2_mul(Y12, pair2_L, pair2_R); /* lane0=α'·(4β−X₃), lane1=γ² */

    fq51x2_t pY1, pG2;
    fq51x2_duplicate_lane0(pY1, Y12);
    fq51x2_duplicate_lane1(pG2, Y12);

    /* Y₃ = y1 − 8γ². */
    fq51x2_t pG2_8, pY3;
    fq51x2_scale_small(pG2_8, pG2, 8);
    fq51x2_canonicalize(pG2_8, pG2_8);
    fq51x2_sub(pY3, pY1, pG2_8);

    fq51x2_copy(rX, pX3);
    fq51x2_copy(rY, pY3);
    fq51x2_copy(rZ, pZ3);
}

/* ------------------------------------------------------------------
 * Packed Jacobian + Affine mixed addition (madd-2007-bl).
 *
 * Broadcast invariant for all inputs and outputs.
 * ------------------------------------------------------------------ */
static inline void shaw_madd_ifma_packed(
    fq51x2_t rX,
    fq51x2_t rY,
    fq51x2_t rZ,
    const fq51x2_t pX,
    const fq51x2_t pY,
    const fq51x2_t pZ,
    const fq51x2_t qx,
    const fq51x2_t qy)
{
    /* Step 1: Z1Z1 = Z1². */
    fq51x2_t pZZ;
    fq51x2_sq(pZZ, pZ);

    /* Step 2: U2 = qx·Z1Z1, t0 = Z1·Z1Z1 — pair (qx,Z1) × (Z1Z1,Z1Z1). */
    fq51x2_t pair_L, R;
    fq51x2_mix_lane1(pair_L, qx, pZ);
    fq51x2_mul(R, pair_L, pZZ); /* lane0=U2, lane1=t0 */

    fq51x2_t pU2, pT0;
    fq51x2_duplicate_lane0(pU2, R);
    fq51x2_duplicate_lane1(pT0, R);

    /* Step 3: S2 = qy · t0. */
    fq51x2_t pS2;
    fq51x2_mul(pS2, qy, pT0);

    /* Step 4: H = U2 − X1. */
    fq51x2_t pH;
    fq51x2_sub(pH, pU2, pX); /* canonical */

    /* Step 5: Z1+H (lazy, need canon for sq), then pair (H, Z1+H) -> sq. */
    fq51x2_t pZ1pH;
    fq51x2_add(pZ1pH, pZ, pH);
    fq51x2_canonicalize(pZ1pH, pZ1pH);

    fq51x2_t pair_sq, R2;
    fq51x2_mix_lane1(pair_sq, pH, pZ1pH);
    fq51x2_sq(R2, pair_sq); /* lane0=HH, lane1=(Z1+H)² */

    fq51x2_t pHH, pZ1pHsq;
    fq51x2_duplicate_lane0(pHH, R2);
    fq51x2_duplicate_lane1(pZ1pHsq, R2);

    /* Step 6: I = 4·HH. */
    fq51x2_t pI;
    fq51x2_scale_small(pI, pHH, 4);
    fq51x2_canonicalize(pI, pI);

    /* Step 7: pair (H, X1) × (I, I) -> (J, V). */
    fq51x2_t pair_L2, R3;
    fq51x2_mix_lane1(pair_L2, pH, pX);
    fq51x2_mul(R3, pair_L2, pI);
    fq51x2_t pJ, pV;
    fq51x2_duplicate_lane0(pJ, R3);
    fq51x2_duplicate_lane1(pV, R3);

    /* Step 8: rr = 2·(S2 − Y1). */
    fq51x2_t pDiff, pRR;
    fq51x2_sub(pDiff, pS2, pY);
    fq51x2_scale_small(pRR, pDiff, 2);
    fq51x2_canonicalize(pRR, pRR);

    /* Step 9: rr². */
    fq51x2_t pRRsq;
    fq51x2_sq(pRRsq, pRR);

    /* Step 10: X3 = rr² − J − 2V. */
    fq51x2_t pV2, pTmp, pX3;
    fq51x2_scale_small(pV2, pV, 2);
    fq51x2_canonicalize(pV2, pV2);
    fq51x2_sub(pTmp, pRRsq, pJ);
    fq51x2_sub(pX3, pTmp, pV2);

    /* Step 11: V−X3 (both canonical, fits fq51x2_sub). */
    fq51x2_t pVmX3;
    fq51x2_sub(pVmX3, pV, pX3);

    /* Pair (rr, Y1) × (V−X3, J) -> (rr·(V−X3), Y1·J). */
    fq51x2_t pair_L3, pair_R3, R4;
    fq51x2_mix_lane1(pair_L3, pRR, pY);
    fq51x2_mix_lane1(pair_R3, pVmX3, pJ);
    fq51x2_mul(R4, pair_L3, pair_R3);
    fq51x2_t pT1, pT2;
    fq51x2_duplicate_lane0(pT1, R4);
    fq51x2_duplicate_lane1(pT2, R4);

    /* Step 12: Y3 = t1 − 2·t2. */
    fq51x2_t p2T2, pY3;
    fq51x2_scale_small(p2T2, pT2, 2);
    fq51x2_canonicalize(p2T2, p2T2);
    fq51x2_sub(pY3, pT1, p2T2);

    /* Step 13: Z3 = (Z1+H)² − Z1Z1 − HH. */
    fq51x2_t pZ3_tmp, pZ3;
    fq51x2_sub(pZ3_tmp, pZ1pHsq, pZZ);
    fq51x2_sub(pZ3, pZ3_tmp, pHH);

    fq51x2_copy(rX, pX3);
    fq51x2_copy(rY, pY3);
    fq51x2_copy(rZ, pZ3);
}

/* ------------------------------------------------------------------
 * fq_fe wrappers — preserve existing correctness test API.
 * ------------------------------------------------------------------ */
static inline void shaw_dbl_ifma(shaw_jacobian *r, const shaw_jacobian *p)
{
    fq51x2_t pX, pY, pZ, rX, rY, rZ;
    fq51x2_broadcast(pX, p->X);
    fq51x2_broadcast(pY, p->Y);
    fq51x2_broadcast(pZ, p->Z);
    shaw_dbl_ifma_packed(rX, rY, rZ, pX, pY, pZ);
    fq_fe d;
    fq51x2_to_pair(r->X, d, rX);
    fq51x2_to_pair(r->Y, d, rY);
    fq51x2_to_pair(r->Z, d, rZ);
}

static inline void shaw_madd_ifma(shaw_jacobian *r, const shaw_jacobian *p, const shaw_affine *q)
{
    fq51x2_t pX, pY, pZ, qx, qy, rX, rY, rZ;
    fq51x2_broadcast(pX, p->X);
    fq51x2_broadcast(pY, p->Y);
    fq51x2_broadcast(pZ, p->Z);
    fq51x2_broadcast(qx, q->x);
    fq51x2_broadcast(qy, q->y);
    shaw_madd_ifma_packed(rX, rY, rZ, pX, pY, pZ, qx, qy);
    fq_fe d;
    fq51x2_to_pair(r->X, d, rX);
    fq51x2_to_pair(r->Y, d, rY);
    fq51x2_to_pair(r->Z, d, rZ);
}

#endif /* __AVX512F__ && __AVX512IFMA__ && !_MSC_VER */

#endif /* RANSHAW_SHAW_X64_IFMA_POINT_OPS_H */
