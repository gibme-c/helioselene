#ifndef HELIOSELENE_FQ_SQRT_H
#define HELIOSELENE_FQ_SQRT_H

#include "fq.h"

#if HELIOSELENE_PLATFORM_64BIT
void fq_sqrt_x64(fq_fe out, const fq_fe z);
static inline void fq_sqrt(fq_fe out, const fq_fe z)
{
    fq_sqrt_x64(out, z);
}
#endif

#endif // HELIOSELENE_FQ_SQRT_H
