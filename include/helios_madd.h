#ifndef HELIOSELENE_HELIOS_MADD_H
#define HELIOSELENE_HELIOS_MADD_H

#include "helios.h"

#if HELIOSELENE_PLATFORM_64BIT
void helios_madd_x64(helios_jacobian *r, const helios_jacobian *p, const helios_affine *q);
static inline void helios_madd(helios_jacobian *r, const helios_jacobian *p, const helios_affine *q)
{
    helios_madd_x64(r, p, q);
}
#endif

#endif // HELIOSELENE_HELIOS_MADD_H
