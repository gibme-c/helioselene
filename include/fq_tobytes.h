#ifndef HELIOSELENE_FQ_TOBYTES_H
#define HELIOSELENE_FQ_TOBYTES_H

#include "fq.h"

#if HELIOSELENE_PLATFORM_64BIT
void fq_tobytes_x64(unsigned char *s, const fq_fe h);
static inline void fq_tobytes(unsigned char *s, const fq_fe h)
{
    fq_tobytes_x64(s, h);
}
#endif

#endif // HELIOSELENE_FQ_TOBYTES_H
