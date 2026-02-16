#ifndef HELIOSELENE_FP_TOBYTES_H
#define HELIOSELENE_FP_TOBYTES_H

#include "fp.h"

#if HELIOSELENE_PLATFORM_64BIT
void fp_tobytes_x64(unsigned char *s, const fp_fe h);
static inline void fp_tobytes(unsigned char *s, const fp_fe h)
{
    fp_tobytes_x64(s, h);
}
#endif

#endif // HELIOSELENE_FP_TOBYTES_H
