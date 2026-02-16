#ifndef HELIOSELENE_SELENE_MADD_H
#define HELIOSELENE_SELENE_MADD_H

#include "selene.h"

#if HELIOSELENE_PLATFORM_64BIT
void selene_madd_x64(selene_jacobian *r, const selene_jacobian *p, const selene_affine *q);
static inline void selene_madd(selene_jacobian *r, const selene_jacobian *p, const selene_affine *q)
{
    selene_madd_x64(r, p, q);
}
#endif

#endif // HELIOSELENE_SELENE_MADD_H
