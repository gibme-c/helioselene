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
 * @file selene_add.h
 * @brief Selene Jacobian point addition with edge-case handling (identity, doubling, inverse).
 */

#ifndef HELIOSELENE_SELENE_ADD_H
#define HELIOSELENE_SELENE_ADD_H

#include "selene_dbl.h"
#include "selene_ops.h"

#if HELIOSELENE_PLATFORM_64BIT
void selene_add_x64(selene_jacobian *r, const selene_jacobian *p, const selene_jacobian *q);
#else
void selene_add_portable(selene_jacobian *r, const selene_jacobian *p, const selene_jacobian *q);
#endif

static inline void selene_add(selene_jacobian *r, const selene_jacobian *p, const selene_jacobian *q)
{
    /* Identity inputs */
    if (selene_is_identity(p))
    {
        selene_copy(r, q);
        return;
    }
    if (selene_is_identity(q))
    {
        selene_copy(r, p);
        return;
    }

    /* Projective x-coordinate comparison: U1 = X1*Z2^2, U2 = X2*Z1^2 */
    fq_fe z1z1, z2z2, u1, u2, diff;
    fq_sq(z1z1, p->Z);
    fq_sq(z2z2, q->Z);
    fq_mul(u1, p->X, z2z2);
    fq_mul(u2, q->X, z1z1);
    fq_sub(diff, u1, u2);

    if (!fq_isnonzero(diff))
    {
        /* Same x: check y parity via S1 = Y1*Z2^3, S2 = Y2*Z1^3 */
        fq_fe s1, s2, t;
        fq_mul(t, q->Z, z2z2);
        fq_mul(s1, p->Y, t);
        fq_mul(t, p->Z, z1z1);
        fq_mul(s2, q->Y, t);
        fq_sub(diff, s1, s2);
        if (!fq_isnonzero(diff))
            selene_dbl(r, p);
        else
            selene_identity(r);
        return;
    }

#if HELIOSELENE_PLATFORM_64BIT
    selene_add_x64(r, p, q);
#else
    selene_add_portable(r, p, q);
#endif
}

#endif // HELIOSELENE_SELENE_ADD_H
