#ifndef HELIOSELENE_FQ_SQ_H
#define HELIOSELENE_FQ_SQ_H

#include "fq.h"

#if HELIOSELENE_PLATFORM_64BIT
void fq_sq_x64(fq_fe h, const fq_fe f);
static inline void fq_sq(fq_fe h, const fq_fe f)
{
    fq_sq_x64(h, f);
}
#endif

#endif // HELIOSELENE_FQ_SQ_H
