#ifndef HELIOSELENE_FQ_INVERT_H
#define HELIOSELENE_FQ_INVERT_H

#include "fq.h"

#if HELIOSELENE_PLATFORM_64BIT
void fq_invert_x64(fq_fe out, const fq_fe z);
static inline void fq_invert(fq_fe out, const fq_fe z)
{
    fq_invert_x64(out, z);
}
#endif

#endif // HELIOSELENE_FQ_INVERT_H
