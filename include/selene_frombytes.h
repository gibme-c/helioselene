#ifndef HELIOSELENE_SELENE_FROMBYTES_H
#define HELIOSELENE_SELENE_FROMBYTES_H

#include "selene.h"

#if HELIOSELENE_PLATFORM_64BIT
int selene_frombytes_x64(selene_jacobian *r, const unsigned char s[32]);
static inline int selene_frombytes(selene_jacobian *r, const unsigned char s[32])
{
    return selene_frombytes_x64(r, s);
}
#endif

#endif // HELIOSELENE_SELENE_FROMBYTES_H
