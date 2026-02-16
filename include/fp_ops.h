#ifndef HELIOSELENE_FP_OPS_H
#define HELIOSELENE_FP_OPS_H

#include "fp.h"

#include <cstring>

#if HELIOSELENE_PLATFORM_64BIT
#include "x64/fp51.h"

static inline void fp_add(fp_fe h, const fp_fe f, const fp_fe g)
{
    h[0] = f[0] + g[0];
    h[1] = f[1] + g[1];
    h[2] = f[2] + g[2];
    h[3] = f[3] + g[3];
    h[4] = f[4] + g[4];
}

static inline void fp_sub(fp_fe h, const fp_fe f, const fp_fe g)
{
    uint64_t c;
    h[0] = f[0] + 0x1FFFFFFFFFFFB4ULL - g[0];
    c = h[0] >> 51;
    h[0] &= FP51_MASK;
    h[1] = f[1] + 0x1FFFFFFFFFFFFCULL - g[1] + c;
    c = h[1] >> 51;
    h[1] &= FP51_MASK;
    h[2] = f[2] + 0x1FFFFFFFFFFFFCULL - g[2] + c;
    c = h[2] >> 51;
    h[2] &= FP51_MASK;
    h[3] = f[3] + 0x1FFFFFFFFFFFFCULL - g[3] + c;
    c = h[3] >> 51;
    h[3] &= FP51_MASK;
    h[4] = f[4] + 0x1FFFFFFFFFFFFCULL - g[4] + c;
    c = h[4] >> 51;
    h[4] &= FP51_MASK;
    h[0] += c * 19;
}

static inline void fp_neg(fp_fe h, const fp_fe f)
{
    uint64_t c;
    h[0] = 0xFFFFFFFFFFFDAULL - f[0];
    c = h[0] >> 51;
    h[0] &= FP51_MASK;
    h[1] = 0xFFFFFFFFFFFFEULL - f[1] + c;
    c = h[1] >> 51;
    h[1] &= FP51_MASK;
    h[2] = 0xFFFFFFFFFFFFEULL - f[2] + c;
    c = h[2] >> 51;
    h[2] &= FP51_MASK;
    h[3] = 0xFFFFFFFFFFFFEULL - f[3] + c;
    c = h[3] >> 51;
    h[3] &= FP51_MASK;
    h[4] = 0xFFFFFFFFFFFFEULL - f[4] + c;
    c = h[4] >> 51;
    h[4] &= FP51_MASK;
    h[0] += c * 19;
}

#endif // HELIOSELENE_PLATFORM_64BIT

static inline void fp_copy(fp_fe h, const fp_fe f)
{
    std::memcpy(h, f, sizeof(fp_fe));
}

static inline void fp_0(fp_fe h)
{
    std::memset(h, 0, sizeof(fp_fe));
}

static inline void fp_1(fp_fe h)
{
    h[0] = 1;
    h[1] = 0;
    h[2] = 0;
    h[3] = 0;
    h[4] = 0;
#if !HELIOSELENE_PLATFORM_64BIT
    h[5] = 0;
    h[6] = 0;
    h[7] = 0;
    h[8] = 0;
    h[9] = 0;
#endif
}

#endif // HELIOSELENE_FP_OPS_H
