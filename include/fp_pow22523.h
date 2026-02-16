#ifndef HELIOSELENE_FP_POW22523_H
#define HELIOSELENE_FP_POW22523_H

#include "fp.h"

#if HELIOSELENE_PLATFORM_64BIT
void fp_pow22523_x64(fp_fe out, const fp_fe z);
static inline void fp_pow22523(fp_fe out, const fp_fe z)
{
    fp_pow22523_x64(out, z);
}
#endif

#endif // HELIOSELENE_FP_POW22523_H
