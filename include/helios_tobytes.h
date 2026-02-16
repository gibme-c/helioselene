#ifndef HELIOSELENE_HELIOS_TOBYTES_H
#define HELIOSELENE_HELIOS_TOBYTES_H

#include "helios.h"

#if HELIOSELENE_PLATFORM_64BIT
void helios_tobytes_x64(unsigned char s[32], const helios_jacobian *p);
static inline void helios_tobytes(unsigned char s[32], const helios_jacobian *p)
{
    helios_tobytes_x64(s, p);
}
#endif

#endif // HELIOSELENE_HELIOS_TOBYTES_H
