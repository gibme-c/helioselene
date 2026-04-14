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

#include "shaw_complete_add.h"

#include "shaw_constants.h"

/*
 * Shaw mirror of Renes-Costello-Batina 2016 Algorithm 4; see
 * ran/src/ran_complete_add.cpp for the full algorithm commentary.
 */
void shaw_complete_add(shaw_projective *r, const shaw_projective *p, const shaw_projective *q)
{
    fq_fe t0, t1, t2, t3, t4, t5, X3, Y3, Z3;

    fq_mul(t0, p->X, q->X);
    fq_mul(t1, p->Y, q->Y);
    fq_mul(t2, p->Z, q->Z);
    fq_add(t3, p->X, p->Y);
    fq_add(t4, q->X, q->Y);
    fq_mul(t3, t3, t4);
    fq_add(t4, t0, t1);
    fq_sub(t3, t3, t4);

    fq_add(t4, p->X, p->Z);
    fq_add(t5, q->X, q->Z);
    fq_mul(t4, t4, t5);
    fq_add(t5, t0, t2);
    fq_sub(t4, t4, t5);

    fq_add(t5, p->Y, p->Z);
    fq_add(X3, q->Y, q->Z);
    fq_mul(t5, t5, X3);
    fq_add(X3, t1, t2);
    fq_sub(t5, t5, X3);

    /* a = -3 specialization: Z3 = -3 * t4.
     * Three chained fq_sub keeps limbs canonical across each step so the
     * formula is valid on backends whose fq_neg has a narrower bias than
     * fq_sub. See the Ran mirror for the explanation. */
    fq_0(Z3);
    fq_sub(Z3, Z3, t4);
    fq_sub(Z3, Z3, t4);
    fq_sub(Z3, Z3, t4);

    fq_mul(X3, SHAW_B3, t2);
    fq_add(Z3, X3, Z3);
    fq_sub(X3, t1, Z3);
    fq_add(Z3, t1, Z3);
    fq_mul(Y3, X3, Z3);

    fq_add(t1, t0, t0);
    fq_add(t1, t1, t0);

    /* a = -3 specialization: t2 = -3 * t2 */
    {
        fq_fe neg_t2;
        fq_0(neg_t2);
        fq_sub(neg_t2, neg_t2, t2);
        fq_sub(neg_t2, neg_t2, t2);
        fq_sub(neg_t2, neg_t2, t2);
        fq_copy(t2, neg_t2);
    }

    fq_mul(t4, SHAW_B3, t4);
    fq_add(t1, t1, t2);
    fq_sub(t2, t0, t2);

    /* a = -3 specialization: t2 = -3 * t2 */
    {
        fq_fe neg_t2;
        fq_0(neg_t2);
        fq_sub(neg_t2, neg_t2, t2);
        fq_sub(neg_t2, neg_t2, t2);
        fq_sub(neg_t2, neg_t2, t2);
        fq_copy(t2, neg_t2);
    }

    fq_add(t4, t4, t2);

    fq_mul(t0, t1, t4);
    fq_add(Y3, Y3, t0);
    fq_mul(t0, t5, t4);
    fq_mul(X3, t3, X3);
    fq_sub(X3, X3, t0);
    fq_mul(t0, t3, t1);
    fq_mul(Z3, t5, Z3);
    fq_add(Z3, Z3, t0);

    fq_copy(r->X, X3);
    fq_copy(r->Y, Y3);
    fq_copy(r->Z, Z3);
}
