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

#include "x64/ifma/shaw_complete_add_ifma.h"

#include "shaw_constants.h"

/*
 * Shaw mirror of ran_complete_add_ifma_8x; see that file for the full
 * algorithm commentary and bound-tracking rationale.
 */
void shaw_complete_add_ifma_8x(shaw_projective_8x *r, const shaw_projective_8x *p, const shaw_projective_8x *q)
{
    fq51x8 t0, t1, t2, t3, t4, t5, X3, Y3, Z3;
    fq51x8 B3;
    fq51x8_broadcast_fe(&B3, SHAW_B3);

    /* Defensive input normalization: see ran_complete_add_ifma.cpp for
     * rationale. Scalar fq_fe outputs can carry 52-bit limbs which would
     * overflow vpmadd52's 52-bit mask after the first fq51x8_add chain. */
    fq51x8 p_x, p_y, p_z, q_x, q_y, q_z;
    fq51x8_copy(&p_x, &p->X);
    fq51x8_copy(&p_y, &p->Y);
    fq51x8_copy(&p_z, &p->Z);
    fq51x8_copy(&q_x, &q->X);
    fq51x8_copy(&q_y, &q->Y);
    fq51x8_copy(&q_z, &q->Z);
    fq51x8_normalize_weak(&p_x);
    fq51x8_normalize_weak(&p_y);
    fq51x8_normalize_weak(&p_z);
    fq51x8_normalize_weak(&q_x);
    fq51x8_normalize_weak(&q_y);
    fq51x8_normalize_weak(&q_z);

    fq51x8_mul(&t0, &p_x, &q_x);
    fq51x8_mul(&t1, &p_y, &q_y);
    fq51x8_mul(&t2, &p_z, &q_z);
    fq51x8_add(&t3, &p_x, &p_y);
    fq51x8_add(&t4, &q_x, &q_y);
    fq51x8_mul(&t3, &t3, &t4);
    fq51x8_add(&t4, &t0, &t1);
    fq51x8_sub(&t3, &t3, &t4);

    fq51x8_add(&t4, &p_x, &p_z);
    fq51x8_add(&t5, &q_x, &q_z);
    fq51x8_mul(&t4, &t4, &t5);
    fq51x8_add(&t5, &t0, &t2);
    fq51x8_sub(&t4, &t4, &t5);

    fq51x8_add(&t5, &p_y, &p_z);
    fq51x8_add(&X3, &q_y, &q_z);
    fq51x8_mul(&t5, &t5, &X3);
    fq51x8_add(&X3, &t1, &t2);
    fq51x8_sub(&t5, &t5, &X3);

    fq51x8_0(&Z3);
    fq51x8_sub(&Z3, &Z3, &t4);
    fq51x8_sub(&Z3, &Z3, &t4);
    fq51x8_sub(&Z3, &Z3, &t4);

    fq51x8_mul(&X3, &B3, &t2);
    fq51x8_add(&Z3, &X3, &Z3);
    fq51x8_sub(&X3, &t1, &Z3);
    fq51x8_add(&Z3, &t1, &Z3);
    fq51x8_normalize_weak(&Z3);
    fq51x8_mul(&Y3, &X3, &Z3);

    fq51x8_add(&t1, &t0, &t0);
    fq51x8_add(&t1, &t1, &t0);

    {
        fq51x8 neg_t2;
        fq51x8_0(&neg_t2);
        fq51x8_sub(&neg_t2, &neg_t2, &t2);
        fq51x8_sub(&neg_t2, &neg_t2, &t2);
        fq51x8_sub(&neg_t2, &neg_t2, &t2);
        fq51x8_copy(&t2, &neg_t2);
    }

    fq51x8_mul(&t4, &B3, &t4);
    fq51x8_add(&t1, &t1, &t2);
    fq51x8_normalize_weak(&t1);
    fq51x8_sub(&t2, &t0, &t2);

    {
        fq51x8 neg_t2;
        fq51x8_0(&neg_t2);
        fq51x8_sub(&neg_t2, &neg_t2, &t2);
        fq51x8_sub(&neg_t2, &neg_t2, &t2);
        fq51x8_sub(&neg_t2, &neg_t2, &t2);
        fq51x8_copy(&t2, &neg_t2);
    }

    fq51x8_add(&t4, &t4, &t2);

    fq51x8_mul(&t0, &t1, &t4);
    fq51x8_add(&Y3, &Y3, &t0);
    fq51x8_mul(&t0, &t5, &t4);
    fq51x8_mul(&X3, &t3, &X3);
    fq51x8_sub(&X3, &X3, &t0);
    fq51x8_mul(&t0, &t3, &t1);
    fq51x8_mul(&Z3, &t5, &Z3);
    fq51x8_add(&Z3, &Z3, &t0);

    /* See ran_complete_add_ifma.cpp for the rationale: normalize Y3 and Z3
     * so that chaining this output into another complete-add call doesn't
     * push IFMA inputs past the silent-52-bit mask. */
    fq51x8_normalize_weak(&Y3);
    fq51x8_normalize_weak(&Z3);

    fq51x8_copy(&r->X, &X3);
    fq51x8_copy(&r->Y, &Y3);
    fq51x8_copy(&r->Z, &Z3);
}
