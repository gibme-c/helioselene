#ifndef HELIOSELENE_HELIOS_VALIDATE_H
#define HELIOSELENE_HELIOS_VALIDATE_H

#include "helios.h"
#include "helios_constants.h"
#include "fp_ops.h"
#include "fp_mul.h"
#include "fp_sq.h"
#include "fp_tobytes.h"

/*
 * Check if an affine point is on the Helios curve: y^2 = x^3 - 3x + b (mod p).
 * Variable-time (validation-only, not secret-dependent).
 * Returns 1 if on curve, 0 if not.
 */
static inline int helios_is_on_curve(const helios_affine *p)
{
    fp_fe x2, x3, rhs, lhs, diff;

    /* lhs = y^2 */
    fp_sq(lhs, p->y);

    /* rhs = x^3 - 3x + b */
    fp_sq(x2, p->x);
    fp_mul(x3, x2, p->x);

    /* t = 3*x */
    fp_fe three_x;
    fp_add(three_x, p->x, p->x);
    fp_add(three_x, three_x, p->x);

    fp_sub(rhs, x3, three_x);
    fp_add(rhs, rhs, HELIOS_B);

    /* Check lhs == rhs */
    fp_sub(diff, lhs, rhs);
    unsigned char bytes[32];
    fp_tobytes(bytes, diff);

    unsigned char d = 0;
    for (int i = 0; i < 32; i++)
        d |= bytes[i];

    return d == 0;
}

#endif // HELIOSELENE_HELIOS_VALIDATE_H
