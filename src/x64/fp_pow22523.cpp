#include "x64/fp_pow22523.h"

#include "helioselene_secure_erase.h"
#include "x64/fp51_chain.h"

void fp_pow22523_x64(fp_fe out, const fp_fe z)
{
    fp_fe t0;
    fp_fe t1;
    fp_fe t2;

    fp51_chain_sq(t0, z);
    fp51_chain_sqn(t1, t0, 2);
    fp51_chain_mul(t1, z, t1);
    fp51_chain_mul(t0, t0, t1);
    fp51_chain_sq(t0, t0);
    fp51_chain_mul(t0, t1, t0);
    fp51_chain_sqn(t1, t0, 5);
    fp51_chain_mul(t0, t1, t0);
    fp51_chain_sqn(t1, t0, 10);
    fp51_chain_mul(t1, t1, t0);
    fp51_chain_sqn(t2, t1, 20);
    fp51_chain_mul(t1, t2, t1);
    fp51_chain_sqn(t1, t1, 10);
    fp51_chain_mul(t0, t1, t0);
    fp51_chain_sqn(t1, t0, 50);
    fp51_chain_mul(t1, t1, t0);
    fp51_chain_sqn(t2, t1, 100);
    fp51_chain_mul(t1, t2, t1);
    fp51_chain_sqn(t1, t1, 50);
    fp51_chain_mul(t0, t1, t0);
    fp51_chain_sqn(t0, t0, 2);
    fp51_chain_mul(out, t0, z);

    helioselene_secure_erase(t0, sizeof(fp_fe));
    helioselene_secure_erase(t1, sizeof(fp_fe));
    helioselene_secure_erase(t2, sizeof(fp_fe));
}
