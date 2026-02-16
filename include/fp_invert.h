#ifndef HELIOSELENE_FP_INVERT_H
#define HELIOSELENE_FP_INVERT_H

#include "fp.h"

#if HELIOSELENE_PLATFORM_64BIT
void fp_invert_x64(fp_fe out, const fp_fe z);
static inline void fp_invert(fp_fe out, const fp_fe z)
{
    fp_invert_x64(out, z);
}
#endif

#endif // HELIOSELENE_FP_INVERT_H
