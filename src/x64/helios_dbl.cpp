#include "x64/helios_dbl.h"

#include "fp_ops.h"
#include "x64/fp51_chain.h"

/*
 * Jacobian point doubling with a = -3 optimization.
 * EFD: dbl-2001-b
 * Cost: 3M + 5S
 *
 * delta = Z1^2
 * gamma = Y1^2
 * beta = X1 * gamma
 * alpha = 3 * (X1 - delta) * (X1 + delta)    [a = -3 optimization]
 * X3 = alpha^2 - 8*beta
 * Z3 = (Y1 + Z1)^2 - gamma - delta
 * Y3 = alpha * (4*beta - X3) - 8*gamma^2
 */
void helios_dbl_x64(helios_jacobian *r, const helios_jacobian *p)
{
    fp_fe delta, gamma, beta, alpha;
    fp_fe t0, t1, t2;

    /* delta = Z1^2 */
    fp51_chain_sq(delta, p->Z);

    /* gamma = Y1^2 */
    fp51_chain_sq(gamma, p->Y);

    /* beta = X1 * gamma */
    fp51_chain_mul(beta, p->X, gamma);

    /* alpha = 3 * (X1 - delta) * (X1 + delta) */
    fp_sub(t0, p->X, delta);
    fp_add(t1, p->X, delta);
    fp51_chain_mul(alpha, t0, t1);
    /* alpha = 3 * alpha */
    fp_add(t0, alpha, alpha);
    fp_add(alpha, t0, alpha);

    /* X3 = alpha^2 - 8*beta */
    fp51_chain_sq(r->X, alpha);
    fp_add(t0, beta, beta);        /* 2*beta */
    fp_add(t0, t0, t0);            /* 4*beta */
    fp_sub(r->X, r->X, t0);        /* alpha^2 - 4*beta */
    fp_sub(r->X, r->X, t0);        /* alpha^2 - 8*beta */

    /* Z3 = (Y1 + Z1)^2 - gamma - delta */
    fp_add(t1, p->Y, p->Z);
    fp51_chain_sq(t2, t1);
    fp_sub(t2, t2, gamma);
    fp_sub(r->Z, t2, delta);

    /* Y3 = alpha * (4*beta - X3) - 8*gamma^2 */
    fp_sub(t1, t0, r->X);          /* 4*beta - X3 */
    fp51_chain_mul(t2, alpha, t1);
    fp51_chain_sq(t0, gamma);       /* gamma^2 */
    fp_add(t0, t0, t0);            /* 2*gamma^2 */
    fp_add(t0, t0, t0);            /* 4*gamma^2 */
    fp_sub(r->Y, t2, t0);          /* ... - 4*gamma^2 */
    fp_sub(r->Y, r->Y, t0);        /* ... - 8*gamma^2 */
}
