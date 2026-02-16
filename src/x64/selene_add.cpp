#include "x64/selene_add.h"

#include "fq_ops.h"
#include "x64/fq51_chain.h"

/*
 * General addition: Jacobian + Jacobian -> Jacobian (over F_q)
 * Same formula as helios_add but over F_q.
 * Cost: 11M + 5S
 */
void selene_add_x64(selene_jacobian *r, const selene_jacobian *p, const selene_jacobian *q)
{
    fq_fe Z1Z1, Z2Z2, U1, U2, S1, S2, H, I, J, rr, V;
    fq_fe t0, t1;

    fq51_chain_sq(Z1Z1, p->Z);
    fq51_chain_sq(Z2Z2, q->Z);

    fq51_chain_mul(U1, p->X, Z2Z2);
    fq51_chain_mul(U2, q->X, Z1Z1);

    fq51_chain_mul(t0, q->Z, Z2Z2);
    fq51_chain_mul(S1, p->Y, t0);
    fq51_chain_mul(t0, p->Z, Z1Z1);
    fq51_chain_mul(S2, q->Y, t0);

    fq_sub(H, U2, U1);

    fq_add(t0, H, H);
    fq51_chain_sq(I, t0);

    fq51_chain_mul(J, H, I);

    fq_sub(rr, S2, S1);
    fq_add(rr, rr, rr);

    fq51_chain_mul(V, U1, I);

    fq51_chain_sq(r->X, rr);
    fq_sub(r->X, r->X, J);
    fq_add(t0, V, V);
    fq_sub(r->X, r->X, t0);

    fq_sub(t0, V, r->X);
    fq51_chain_mul(t1, rr, t0);
    fq51_chain_mul(t0, S1, J);
    fq_add(t0, t0, t0);
    fq_sub(r->Y, t1, t0);

    fq_add(t0, p->Z, q->Z);
    fq51_chain_sq(t1, t0);
    fq_sub(t1, t1, Z1Z1);
    fq_sub(t1, t1, Z2Z2);
    fq51_chain_mul(r->Z, t1, H);
}
