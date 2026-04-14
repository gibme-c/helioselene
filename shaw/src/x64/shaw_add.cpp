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

#include "x64/shaw_add.h"

#include "fq_ops.h"
#include "fq_utils.h"
#include "x64/fq51_chain.h"
#include "x64/shaw_inner_packed.h"

/*
 * General addition: Jacobian + Jacobian -> Jacobian (over F_q)
 * EFD: add-2007-bl. Cost: 11M + 5S
 *
 * Raw incomplete formula — does not handle p == q, p == -q, or identity inputs.
 * Edge cases are handled by the inline wrapper in shaw_add.h.
 */

#if defined(FQ51_HAVE_ADX_MUL)

/*
 * 4×64 packed general addition (11M + 5S). Accepts inputs in packed form
 * and produces outputs in packed form. Callers that already hold packed
 * accumulators (scalarmult_vartime inner loop, msm_vartime bucket sums)
 * can chain ops without the pack/unpack tax.
 *
 * Aliasing: rX/rY/rZ may alias pX/pY/pZ and qX/qY/qZ element-wise; the
 * body writes to local scratch buffers and commits to the result arrays
 * only at the end, so any aliasing permutation is safe.
 */
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
    const uint64_t U2[4])
{
    uint64_t S1[4], S2[4];
    uint64_t H[4], I[4], J[4], rr[4], V[4];
    uint64_t t0[4], t1[4];
    uint64_t outX[4], outY[4], outZ[4];

    fq64_mul(t0, qZ, Z2Z2);
    fq64_mul(S1, pY, t0);
    fq64_mul(t0, pZ, Z1Z1);
    fq64_mul(S2, qY, t0);

    fq64_sub(H, U2, U1);

    fq64_add(t0, H, H);
    fq64_sq(I, t0);

    fq64_mul(J, H, I);

    fq64_sub(rr, S2, S1);
    fq64_add(rr, rr, rr);

    fq64_mul(V, U1, I);

    fq64_sq(outX, rr);
    fq64_sub(outX, outX, J);
    fq64_add(t0, V, V);
    fq64_sub(outX, outX, t0);

    fq64_sub(t0, V, outX);
    fq64_mul(t1, rr, t0);
    fq64_mul(t0, S1, J);
    fq64_add(t0, t0, t0);
    fq64_sub(outY, t1, t0);

    fq64_add(t0, pZ, qZ);
    fq64_sq(t1, t0);
    fq64_sub(t1, t1, Z1Z1);
    fq64_sub(t1, t1, Z2Z2);
    fq64_mul(outZ, t1, H);

    rX[0] = outX[0];
    rX[1] = outX[1];
    rX[2] = outX[2];
    rX[3] = outX[3];
    rY[0] = outY[0];
    rY[1] = outY[1];
    rY[2] = outY[2];
    rY[3] = outY[3];
    rZ[0] = outZ[0];
    rZ[1] = outZ[1];
    rZ[2] = outZ[2];
    rZ[3] = outZ[3];
}

void shaw_add_x64_packed(
    uint64_t rX[4],
    uint64_t rY[4],
    uint64_t rZ[4],
    const uint64_t pX[4],
    const uint64_t pY[4],
    const uint64_t pZ[4],
    const uint64_t qX[4],
    const uint64_t qY[4],
    const uint64_t qZ[4])
{
    uint64_t Z1Z1[4], Z2Z2[4], U1[4], U2[4];

    fq64_sq(Z1Z1, pZ);
    fq64_sq(Z2Z2, qZ);
    fq64_mul(U1, pX, Z2Z2);
    fq64_mul(U2, qX, Z1Z1);

    shaw_add_x64_packed_prefixed(rX, rY, rZ, pY, pZ, qY, qZ, Z1Z1, Z2Z2, U1, U2);
}

/*
 * Jacobian negation in 4×64 packed form: r = (pX, q - pY, pZ).
 * Uses fq64_sub with a zero constant to perform modular negation of Y
 * with the same carry semantics as fq64_add/fq64_sub elsewhere.
 */
void shaw_jac_neg_x64_packed(
    uint64_t rX[4],
    uint64_t rY[4],
    uint64_t rZ[4],
    const uint64_t pX[4],
    const uint64_t pY[4],
    const uint64_t pZ[4])
{
    static const uint64_t FQ64_ZERO[4] = {0, 0, 0, 0};

    /* Read X and Z into locals before any write so aliasing r == p is safe. */
    uint64_t x0 = pX[0], x1 = pX[1], x2 = pX[2], x3 = pX[3];
    uint64_t z0 = pZ[0], z1 = pZ[1], z2 = pZ[2], z3 = pZ[3];

    fq64_sub(rY, FQ64_ZERO, pY);

    rX[0] = x0;
    rX[1] = x1;
    rX[2] = x2;
    rX[3] = x3;
    rZ[0] = z0;
    rZ[1] = z1;
    rZ[2] = z2;
    rZ[3] = z3;
}

int shaw_add_safe_packed(packed_jac *r, const packed_jac *p, const packed_jac *q)
{
    /* Identity input handling: if either operand is identity, result is
     * the other operand (copy) or identity (both identity). Must come
     * before the diff computation because 0 * anything = 0 would make
     * every identity input look like an X-collision, and then the
     * doubling branch would call shaw_dbl_x64_packed on an identity
     * input which returns identity with return value 0, silently
     * setting a non-identity flag on an identity acc. */
    const int p_id = is_identity_packed(p);
    const int q_id = is_identity_packed(q);
    if (p_id)
    {
        if (q_id)
            return 1;
        copy_packed(r, q);
        return 0;
    }
    if (q_id)
    {
        copy_packed(r, p);
        return 0;
    }

    uint64_t z1z1[4], z2z2[4], u1[4], u2[4], diff4[4];
    fq_fe diff5;

    fq64_sq(z1z1, p->Z);
    fq64_sq(z2z2, q->Z);
    fq64_mul(u1, p->X, z2z2);
    fq64_mul(u2, q->X, z1z1);
    fq64_sub(diff4, u1, u2);
    unpack_and_normalize(diff5, diff4);

    if (fq_isnonzero(diff5))
    {
        /* Feed the precomputed prefix into shaw_add_x64_packed's prefixed
         * entry point, saving the 2S + 2M that would otherwise be
         * recomputed inside the full shaw_add_x64_packed. */
        shaw_add_x64_packed_prefixed(r->X, r->Y, r->Z, p->Y, p->Z, q->Y, q->Z, z1z1, z2z2, u1, u2);
        return 0;
    }

    uint64_t s1[4], s2[4], tmp[4];
    fq64_mul(tmp, q->Z, z2z2);
    fq64_mul(s1, p->Y, tmp);
    fq64_mul(tmp, p->Z, z1z1);
    fq64_mul(s2, q->Y, tmp);
    fq64_sub(diff4, s1, s2);
    unpack_and_normalize(diff5, diff4);

    if (fq_isnonzero(diff5))
    {
        return 1;
    }

    shaw_dbl_x64_packed(r->X, r->Y, r->Z, p->X, p->Y, p->Z);
    return 0;
}

void shaw_add_x64(shaw_jacobian *r, const shaw_jacobian *p, const shaw_jacobian *q)
{
    uint64_t pX[4], pY[4], pZ[4], qX[4], qY[4], qZ[4];
    fq51_normalize_and_pack(pX, p->X);
    fq51_normalize_and_pack(pY, p->Y);
    fq51_normalize_and_pack(pZ, p->Z);
    fq51_normalize_and_pack(qX, q->X);
    fq51_normalize_and_pack(qY, q->Y);
    fq51_normalize_and_pack(qZ, q->Z);

    uint64_t rX[4], rY[4], rZ[4];
    shaw_add_x64_packed(rX, rY, rZ, pX, pY, pZ, qX, qY, qZ);

    unpack_and_normalize(r->X, rX);
    unpack_and_normalize(r->Y, rY);
    unpack_and_normalize(r->Z, rZ);
}

#else

void shaw_add_x64(shaw_jacobian *r, const shaw_jacobian *p, const shaw_jacobian *q)
{
    fq_fe Z1Z1, Z2Z2, U1, U2, S1, S2, H, I, J, rr, V;
    fq_fe t0, t1;

    fq51_chain_sq(Z1Z1, p->Z);
    fq51_chain_sq(Z2Z2, q->Z);

    fq51_chain_mul(U1, p->X, Z2Z2);
    fq51_chain_mul(U2, q->X, Z1Z1);

    fq51_chain_mul(t0, q->Z, Z2Z2);
    fq51_chain_mul(S1, p->Y, t0);
    fq51_chain_mul(t0, p->Z, Z1Z1);
    fq51_chain_mul(S2, q->Y, t0);

    fq_sub(H, U2, U1);

    fq_add(t0, H, H);
    fq51_chain_sq(I, t0);

    fq51_chain_mul(J, H, I);

    fq_sub(rr, S2, S1);
    fq_add(rr, rr, rr);

    fq51_chain_mul(V, U1, I);

    fq51_chain_sq(r->X, rr);
    fq_sub(r->X, r->X, J);
    fq_add(t0, V, V);
    fq_sub(r->X, r->X, t0);

    fq_sub(t0, V, r->X);
    fq51_chain_mul(t1, rr, t0);
    fq51_chain_mul(t0, S1, J);
    fq_add(t0, t0, t0);
    fq_sub(r->Y, t1, t0);

    fq_add(t0, p->Z, q->Z);
    fq51_chain_sq(t1, t0);
    fq_sub(t1, t1, Z1Z1);
    fq_sub(t1, t1, Z2Z2);
    fq51_chain_mul(r->Z, t1, H);
}

#endif /* FQ51_HAVE_ADX_MUL */
