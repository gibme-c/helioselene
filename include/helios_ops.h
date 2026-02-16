#ifndef HELIOSELENE_HELIOS_OPS_H
#define HELIOSELENE_HELIOS_OPS_H

#include "helios.h"
#include "fp_ops.h"
#include "fp_cmov.h"
#include "fp_utils.h"
#include "fp_invert.h"
#include "fp_mul.h"
#include "fp_sq.h"

/* Set r to the identity (point at infinity): (1:1:0) */
static inline void helios_identity(helios_jacobian *r)
{
    fp_1(r->X);
    fp_1(r->Y);
    fp_0(r->Z);
}

/* Copy p to r */
static inline void helios_copy(helios_jacobian *r, const helios_jacobian *p)
{
    fp_copy(r->X, p->X);
    fp_copy(r->Y, p->Y);
    fp_copy(r->Z, p->Z);
}

/* Check if p is the identity (Z == 0) */
static inline int helios_is_identity(const helios_jacobian *p)
{
    return !fp_isnonzero(p->Z);
}

/* Negate: (X:Y:Z) -> (X:-Y:Z) */
static inline void helios_neg(helios_jacobian *r, const helios_jacobian *p)
{
    fp_copy(r->X, p->X);
    fp_neg(r->Y, p->Y);
    fp_copy(r->Z, p->Z);
}

/* Constant-time conditional move: r = b ? p : r */
static inline void helios_cmov(helios_jacobian *r, const helios_jacobian *p, unsigned int b)
{
    fp_cmov(r->X, p->X, b);
    fp_cmov(r->Y, p->Y, b);
    fp_cmov(r->Z, p->Z, b);
}

/* Constant-time conditional move for affine points */
static inline void helios_affine_cmov(helios_affine *r, const helios_affine *p, unsigned int b)
{
    fp_cmov(r->x, p->x, b);
    fp_cmov(r->y, p->y, b);
}

/* Constant-time conditional negate: if b, negate Y in place */
static inline void helios_cneg(helios_jacobian *r, unsigned int b)
{
    fp_fe neg_y;
    fp_neg(neg_y, r->Y);
    fp_cmov(r->Y, neg_y, b);
}

/* Constant-time conditional negate for affine: if b, negate y in place */
static inline void helios_affine_cneg(helios_affine *r, unsigned int b)
{
    fp_fe neg_y;
    fp_neg(neg_y, r->y);
    fp_cmov(r->y, neg_y, b);
}

/* Convert Jacobian to affine: x = X/Z^2, y = Y/Z^3 */
static inline void helios_to_affine(helios_affine *r, const helios_jacobian *p)
{
    fp_fe z_inv, z_inv2, z_inv3;
    fp_invert(z_inv, p->Z);
    fp_sq(z_inv2, z_inv);
    fp_mul(z_inv3, z_inv2, z_inv);
    fp_mul(r->x, p->X, z_inv2);
    fp_mul(r->y, p->Y, z_inv3);
}

/* Convert affine to Jacobian: (x, y) -> (x:y:1) */
static inline void helios_from_affine(helios_jacobian *r, const helios_affine *p)
{
    fp_copy(r->X, p->x);
    fp_copy(r->Y, p->y);
    fp_1(r->Z);
}

#endif // HELIOSELENE_HELIOS_OPS_H
