#include "x64/selene_dbl.h"

#include "fq_ops.h"
#include "x64/fq51_chain.h"

/*
 * Jacobian point doubling with a = -3 optimization.
 * Same formula as helios_dbl but over F_q.
 * Cost: 3M + 5S
 */
void selene_dbl_x64(selene_jacobian *r, const selene_jacobian *p)
{
    fq_fe delta, gamma, beta, alpha;
    fq_fe t0, t1, t2;

    /* delta = Z1^2 */
    fq51_chain_sq(delta, p->Z);

    /* gamma = Y1^2 */
    fq51_chain_sq(gamma, p->Y);

    /* beta = X1 * gamma */
    fq51_chain_mul(beta, p->X, gamma);

    /* alpha = 3 * (X1 - delta) * (X1 + delta) */
    fq_sub(t0, p->X, delta);
    fq_add(t1, p->X, delta);
    fq51_chain_mul(alpha, t0, t1);
    fq_add(t0, alpha, alpha);
    fq_add(alpha, t0, alpha);

    /* X3 = alpha^2 - 8*beta */
    fq51_chain_sq(r->X, alpha);
    fq_add(t0, beta, beta);
    fq_add(t0, t0, t0);
    fq_add(t1, t0, t0);
    fq_sub(r->X, r->X, t1);

    /* Z3 = (Y1 + Z1)^2 - gamma - delta */
    fq_add(t1, p->Y, p->Z);
    fq51_chain_sq(t2, t1);
    fq_sub(t2, t2, gamma);
    fq_sub(r->Z, t2, delta);

    /* Y3 = alpha * (4*beta - X3) - 8*gamma^2 */
    fq_sub(t1, t0, r->X);
    fq51_chain_mul(t2, alpha, t1);
    fq51_chain_sq(t0, gamma);
    fq_add(t0, t0, t0);
    fq_add(t0, t0, t0);
    fq_add(t0, t0, t0);
    fq_sub(r->Y, t2, t0);
}
