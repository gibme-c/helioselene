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
 * @file shaw_projective.h
 * @brief Homogeneous projective coordinates for the Shaw curve, used only by
 *        the constant-time complete-addition driver (Renes-Costello-Batina 2016).
 *
 * Mirror of ran_projective.h; see that header for full commentary.
 */

#ifndef RANSHAW_SHAW_PROJECTIVE_H
#define RANSHAW_SHAW_PROJECTIVE_H

#include "fq_cmov.h"
#include "fq_mul.h"
#include "fq_ops.h"
#include "fq_sq.h"
#include "fq_utils.h"
#include "shaw.h"

typedef struct
{
    fq_fe X;
    fq_fe Y;
    fq_fe Z;
} shaw_projective;

/* Set r to the projective identity (0:1:0) */
static inline void shaw_proj_identity(shaw_projective *r)
{
    fq_0(r->X);
    fq_1(r->Y);
    fq_0(r->Z);
}

/* Check if p is the projective identity (Z == 0) */
static inline int shaw_proj_is_identity(const shaw_projective *p)
{
    return !fq_isnonzero(p->Z);
}

/* Constant-time conditional move: r = b ? p : r */
static inline void shaw_proj_cmov(shaw_projective *r, const shaw_projective *p, unsigned int b)
{
    fq_cmov(r->X, p->X, b);
    fq_cmov(r->Y, p->Y, b);
    fq_cmov(r->Z, p->Z, b);
}

/* Constant-time conditional negate: if b, negate Y in place */
static inline void shaw_proj_cneg(shaw_projective *r, unsigned int b)
{
    fq_fe neg_y;
    fq_neg(neg_y, r->Y);
    fq_cmov(r->Y, neg_y, b);
}

/* Convert Jacobian (X_J:Y_J:Z_J) to projective (X_P:Y_P:Z_P). Cost: 2M + 1S. */
static inline void shaw_jac_to_proj(shaw_projective *proj, const shaw_jacobian *jac)
{
    fq_fe z2;
    fq_sq(z2, jac->Z);
    fq_mul(proj->X, jac->X, jac->Z);
    fq_copy(proj->Y, jac->Y);
    fq_mul(proj->Z, z2, jac->Z);
}

/* Convert projective (X_P:Y_P:Z_P) to Jacobian (X_J:Y_J:Z_J). Cost: 2M + 1S. */
static inline void shaw_proj_to_jac(shaw_jacobian *jac, const shaw_projective *proj)
{
    fq_fe z2;
    fq_sq(z2, proj->Z);
    fq_mul(jac->X, proj->X, proj->Z);
    fq_mul(jac->Y, proj->Y, z2);
    fq_copy(jac->Z, proj->Z);
}

#endif // RANSHAW_SHAW_PROJECTIVE_H
