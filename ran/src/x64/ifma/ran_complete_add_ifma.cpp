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

#include "x64/ifma/ran_complete_add_ifma.h"

#include "ran_constants.h"

/*
 * 8-way parallel Renes-Costello-Batina 2016 Algorithm 4; mirrors the scalar
 * body in ran/src/ran_complete_add.cpp using fp51x8 SIMD primitives.
 *
 * Bound tracking: fp51x8_mul / fp51x8_sq / fp51x8_sub all produce <=51-bit
 * canonical limbs. fp51x8_add does NOT carry; its output is <=52 bits from a
 * single add of <=51-bit inputs, and ~53 bits from two chained adds. IFMA
 * vpmadd52 silently masks inputs to 52 bits, so any accumulator that has
 * taken 2+ chained adds before feeding a mul needs fp51x8_normalize_weak
 * first. The three `-3*x` sequences use chained fp51x8_sub from zero (each
 * intermediate is canonical), mirroring the scalar correctness trick.
 */
void ran_complete_add_ifma_8x(ran_projective_8x *r, const ran_projective_8x *p, const ran_projective_8x *q)
{
    fp51x8 t0, t1, t2, t3, t4, t5, X3, Y3, Z3;
    fp51x8 B3;
    fp51x8_broadcast_fe(&B3, RAN_B3);

    /* Defensive input normalization. Callers include scalar projective values
     * packed via fp51x8_insert_lane: scalar fp_fe outputs from fp_mul /
     * fp_sq / fp_sub are guaranteed <=51 bits per limb, but scalar fp_add
     * outputs can be up to 52 bits (no carry). And intermediate projective
     * values coming back from a previous complete-add may carry 52-bit
     * limbs too. The first fp51x8_add below chains two such limbs to ~53
     * bits, which vpmadd52 silently truncates to 52 bits. Normalize first.
     *
     * Once the input bounds are guaranteed <=51, the rest of the body's
     * bound comments (<=52 after one add, etc.) hold and the chain is
     * IFMA-safe. */
    fp51x8 p_x, p_y, p_z, q_x, q_y, q_z;
    fp51x8_copy(&p_x, &p->X);
    fp51x8_copy(&p_y, &p->Y);
    fp51x8_copy(&p_z, &p->Z);
    fp51x8_copy(&q_x, &q->X);
    fp51x8_copy(&q_y, &q->Y);
    fp51x8_copy(&q_z, &q->Z);
    fp51x8_normalize_weak(&p_x);
    fp51x8_normalize_weak(&p_y);
    fp51x8_normalize_weak(&p_z);
    fp51x8_normalize_weak(&q_x);
    fp51x8_normalize_weak(&q_y);
    fp51x8_normalize_weak(&q_z);

    fp51x8_mul(&t0, &p_x, &q_x); /* M1 */
    fp51x8_mul(&t1, &p_y, &q_y); /* M2 */
    fp51x8_mul(&t2, &p_z, &q_z); /* M3 */
    fp51x8_add(&t3, &p_x, &p_y); /* <=52 */
    fp51x8_add(&t4, &q_x, &q_y); /* <=52 */
    fp51x8_mul(&t3, &t3, &t4); /* M4 (<=52 inputs OK) */
    fp51x8_add(&t4, &t0, &t1); /* <=52 */
    fp51x8_sub(&t3, &t3, &t4); /* <=51 */

    fp51x8_add(&t4, &p_x, &p_z); /* <=52 */
    fp51x8_add(&t5, &q_x, &q_z); /* <=52 */
    fp51x8_mul(&t4, &t4, &t5); /* M5 */
    fp51x8_add(&t5, &t0, &t2); /* <=52 */
    fp51x8_sub(&t4, &t4, &t5); /* <=51 */

    fp51x8_add(&t5, &p_y, &p_z); /* <=52 */
    fp51x8_add(&X3, &q_y, &q_z); /* <=52 */
    fp51x8_mul(&t5, &t5, &X3); /* M6 */
    fp51x8_add(&X3, &t1, &t2); /* <=52 */
    fp51x8_sub(&t5, &t5, &X3); /* <=51 */

    /* Z3 = -3 * t4 (three subs from zero; each intermediate canonical). */
    fp51x8_0(&Z3);
    fp51x8_sub(&Z3, &Z3, &t4);
    fp51x8_sub(&Z3, &Z3, &t4);
    fp51x8_sub(&Z3, &Z3, &t4); /* Z3 <=51 */

    fp51x8_mul(&X3, &B3, &t2); /* Mb1: b3*t2, <=51 */
    fp51x8_add(&Z3, &X3, &Z3); /* <=52 */
    fp51x8_sub(&X3, &t1, &Z3); /* <=51 */
    fp51x8_add(&Z3, &t1, &Z3); /* <=53 (two add tiers feeding Z3) */
    fp51x8_normalize_weak(&Z3); /* Z3 canonical for mul */
    fp51x8_mul(&Y3, &X3, &Z3); /* M7 */

    fp51x8_add(&t1, &t0, &t0); /* <=52 */
    fp51x8_add(&t1, &t1, &t0); /* <=53, chain feeds add later then mul; normalize before mul */

    /* t2 = -3 * t2 (three subs from zero, with local scratch to avoid aliasing). */
    {
        fp51x8 neg_t2;
        fp51x8_0(&neg_t2);
        fp51x8_sub(&neg_t2, &neg_t2, &t2);
        fp51x8_sub(&neg_t2, &neg_t2, &t2);
        fp51x8_sub(&neg_t2, &neg_t2, &t2);
        fp51x8_copy(&t2, &neg_t2); /* t2 <=51 */
    }

    fp51x8_mul(&t4, &B3, &t4); /* Mb2: b3*t4, <=51 */
    fp51x8_add(&t1, &t1, &t2); /* t1 was <=53, adding <=51 gives <=54; normalize before mul */
    fp51x8_normalize_weak(&t1); /* t1 canonical */
    fp51x8_sub(&t2, &t0, &t2); /* <=51 */

    /* t2 = -3 * t2 (second application). */
    {
        fp51x8 neg_t2;
        fp51x8_0(&neg_t2);
        fp51x8_sub(&neg_t2, &neg_t2, &t2);
        fp51x8_sub(&neg_t2, &neg_t2, &t2);
        fp51x8_sub(&neg_t2, &neg_t2, &t2);
        fp51x8_copy(&t2, &neg_t2); /* t2 <=51 */
    }

    fp51x8_add(&t4, &t4, &t2); /* <=52 */

    fp51x8_mul(&t0, &t1, &t4); /* M8 (<=52 inputs OK) */
    fp51x8_add(&Y3, &Y3, &t0); /* <=52 */
    fp51x8_mul(&t0, &t5, &t4); /* M9 */
    fp51x8_mul(&X3, &t3, &X3); /* M10 */
    fp51x8_sub(&X3, &X3, &t0); /* <=51 */
    fp51x8_mul(&t0, &t3, &t1); /* M11 */
    fp51x8_mul(&Z3, &t5, &Z3); /* M12 */
    fp51x8_add(&Z3, &Z3, &t0); /* <=52 */

    /* Y3 and Z3 are up to 52 bits from the final fp51x8_add. Normalize before
     * writing the result so the output has canonical <=51-bit limbs. Required
     * because the next caller may feed the result back as p or q, and the
     * early fp51x8_add(t3, p.X, p.Y) chains two <=51 inputs to <=52, then the
     * subsequent fp51x8_mul would see 52-bit operands (fine) — but if THIS
     * output were <=52 already, chaining to a <=52 plus another <=52 gives
     * <=53 which vpmadd52 silently masks. */
    fp51x8_normalize_weak(&Y3);
    fp51x8_normalize_weak(&Z3);

    fp51x8_copy(&r->X, &X3);
    fp51x8_copy(&r->Y, &Y3);
    fp51x8_copy(&r->Z, &Z3);
}
