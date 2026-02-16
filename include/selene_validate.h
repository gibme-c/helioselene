#ifndef HELIOSELENE_SELENE_VALIDATE_H
#define HELIOSELENE_SELENE_VALIDATE_H

#include "selene.h"
#include "selene_constants.h"
#include "fq_ops.h"
#include "fq_mul.h"
#include "fq_sq.h"
#include "fq_tobytes.h"

/*
 * Check if an affine point is on the Selene curve: y^2 = x^3 - 3x + b (mod q).
 * Variable-time (validation-only, not secret-dependent).
 * Returns 1 if on curve, 0 if not.
 */
static inline int selene_is_on_curve(const selene_affine *p)
{
    fq_fe x2, x3, rhs, lhs, diff;

    /* lhs = y^2 */
    fq_sq(lhs, p->y);

    /* rhs = x^3 - 3x + b */
    fq_sq(x2, p->x);
    fq_mul(x3, x2, p->x);

    fq_fe three_x;
    fq_add(three_x, p->x, p->x);
    fq_add(three_x, three_x, p->x);

    fq_sub(rhs, x3, three_x);
    fq_add(rhs, rhs, SELENE_B);

    /* Check lhs == rhs */
    fq_sub(diff, lhs, rhs);
    unsigned char bytes[32];
    fq_tobytes(bytes, diff);

    unsigned char d = 0;
    for (int i = 0; i < 32; i++)
        d |= bytes[i];

    return d == 0;
}

#endif // HELIOSELENE_SELENE_VALIDATE_H
