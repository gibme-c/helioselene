#include "x64/selene_madd.h"

#include "fq_ops.h"
#include "x64/fq51_chain.h"

/*
 * Mixed addition: Jacobian + Affine -> Jacobian (over F_q)
 * Same formula as helios_madd but over F_q.
 * Cost: 7M + 4S
 */
void selene_madd_x64(selene_jacobian *r, const selene_jacobian *p, const selene_affine *q)
{
    fq_fe Z1Z1, U2, S2, H, HH, I, J, rr, V;
    fq_fe t0, t1;

    fq51_chain_sq(Z1Z1, p->Z);

    fq51_chain_mul(U2, q->x, Z1Z1);

    fq51_chain_mul(t0, p->Z, Z1Z1);
    fq51_chain_mul(S2, q->y, t0);

    fq_sub(H, U2, p->X);

    fq51_chain_sq(HH, H);

    fq_add(I, HH, HH);
    fq_add(I, I, I);

    fq51_chain_mul(J, H, I);

    fq_sub(rr, S2, p->Y);
    fq_add(rr, rr, rr);

    fq51_chain_mul(V, p->X, I);

    fq51_chain_sq(r->X, rr);
    fq_sub(r->X, r->X, J);
    fq_add(t0, V, V);
    fq_sub(r->X, r->X, t0);

    fq_sub(t0, V, r->X);
    fq51_chain_mul(t1, rr, t0);
    fq51_chain_mul(t0, p->Y, J);
    fq_add(t0, t0, t0);
    fq_sub(r->Y, t1, t0);

    fq_add(t0, p->Z, H);
    fq51_chain_sq(t1, t0);
    fq_sub(t1, t1, Z1Z1);
    fq_sub(r->Z, t1, HH);
}
