#ifndef HELIOSELENE_HELIOS_SCALARMULT_H
#define HELIOSELENE_HELIOS_SCALARMULT_H

#include "helios.h"

#if HELIOSELENE_PLATFORM_64BIT
void helios_scalarmult_x64(helios_jacobian *r, const unsigned char scalar[32], const helios_jacobian *p);
static inline void helios_scalarmult(helios_jacobian *r, const unsigned char scalar[32], const helios_jacobian *p)
{
    helios_scalarmult_x64(r, scalar, p);
}
#endif

#endif // HELIOSELENE_HELIOS_SCALARMULT_H
