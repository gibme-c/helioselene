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

#include "ran_complete_add.h"

#include "ran_constants.h"

/*
 * Renes-Costello-Batina 2016 Algorithm 4 (complete projective addition for a = -3).
 * Paper: https://eprint.iacr.org/2015/1060 §A.2. Listing cross-checked against
 * the general add-2015-rcb formula on the Explicit-Formulas Database
 * (hyperelliptic.org/EFD/g1p/auto-shortw-projective-3.html) with a = -3 inlined.
 *
 * Cost per call: 12 variable fp_mul + 2 muls by RAN_B3 + 29 fp_add/sub/neg.
 * Branchless: no conditional control flow, no data-dependent memory access.
 *
 * Correct for every input pair including P = identity, Q = identity, P = Q,
 * P = -Q. No edge-case branches.
 *
 * Aliasing: r may alias p or q; intermediate results live in locals until the
 * final copy-out, so caller-visible state is only written at the end.
 */
void ran_complete_add(ran_projective *r, const ran_projective *p, const ran_projective *q)
{
    fp_fe t0, t1, t2, t3, t4, t5, X3, Y3, Z3;

    fp_mul(t0, p->X, q->X); /* M1:  t0 = X1 * X2                             */
    fp_mul(t1, p->Y, q->Y); /* M2:  t1 = Y1 * Y2                             */
    fp_mul(t2, p->Z, q->Z); /* M3:  t2 = Z1 * Z2                             */
    fp_add(t3, p->X, p->Y); /*      t3 = X1 + Y1                             */
    fp_add(t4, q->X, q->Y); /*      t4 = X2 + Y2                             */
    fp_mul(t3, t3, t4); /* M4:  t3 = (X1+Y1)(X2+Y2)                      */
    fp_add(t4, t0, t1); /*      t4 = t0 + t1                             */
    fp_sub(t3, t3, t4); /*      t3 = X1*Y2 + X2*Y1                       */

    fp_add(t4, p->X, p->Z); /*      t4 = X1 + Z1                             */
    fp_add(t5, q->X, q->Z); /*      t5 = X2 + Z2                             */
    fp_mul(t4, t4, t5); /* M5:  t4 = (X1+Z1)(X2+Z2)                      */
    fp_add(t5, t0, t2); /*      t5 = t0 + t2                             */
    fp_sub(t4, t4, t5); /*      t4 = X1*Z2 + X2*Z1                       */

    fp_add(t5, p->Y, p->Z); /*      t5 = Y1 + Z1                             */
    fp_add(X3, q->Y, q->Z); /*      X3 = Y2 + Z2                             */
    fp_mul(t5, t5, X3); /* M6:  t5 = (Y1+Z1)(Y2+Z2)                      */
    fp_add(X3, t1, t2); /*      X3 = t1 + t2                             */
    fp_sub(t5, t5, X3); /*      t5 = Y1*Z2 + Y2*Z1                       */

    /* a = -3 specialization: Z3 = -3 * t4.
     * Computed as three chained fp_sub from zero; each intermediate is
     * canonical-reduced by fp_sub. fp_neg would underflow its narrow bias
     * if fed an unreduced (3 * t4) accumulator. */
    fp_0(Z3);
    fp_sub(Z3, Z3, t4); /*      Z3 = -t4                                 */
    fp_sub(Z3, Z3, t4); /*      Z3 = -2*t4                               */
    fp_sub(Z3, Z3, t4); /*      Z3 = -3*t4                               */

    fp_mul(X3, RAN_B3, t2); /* Mb1: X3 = b3 * t2                             */
    fp_add(Z3, X3, Z3); /*      Z3 = b3*t2 - 3*t4                        */
    fp_sub(X3, t1, Z3); /*      X3 = t1 - Z3                             */
    fp_add(Z3, t1, Z3); /*      Z3 = t1 + Z3                             */
    fp_mul(Y3, X3, Z3); /* M7:  Y3 = X3 * Z3                             */

    fp_add(t1, t0, t0); /*      t1 = 2 * t0                              */
    fp_add(t1, t1, t0); /*      t1 = 3 * t0                              */

    /* a = -3 specialization: t2 = -3 * t2 (in place, canonical per step) */
    {
        fp_fe neg_t2;
        fp_0(neg_t2);
        fp_sub(neg_t2, neg_t2, t2);
        fp_sub(neg_t2, neg_t2, t2);
        fp_sub(neg_t2, neg_t2, t2);
        fp_copy(t2, neg_t2);
    }

    fp_mul(t4, RAN_B3, t4); /* Mb2: t4 = b3 * t4                             */
    fp_add(t1, t1, t2); /*      t1 = 3*t0 - 3*t2_orig                    */
    fp_sub(t2, t0, t2); /*      t2 = t0 - (-3*t2_orig)                   */

    /* a = -3 specialization: t2 = -3 * t2 */
    {
        fp_fe neg_t2;
        fp_0(neg_t2);
        fp_sub(neg_t2, neg_t2, t2);
        fp_sub(neg_t2, neg_t2, t2);
        fp_sub(neg_t2, neg_t2, t2);
        fp_copy(t2, neg_t2);
    }

    fp_add(t4, t4, t2); /*      t4 = b3*t4_orig + t2                     */

    fp_mul(t0, t1, t4); /* M8:  t0 = t1 * t4                             */
    fp_add(Y3, Y3, t0); /*      Y3 = Y3 + t0                             */
    fp_mul(t0, t5, t4); /* M9:  t0 = t5 * t4                             */
    fp_mul(X3, t3, X3); /* M10: X3 = t3 * X3                             */
    fp_sub(X3, X3, t0); /*      X3 = X3 - t0                             */
    fp_mul(t0, t3, t1); /* M11: t0 = t3 * t1                             */
    fp_mul(Z3, t5, Z3); /* M12: Z3 = t5 * Z3                             */
    fp_add(Z3, Z3, t0); /*      Z3 = Z3 + t0                             */

    fp_copy(r->X, X3);
    fp_copy(r->Y, Y3);
    fp_copy(r->Z, Z3);
}
