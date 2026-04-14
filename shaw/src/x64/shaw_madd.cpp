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

#include "x64/shaw_madd.h"

#include "fq_ops.h"
#include "x64/fq51_chain.h"
#include "x64/shaw_inner_packed.h"

/*
 * Mixed addition: Jacobian + Affine -> Jacobian (over F_q)
 * Same formula as ran_madd but over F_q.
 * Cost: 7M + 4S
 */

#if defined(FQ51_HAVE_ADX_MUL)

/*
 * 4×64 packed mixed addition (7M + 4S). Accepts packed inputs/outputs; aliasing
 * between pX/pY/pZ and rX/rY/rZ element-wise is allowed (same contract as
 * shaw_dbl_x64_packed / shaw_add_x64's pack-once path).
 */
void shaw_madd_x64_packed(
    uint64_t rX[4],
    uint64_t rY[4],
    uint64_t rZ[4],
    const uint64_t pX[4],
    const uint64_t pY[4],
    const uint64_t pZ[4],
    const uint64_t qx[4],
    const uint64_t qy[4])
{
    uint64_t Z1Z1[4], U2[4], S2[4], H[4], HH[4], I[4], J[4], rr[4], V[4];
    uint64_t t0[4], t1[4];

    fq64_sq(Z1Z1, pZ);

    fq64_mul(U2, qx, Z1Z1);

    fq64_mul(t0, pZ, Z1Z1);
    fq64_mul(S2, qy, t0);

    fq64_sub(H, U2, pX);

    fq64_sq(HH, H);

    fq64_add(I, HH, HH);
    fq64_add(I, I, I);

    fq64_mul(J, H, I);

    fq64_sub(rr, S2, pY);
    fq64_add(rr, rr, rr);

    fq64_mul(V, pX, I);

    /* Z3 = (Z1 + H)^2 - Z1Z1 - HH (destroys pZ via aliasing — already consumed) */
    fq64_add(t0, pZ, H);
    fq64_sq(t1, t0);
    fq64_sub(t1, t1, Z1Z1);
    fq64_sub(rZ, t1, HH);

    /* Y3 = rr * (V - X3) - 2*pY*J  — compute pY*J before overwriting rY via aliasing */
    uint64_t pYJ[4];
    fq64_mul(pYJ, pY, J);
    fq64_add(pYJ, pYJ, pYJ);

    /* X3 = rr^2 - J - 2*V (destroys pX via aliasing — already consumed) */
    fq64_sq(rX, rr);
    fq64_sub(rX, rX, J);
    fq64_add(t0, V, V);
    fq64_sub(rX, rX, t0);

    fq64_sub(t0, V, rX);
    fq64_mul(t1, rr, t0);
    fq64_sub(rY, t1, pYJ);
}

void shaw_madd_x64(shaw_jacobian *r, const shaw_jacobian *p, const shaw_affine *q)
{
    uint64_t pX[4], pY[4], pZ[4], qx[4], qy[4];
    fq51_normalize_and_pack(pX, p->X);
    fq51_normalize_and_pack(pY, p->Y);
    fq51_normalize_and_pack(pZ, p->Z);
    fq51_normalize_and_pack(qx, q->x);
    fq51_normalize_and_pack(qy, q->y);

    uint64_t rX[4], rY[4], rZ[4];
    shaw_madd_x64_packed(rX, rY, rZ, pX, pY, pZ, qx, qy);

    /* Native 4x64: unpack is a canonical reduce into the fq_fe coordinates. */
    unpack_and_normalize(r->X, rX);
    unpack_and_normalize(r->Y, rY);
    unpack_and_normalize(r->Z, rZ);
}

#else

void shaw_madd_x64(shaw_jacobian *r, const shaw_jacobian *p, const shaw_affine *q)
{
    fq_fe Z1Z1, U2, S2, H, HH, I, J, rr, V;
    fq_fe t0, t1;

    fq51_chain_sq(Z1Z1, p->Z);

    fq51_chain_mul(U2, q->x, Z1Z1);

    fq51_chain_mul(t0, p->Z, Z1Z1);
    fq51_chain_mul(S2, q->y, t0);

    fq_sub(H, U2, p->X);

    fq51_chain_sq(HH, H);

    fq_add(I, HH, HH);
    fq_add(I, I, I);

    fq51_chain_mul(J, H, I);

    fq_sub(rr, S2, p->Y);
    fq_add(rr, rr, rr);

    fq51_chain_mul(V, p->X, I);

    fq51_chain_sq(r->X, rr);
    fq_sub(r->X, r->X, J);
    fq_add(t0, V, V);
    fq_sub(r->X, r->X, t0);

    fq_sub(t0, V, r->X);
    fq51_chain_mul(t1, rr, t0);
    fq51_chain_mul(t0, p->Y, J);
    fq_add(t0, t0, t0);
    fq_sub(r->Y, t1, t0);

    fq_add(t0, p->Z, H);
    fq51_chain_sq(t1, t0);
    fq_sub(t1, t1, Z1Z1);
    fq_sub(r->Z, t1, HH);
}

#endif /* FQ51_HAVE_ADX_MUL */
