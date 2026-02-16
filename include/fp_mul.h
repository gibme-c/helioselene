#ifndef HELIOSELENE_FP_MUL_H
#define HELIOSELENE_FP_MUL_H

#include "fp.h"

#if HELIOSELENE_PLATFORM_64BIT
void fp_mul_x64(fp_fe h, const fp_fe f, const fp_fe g);
static inline void fp_mul(fp_fe h, const fp_fe f, const fp_fe g)
{
    fp_mul_x64(h, f, g);
}
#endif

#endif // HELIOSELENE_FP_MUL_H
