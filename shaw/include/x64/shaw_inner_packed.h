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
 * @file shaw_inner_packed.h
 * @brief Private declarations of 4×64 packed Shaw point-ops and shared
 *        pack/unpack/cmov helpers. Only visible inside the x64 Shaw backend
 *        (not part of the public API). Function bodies live in
 *        shaw_dbl.cpp / shaw_madd.cpp / shaw_add.cpp; inline helpers are
 *        defined in this header so callers in scalarmult / scalarmult_vartime
 *        / msm_vartime share a single definition.
 *
 *        When FQ51_HAVE_ADX_MUL is not set (e.g. MSVC), this header declares
 *        nothing — the call sites are also guarded by the same macro.
 */

#ifndef RANSHAW_X64_SHAW_INNER_PACKED_H
#define RANSHAW_X64_SHAW_INNER_PACKED_H

#include "ranshaw_ct_barrier.h"
#include "shaw.h"
#include "x64/fq51_inline.h"

#if defined(FQ51_HAVE_ADX_MUL)

#include <stdint.h>

/* 4x64 packed Jacobian point used across the ADX-capable shaw backend
 * (msm_vartime, scalarmult, scalarmult_vartime). Each coordinate is
 * 4 uint64_t limbs at radix 2^64 with Crandall-folded output. */
struct packed_jac
{
    uint64_t X[4];
    uint64_t Y[4];
    uint64_t Z[4];
};

static RANSHAW_FORCE_INLINE void pack_jac(packed_jac *out, const shaw_jacobian *in)
{
    fq51_normalize_and_pack(out->X, in->X);
    fq51_normalize_and_pack(out->Y, in->Y);
    fq51_normalize_and_pack(out->Z, in->Z);
}

static RANSHAW_FORCE_INLINE void copy_packed(packed_jac *dst, const packed_jac *src)
{
    dst->X[0] = src->X[0];
    dst->X[1] = src->X[1];
    dst->X[2] = src->X[2];
    dst->X[3] = src->X[3];
    dst->Y[0] = src->Y[0];
    dst->Y[1] = src->Y[1];
    dst->Y[2] = src->Y[2];
    dst->Y[3] = src->Y[3];
    dst->Z[0] = src->Z[0];
    dst->Z[1] = src->Z[1];
    dst->Z[2] = src->Z[2];
    dst->Z[3] = src->Z[3];
}

/* A packed Jacobian value is identity iff Z == 0. In 4x64 form the
 * post-op limb profile is strictly sub-2q, so zero on Z is encoded
 * exactly as all four limbs zero (no non-canonical q-representations
 * of zero leak through the fq64_* pipeline once the 3-stage Crandall
 * fold has run). */
static RANSHAW_FORCE_INLINE int is_identity_packed(const packed_jac *p)
{
    return (p->Z[0] | p->Z[1] | p->Z[2] | p->Z[3]) == 0;
}

/* Jacobian doubling in 4×64 packed form (3M + 5S). Element-wise aliasing
 * between the three result arrays and the three input arrays is allowed. */
void shaw_dbl_x64_packed(
    uint64_t rX[4],
    uint64_t rY[4],
    uint64_t rZ[4],
    const uint64_t pX[4],
    const uint64_t pY[4],
    const uint64_t pZ[4]);

/* Jacobian + affine mixed addition in 4×64 packed form (7M + 4S).
 * Element-wise aliasing between the three result arrays and the first three
 * input arrays (Jacobian side) is allowed. qx/qy are read-only. */
void shaw_madd_x64_packed(
    uint64_t rX[4],
    uint64_t rY[4],
    uint64_t rZ[4],
    const uint64_t pX[4],
    const uint64_t pY[4],
    const uint64_t pZ[4],
    const uint64_t qx[4],
    const uint64_t qy[4]);

/* Jacobian + Jacobian addition in 4×64 packed form (11M + 5S).
 * Incomplete formula (raw EFD add-2007-bl): does NOT handle p==q, p==-q,
 * or identity inputs. Edge cases are the caller's responsibility.
 * Element-wise aliasing between the three result arrays and the six input
 * arrays is allowed (the body writes to scratch locals before committing
 * to the result arrays). */
void shaw_add_x64_packed(
    uint64_t rX[4],
    uint64_t rY[4],
    uint64_t rZ[4],
    const uint64_t pX[4],
    const uint64_t pY[4],
    const uint64_t pZ[4],
    const uint64_t qX[4],
    const uint64_t qY[4],
    const uint64_t qZ[4]);

/* Prefixed-entry variant of shaw_add_x64_packed: accepts precomputed
 * Z1Z1 = pZ^2, Z2Z2 = qZ^2, U1 = pX*Z2Z2, U2 = qX*Z1Z1 and skips those
 * 2S + 2M at the head of the EFD add-2007-bl formula. pX and qX are not
 * needed since they feed only U1 and U2. Used by shaw_add_safe_packed,
 * which already computes the prefix as part of its projective-X
 * collision test.
 *
 * Same aliasing contract as shaw_add_x64_packed. */
void shaw_add_x64_packed_prefixed(
    uint64_t rX[4],
    uint64_t rY[4],
    uint64_t rZ[4],
    const uint64_t pY[4],
    const uint64_t pZ[4],
    const uint64_t qY[4],
    const uint64_t qZ[4],
    const uint64_t Z1Z1[4],
    const uint64_t Z2Z2[4],
    const uint64_t U1[4],
    const uint64_t U2[4]);

/* Jacobian negation in 4×64 packed form: r = (pX, q - pY, pZ).
 * Element-wise aliasing between the result arrays and the input arrays is
 * allowed. */
void shaw_jac_neg_x64_packed(
    uint64_t rX[4],
    uint64_t rY[4],
    uint64_t rZ[4],
    const uint64_t pX[4],
    const uint64_t pY[4],
    const uint64_t pZ[4]);

/* Safe packed Jacobian add: r = p + q. Returns 1 if the algebraic sum
 * is identity (p == -q or both operands identity), 0 otherwise. Mirrors
 * the 5x51 edge-case logic in shaw_add.h's inline wrapper: handles
 * identity inputs via Z-limb-OR detection, and distinguishes p == q
 * (doubling) from p == -q via a single 4x64 projective-X diff unpacked
 * through 5x51 for fq_isnonzero. On the typical non-degenerate path
 * dispatches to shaw_add_x64_packed.
 *
 * Element-wise aliasing between r and p/q is allowed (same contract as
 * shaw_add_x64_packed itself). */
int shaw_add_safe_packed(packed_jac *r, const packed_jac *p, const packed_jac *q);

/* Constant-time conditional move for 4×64 packed arrays: r = b ? s : r.
 *
 * Mask pattern matches fq_cmov; mask must be derived from `b & 1` with a
 * CT barrier so the compiler cannot lower to a data-dependent branch. */
static RANSHAW_FORCE_INLINE void cmov_4x64(uint64_t r[4], const uint64_t s[4], unsigned int b)
{
    uint64_t mask = 0 - (uint64_t)ranshaw_ct_barrier_u32(b);
    r[0] ^= mask & (r[0] ^ s[0]);
    r[1] ^= mask & (r[1] ^ s[1]);
    r[2] ^= mask & (r[2] ^ s[2]);
    r[3] ^= mask & (r[3] ^ s[3]);
}

/* Unpack one 4×64 packed field into 5×51 with two-pass γ-fold post-normalize.
 * Same body used by shaw_add/dbl/madd public wrappers and by the scalarmult
 * exit path. */
static RANSHAW_FORCE_INLINE void unpack_and_normalize(fq_fe h, const uint64_t a[4])
{
    /* Native 4x64: the packed result IS an fq_fe. Canonically reduce to
     * [0, q) so stored point coordinates remain canonical. */
    fq64_reduce_canonical(h, a);
}

#endif /* FQ51_HAVE_ADX_MUL */

#endif /* RANSHAW_X64_SHAW_INNER_PACKED_H */
