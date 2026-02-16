#ifndef HELIOSELENE_FP_SQ_H
#define HELIOSELENE_FP_SQ_H

#include "fp.h"

#if HELIOSELENE_PLATFORM_64BIT
void fp_sq_x64(fp_fe h, const fp_fe f);
static inline void fp_sq(fp_fe h, const fp_fe f)
{
    fp_sq_x64(h, f);
}
#endif

#endif // HELIOSELENE_FP_SQ_H
