#ifndef HELIOSELENE_HELIOS_DBL_H
#define HELIOSELENE_HELIOS_DBL_H

#include "helios.h"

#if HELIOSELENE_PLATFORM_64BIT
void helios_dbl_x64(helios_jacobian *r, const helios_jacobian *p);
static inline void helios_dbl(helios_jacobian *r, const helios_jacobian *p)
{
    helios_dbl_x64(r, p);
}
#endif

#endif // HELIOSELENE_HELIOS_DBL_H
