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
 * @file shaw_projective_8x.h
 * @brief 8-way parallel homogeneous projective points for Shaw. Mirror of
 *        ran_projective_8x.h.
 */

#ifndef RANSHAW_X64_IFMA_SHAW_PROJECTIVE_8X_H
#define RANSHAW_X64_IFMA_SHAW_PROJECTIVE_8X_H

#include "fq.h"
#include "shaw_projective.h"
#include "x64/ifma/fq51x8_ifma.h"

#include <immintrin.h>

typedef struct alignas(64)
{
    fq51x8 X, Y, Z;
} shaw_projective_8x;

static inline void shaw_proj_identity_8x(shaw_projective_8x *r)
{
    fq51x8_0(&r->X);
    fq51x8_1(&r->Y);
    fq51x8_0(&r->Z);
}

static inline void shaw_proj_copy_8x(shaw_projective_8x *r, const shaw_projective_8x *p)
{
    fq51x8_copy(&r->X, &p->X);
    fq51x8_copy(&r->Y, &p->Y);
    fq51x8_copy(&r->Z, &p->Z);
}

static inline void shaw_proj_cmov_8x(shaw_projective_8x *t, const shaw_projective_8x *u, __mmask8 mask)
{
    fq51x8_cmov(&t->X, &u->X, mask);
    fq51x8_cmov(&t->Y, &u->Y, mask);
    fq51x8_cmov(&t->Z, &u->Z, mask);
}

static inline void shaw_proj_cneg_8x(shaw_projective_8x *r, __mmask8 mask)
{
    fq51x8 neg_y;
    fq51x8_neg(&neg_y, &r->Y);
    fq51x8_cmov(&r->Y, &neg_y, mask);
}

static inline void fq51x8_broadcast_fe(fq51x8 *out, const fq_fe in)
{
    /* fq_fe -> 5x51 broadcast across all 8 lanes (expand on native 4x64;
     * direct copy of the 5x51 limbs otherwise). */
    uint64_t in5[5];
    fq_fe_to_5x51(in5, in);
    out->v[0] = _mm512_set1_epi64((long long)in5[0]);
    out->v[1] = _mm512_set1_epi64((long long)in5[1]);
    out->v[2] = _mm512_set1_epi64((long long)in5[2]);
    out->v[3] = _mm512_set1_epi64((long long)in5[3]);
    out->v[4] = _mm512_set1_epi64((long long)in5[4]);
}

static inline void shaw_pack_proj_8x(
    shaw_projective_8x *out,
    const shaw_projective *p0,
    const shaw_projective *p1,
    const shaw_projective *p2,
    const shaw_projective *p3,
    const shaw_projective *p4,
    const shaw_projective *p5,
    const shaw_projective *p6,
    const shaw_projective *p7)
{
    const shaw_projective *ps[8] = {p0, p1, p2, p3, p4, p5, p6, p7};

    shaw_projective id;
    fq_0(id.X);
    fq_1(id.Y);
    fq_0(id.Z);

    for (int k = 0; k < 8; k++)
    {
        const shaw_projective *src = ps[k] ? ps[k] : &id;
        fq51x8_insert_lane(&out->X, src->X, k);
        fq51x8_insert_lane(&out->Y, src->Y, k);
        fq51x8_insert_lane(&out->Z, src->Z, k);
    }
}

static inline void shaw_unpack_proj_8x(
    shaw_projective *p0,
    shaw_projective *p1,
    shaw_projective *p2,
    shaw_projective *p3,
    shaw_projective *p4,
    shaw_projective *p5,
    shaw_projective *p6,
    shaw_projective *p7,
    const shaw_projective_8x *in)
{
    shaw_projective *ps[8] = {p0, p1, p2, p3, p4, p5, p6, p7};
    for (int k = 0; k < 8; k++)
    {
        fq51x8_extract_lane(ps[k]->X, &in->X, k);
        fq51x8_extract_lane(ps[k]->Y, &in->Y, k);
        fq51x8_extract_lane(ps[k]->Z, &in->Z, k);
    }
}

#endif // RANSHAW_X64_IFMA_SHAW_PROJECTIVE_8X_H
