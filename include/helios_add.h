#ifndef HELIOSELENE_HELIOS_ADD_H
#define HELIOSELENE_HELIOS_ADD_H

#include "helios.h"

#if HELIOSELENE_PLATFORM_64BIT
void helios_add_x64(helios_jacobian *r, const helios_jacobian *p, const helios_jacobian *q);
static inline void helios_add(helios_jacobian *r, const helios_jacobian *p, const helios_jacobian *q)
{
    helios_add_x64(r, p, q);
}
#endif

#endif // HELIOSELENE_HELIOS_ADD_H
