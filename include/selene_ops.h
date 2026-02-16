#ifndef HELIOSELENE_SELENE_OPS_H
#define HELIOSELENE_SELENE_OPS_H

#include "selene.h"
#include "fq_ops.h"
#include "fq_cmov.h"
#include "fq_utils.h"
#include "fq_invert.h"
#include "fq_mul.h"
#include "fq_sq.h"

/* Set r to the identity (point at infinity): (1:1:0) */
static inline void selene_identity(selene_jacobian *r)
{
    fq_1(r->X);
    fq_1(r->Y);
    fq_0(r->Z);
}

/* Copy p to r */
static inline void selene_copy(selene_jacobian *r, const selene_jacobian *p)
{
    fq_copy(r->X, p->X);
    fq_copy(r->Y, p->Y);
    fq_copy(r->Z, p->Z);
}

/* Check if p is the identity (Z == 0) */
static inline int selene_is_identity(const selene_jacobian *p)
{
    return !fq_isnonzero(p->Z);
}

/* Negate: (X:Y:Z) -> (X:-Y:Z) */
static inline void selene_neg(selene_jacobian *r, const selene_jacobian *p)
{
    fq_copy(r->X, p->X);
    fq_neg(r->Y, p->Y);
    fq_copy(r->Z, p->Z);
}

/* Constant-time conditional move: r = b ? p : r */
static inline void selene_cmov(selene_jacobian *r, const selene_jacobian *p, unsigned int b)
{
    fq_cmov(r->X, p->X, b);
    fq_cmov(r->Y, p->Y, b);
    fq_cmov(r->Z, p->Z, b);
}

/* Constant-time conditional move for affine points */
static inline void selene_affine_cmov(selene_affine *r, const selene_affine *p, unsigned int b)
{
    fq_cmov(r->x, p->x, b);
    fq_cmov(r->y, p->y, b);
}

/* Constant-time conditional negate: if b, negate Y in place */
static inline void selene_cneg(selene_jacobian *r, unsigned int b)
{
    fq_fe neg_y;
    fq_neg(neg_y, r->Y);
    fq_cmov(r->Y, neg_y, b);
}

/* Constant-time conditional negate for affine: if b, negate y in place */
static inline void selene_affine_cneg(selene_affine *r, unsigned int b)
{
    fq_fe neg_y;
    fq_neg(neg_y, r->y);
    fq_cmov(r->y, neg_y, b);
}

/* Convert Jacobian to affine: x = X/Z^2, y = Y/Z^3 */
static inline void selene_to_affine(selene_affine *r, const selene_jacobian *p)
{
    fq_fe z_inv, z_inv2, z_inv3;
    fq_invert(z_inv, p->Z);
    fq_sq(z_inv2, z_inv);
    fq_mul(z_inv3, z_inv2, z_inv);
    fq_mul(r->x, p->X, z_inv2);
    fq_mul(r->y, p->Y, z_inv3);
}

/* Convert affine to Jacobian: (x, y) -> (x:y:1) */
static inline void selene_from_affine(selene_jacobian *r, const selene_affine *p)
{
    fq_copy(r->X, p->x);
    fq_copy(r->Y, p->y);
    fq_1(r->Z);
}

#endif // HELIOSELENE_SELENE_OPS_H
