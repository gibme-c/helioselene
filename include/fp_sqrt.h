#ifndef HELIOSELENE_FP_SQRT_H
#define HELIOSELENE_FP_SQRT_H

#include "fp.h"

#if HELIOSELENE_PLATFORM_64BIT
int fp_sqrt_x64(fp_fe out, const fp_fe z);
static inline int fp_sqrt(fp_fe out, const fp_fe z)
{
    return fp_sqrt_x64(out, z);
}
#endif

#endif // HELIOSELENE_FP_SQRT_H
