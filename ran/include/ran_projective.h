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
 * @file ran_projective.h
 * @brief Homogeneous projective coordinates for the Ran curve, used only by the
 *        constant-time complete-addition driver (Renes-Costello-Batina 2016).
 *
 * The rest of the library stores Ran points in Jacobian coordinates (X:Y:Z) with
 * affine (x, y) = (X/Z^2, Y/Z^3) and identity at Z=0. Projective coordinates use
 * affine (x, y) = (X/Z, Y/Z) with identity at Z=0. Converters between the two
 * each cost 2M + 1S.
 */

#ifndef RANSHAW_RAN_PROJECTIVE_H
#define RANSHAW_RAN_PROJECTIVE_H

#include "fp_cmov.h"
#include "fp_mul.h"
#include "fp_ops.h"
#include "fp_sq.h"
#include "fp_utils.h"
#include "ran.h"

typedef struct
{
    fp_fe X;
    fp_fe Y;
    fp_fe Z;
} ran_projective;

/* Set r to the projective identity (0:1:0) */
static inline void ran_proj_identity(ran_projective *r)
{
    fp_0(r->X);
    fp_1(r->Y);
    fp_0(r->Z);
}

/* Check if p is the projective identity (Z == 0) */
static inline int ran_proj_is_identity(const ran_projective *p)
{
    return !fp_isnonzero(p->Z);
}

/* Constant-time conditional move: r = b ? p : r */
static inline void ran_proj_cmov(ran_projective *r, const ran_projective *p, unsigned int b)
{
    fp_cmov(r->X, p->X, b);
    fp_cmov(r->Y, p->Y, b);
    fp_cmov(r->Z, p->Z, b);
}

/* Constant-time conditional negate: if b, negate Y in place */
static inline void ran_proj_cneg(ran_projective *r, unsigned int b)
{
    fp_fe neg_y;
    fp_neg(neg_y, r->Y);
    fp_cmov(r->Y, neg_y, b);
}

/* Convert Jacobian (X_J:Y_J:Z_J) to projective (X_P:Y_P:Z_P).
 *
 *   affine_jac   = (X_J / Z_J^2, Y_J / Z_J^3)
 *   affine_proj  = (X_P / Z_P,   Y_P / Z_P)
 *
 * Setting Z_P = Z_J^3 gives X_P = X_J * Z_J, Y_P = Y_J. Cost: 2M + 1S.
 * Identity (Z_J = 0) maps to (0, Y_J, 0) which is a valid projective identity. */
static inline void ran_jac_to_proj(ran_projective *proj, const ran_jacobian *jac)
{
    fp_fe z2;
    fp_sq(z2, jac->Z);
    fp_mul(proj->X, jac->X, jac->Z);
    fp_copy(proj->Y, jac->Y);
    fp_mul(proj->Z, z2, jac->Z);
}

/* Convert projective (X_P:Y_P:Z_P) to Jacobian (X_J:Y_J:Z_J).
 *
 * Setting Z_J = Z_P gives X_J = X_P * Z_P, Y_J = Y_P * Z_P^2. Cost: 2M + 1S.
 * Identity (Z_P = 0) maps to (0, 0, 0); still valid because ran_is_identity
 * tests Z == 0. Non-canonical vs ran_identity's (1, 1, 0) but behaviourally
 * equivalent for downstream Jacobian ops. */
static inline void ran_proj_to_jac(ran_jacobian *jac, const ran_projective *proj)
{
    fp_fe z2;
    fp_sq(z2, proj->Z);
    fp_mul(jac->X, proj->X, proj->Z);
    fp_mul(jac->Y, proj->Y, z2);
    fp_copy(jac->Z, proj->Z);
}

#endif // RANSHAW_RAN_PROJECTIVE_H
