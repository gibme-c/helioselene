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
 * @file ran_projective_8x.h
 * @brief 8-way parallel homogeneous projective points for Ran, used by the
 *        IFMA per-point Straus CT MSM driver.
 *
 * Mirror of ran_projective.h at the 8-lane level: each ran_projective_8x holds
 * eight independent projective points in AVX-512 ZMM lanes (three fp51x8
 * registers for X, Y, Z). Identity convention: Z = 0 in the corresponding
 * lane, matching the scalar projective representation.
 */

#ifndef RANSHAW_X64_IFMA_RAN_PROJECTIVE_8X_H
#define RANSHAW_X64_IFMA_RAN_PROJECTIVE_8X_H

#include "fp.h"
#include "ran_projective.h"
#include "x64/ifma/fp51x8_ifma.h"

#include <immintrin.h>

typedef struct alignas(64)
{
    fp51x8 X, Y, Z;
} ran_projective_8x;

/* Projective identity in all 8 lanes: (0 : 1 : 0). */
static inline void ran_proj_identity_8x(ran_projective_8x *r)
{
    fp51x8_0(&r->X);
    fp51x8_1(&r->Y);
    fp51x8_0(&r->Z);
}

static inline void ran_proj_copy_8x(ran_projective_8x *r, const ran_projective_8x *p)
{
    fp51x8_copy(&r->X, &p->X);
    fp51x8_copy(&r->Y, &p->Y);
    fp51x8_copy(&r->Z, &p->Z);
}

/* Per-lane conditional move: for each bit k in mask, lane k gets u's lane k. */
static inline void ran_proj_cmov_8x(ran_projective_8x *t, const ran_projective_8x *u, __mmask8 mask)
{
    fp51x8_cmov(&t->X, &u->X, mask);
    fp51x8_cmov(&t->Y, &u->Y, mask);
    fp51x8_cmov(&t->Z, &u->Z, mask);
}

/* Per-lane conditional negate: lanes where mask bit is set get Y -> -Y. */
static inline void ran_proj_cneg_8x(ran_projective_8x *r, __mmask8 mask)
{
    fp51x8 neg_y;
    fp51x8_neg(&neg_y, &r->Y);
    fp51x8_cmov(&r->Y, &neg_y, mask);
}

/* Broadcast a scalar fp_fe into all 8 lanes of an fp51x8. */
static inline void fp51x8_broadcast_fe(fp51x8 *out, const fp_fe in)
{
    out->v[0] = _mm512_set1_epi64((long long)in[0]);
    out->v[1] = _mm512_set1_epi64((long long)in[1]);
    out->v[2] = _mm512_set1_epi64((long long)in[2]);
    out->v[3] = _mm512_set1_epi64((long long)in[3]);
    out->v[4] = _mm512_set1_epi64((long long)in[4]);
}

/* Pack 8 scalar projective points into one 8-lane projective.
 *
 * pN may be NULL, which places the projective identity (0:1:0) in that lane.
 * Lanes index 0..7 correspond to p0..p7. */
static inline void ran_pack_proj_8x(
    ran_projective_8x *out,
    const ran_projective *p0,
    const ran_projective *p1,
    const ran_projective *p2,
    const ran_projective *p3,
    const ran_projective *p4,
    const ran_projective *p5,
    const ran_projective *p6,
    const ran_projective *p7)
{
    const ran_projective *ps[8] = {p0, p1, p2, p3, p4, p5, p6, p7};

    ran_projective id;
    fp_0(id.X);
    fp_1(id.Y);
    fp_0(id.Z);

    for (int k = 0; k < 8; k++)
    {
        const ran_projective *src = ps[k] ? ps[k] : &id;
        fp51x8_insert_lane(&out->X, src->X, k);
        fp51x8_insert_lane(&out->Y, src->Y, k);
        fp51x8_insert_lane(&out->Z, src->Z, k);
    }
}

/* Unpack an 8-lane projective into 8 scalar projectives. */
static inline void ran_unpack_proj_8x(
    ran_projective *p0,
    ran_projective *p1,
    ran_projective *p2,
    ran_projective *p3,
    ran_projective *p4,
    ran_projective *p5,
    ran_projective *p6,
    ran_projective *p7,
    const ran_projective_8x *in)
{
    ran_projective *ps[8] = {p0, p1, p2, p3, p4, p5, p6, p7};
    for (int k = 0; k < 8; k++)
    {
        fp51x8_extract_lane(ps[k]->X, &in->X, k);
        fp51x8_extract_lane(ps[k]->Y, &in->Y, k);
        fp51x8_extract_lane(ps[k]->Z, &in->Z, k);
    }
}

#endif // RANSHAW_X64_IFMA_RAN_PROJECTIVE_8X_H
