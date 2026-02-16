#include "x64/fp_invert.h"

#include "helioselene_secure_erase.h"
#include "x64/fp51_chain.h"

void fp_invert_x64(fp_fe out, const fp_fe z)
{
    fp_fe t0;
    fp_fe t1;
    fp_fe t2;
    fp_fe t3;

    fp51_chain_sq(t0, z);
    fp51_chain_sq(t1, t0);
    fp51_chain_sq(t1, t1);
    fp51_chain_mul(t1, z, t1);
    fp51_chain_mul(t0, t0, t1);
    fp51_chain_sq(t2, t0);
    fp51_chain_mul(t1, t1, t2);
    fp51_chain_sqn(t2, t1, 5);
    fp51_chain_mul(t1, t2, t1);
    fp51_chain_sqn(t2, t1, 10);
    fp51_chain_mul(t2, t2, t1);
    fp51_chain_sqn(t3, t2, 20);
    fp51_chain_mul(t2, t3, t2);
    fp51_chain_sqn(t2, t2, 10);
    fp51_chain_mul(t1, t2, t1);
    fp51_chain_sqn(t2, t1, 50);
    fp51_chain_mul(t2, t2, t1);
    fp51_chain_sqn(t3, t2, 100);
    fp51_chain_mul(t2, t3, t2);
    fp51_chain_sqn(t2, t2, 50);
    fp51_chain_mul(t1, t2, t1);
    fp51_chain_sqn(t1, t1, 5);
    fp51_chain_mul(out, t1, t0);

    helioselene_secure_erase(t0, sizeof(fp_fe));
    helioselene_secure_erase(t1, sizeof(fp_fe));
    helioselene_secure_erase(t2, sizeof(fp_fe));
    helioselene_secure_erase(t3, sizeof(fp_fe));
}
