#ifndef HELIOSELENE_FQ_CMOV_H
#define HELIOSELENE_FQ_CMOV_H

#include "ct_barrier.h"
#include "fq.h"

#if HELIOSELENE_PLATFORM_64BIT
static inline void fq_cmov(fq_fe f, const fq_fe g, unsigned int b)
{
    uint64_t mask = 0 - (uint64_t)ct_barrier_u32(b);
    f[0] ^= mask & (f[0] ^ g[0]);
    f[1] ^= mask & (f[1] ^ g[1]);
    f[2] ^= mask & (f[2] ^ g[2]);
    f[3] ^= mask & (f[3] ^ g[3]);
    f[4] ^= mask & (f[4] ^ g[4]);
}
#endif

#endif // HELIOSELENE_FQ_CMOV_H
