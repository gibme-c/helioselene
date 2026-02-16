#ifndef HELIOSELENE_FQ_FROMBYTES_H
#define HELIOSELENE_FQ_FROMBYTES_H

#include "fq.h"

#if HELIOSELENE_PLATFORM_64BIT
void fq_frombytes_x64(fq_fe h, const unsigned char *s);
static inline void fq_frombytes(fq_fe h, const unsigned char *s)
{
    fq_frombytes_x64(h, s);
}
#endif

#endif // HELIOSELENE_FQ_FROMBYTES_H
