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

#include "x64/fq_is_qr.h"

#include "fq_tobytes.h"
#include "ranshaw_secure_erase.h"
#include "x64/fq_divsteps.h"

/*
 * Compute Legendre symbol (z / q) via Bernstein-Yang positive-divsteps with
 * Jacobi-bit tracking. See fq_posdivsteps_62_jacobi in fq_divsteps.h for the
 * inner-loop rationale.
 *
 * Starting from (f = q, g = z mod q), run 12 × 62 = 744 positive divsteps.
 * For z coprime to q (i.e., z != 0 mod q), the iteration converges with
 * f = gcd(q, z) = 1 and g = 0, at which point the accumulated jac bit equals
 * the parity of (z / q):
 *   jac & 1 == 0  →  z is a QR   → return 1
 *   jac & 1 == 1  →  z is a NR   → return 0
 *
 * By convention, z = 0 is treated as a QR (sqrt(0) = 0 is a valid root);
 * the posdivsteps iteration on g=0 degenerates (never swaps), so we compute
 * is_zero explicitly and OR it into the result. We still run all 744 steps
 * to keep timing identical with nonzero inputs.
 */
int fq_is_qr_x64(const fq_fe z)
{
    /* Zero detection via canonicalized byte serialization. */
    unsigned char zb[32];
    fq_tobytes(zb, z);
    uint32_t nz = 0;
    for (int i = 0; i < 32; i++)
        nz |= zb[i];
    /* is_zero = 1 if all bytes zero, else 0. */
    unsigned int is_zero = ((uint32_t)(nz - 1u)) >> 31;

    /* f = q (modulus in signed62), g = z canonicalized */
    fq_signed62 f, g;
    f = FQ_MODULUS_S62;
    fq_fe_to_signed62(&g, z);

    int64_t delta = 1;
    uint32_t jac = 0;

    /* 25 outer iterations × 62 posdivsteps = 1550 total. Positive-divsteps
     * has a different (and larger) convergence bound than standard BY divsteps
     * — secp256k1 uses JACOBI64_ITERATIONS=25 for 256-bit moduli with the
     * analogous routine. 1550 steps gives safety margin for 255-bit F_q.
     *
     * Pass 64-bit low words of f and g (not just 62-bit limb 0) so Jacobi-bit
     * tracking has enough precision to survive 62 halvings of g_sim. See
     * fq_posdivsteps_62_jacobi for the precision analysis. Limbs v[0] are
     * 62-bit values; we pull bits 62..63 from the low 2 bits of limb v[1].
     * Since f, g stay non-negative in pos-divsteps, the casts below preserve
     * the intended low-64-bit interpretation without sign-extension noise. */
    for (int i = 0; i < 25; i++)
    {
        fq_trans2x2 t;
        uint64_t f64 = (uint64_t)f.v[0] | ((uint64_t)f.v[1] << 62);
        uint64_t g64 = (uint64_t)g.v[0] | ((uint64_t)g.v[1] << 62);
        delta = fq_posdivsteps_62_jacobi(delta, f64, g64, &t, &jac);
        fq_update_fg(&f, &g, &t);
    }

    /* Final parity: jac bit 0 = 0 → QR, 1 → NR. Override to 1 for z = 0. */
    int is_qr = (int)(1u - (jac & 1u));
    is_qr |= (int)is_zero;

    ranshaw_secure_erase(&f, sizeof(f));
    ranshaw_secure_erase(&g, sizeof(g));
    ranshaw_secure_erase(zb, sizeof(zb));
    return is_qr;
}
