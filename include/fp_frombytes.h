#ifndef HELIOSELENE_FP_FROMBYTES_H
#define HELIOSELENE_FP_FROMBYTES_H

#include "fp.h"

#if HELIOSELENE_PLATFORM_64BIT
void fp_frombytes_x64(fp_fe h, const unsigned char *s);
static inline void fp_frombytes(fp_fe h, const unsigned char *s)
{
    fp_frombytes_x64(h, s);
}
#endif

#endif // HELIOSELENE_FP_FROMBYTES_H
