#ifndef HELIOSELENE_HELIOS_FROMBYTES_H
#define HELIOSELENE_HELIOS_FROMBYTES_H

#include "helios.h"

#if HELIOSELENE_PLATFORM_64BIT
int helios_frombytes_x64(helios_jacobian *r, const unsigned char s[32]);
static inline int helios_frombytes(helios_jacobian *r, const unsigned char s[32])
{
    return helios_frombytes_x64(r, s);
}
#endif

#endif // HELIOSELENE_HELIOS_FROMBYTES_H
