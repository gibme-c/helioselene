#ifndef HELIOSELENE_FQ_MUL_H
#define HELIOSELENE_FQ_MUL_H

#include "fq.h"

#if HELIOSELENE_PLATFORM_64BIT
void fq_mul_x64(fq_fe h, const fq_fe f, const fq_fe g);
static inline void fq_mul(fq_fe h, const fq_fe f, const fq_fe g)
{
    fq_mul_x64(h, f, g);
}
#endif

#endif // HELIOSELENE_FQ_MUL_H
