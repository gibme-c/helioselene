#ifndef HELIOSELENE_SELENE_TOBYTES_H
#define HELIOSELENE_SELENE_TOBYTES_H

#include "selene.h"

#if HELIOSELENE_PLATFORM_64BIT
void selene_tobytes_x64(unsigned char s[32], const selene_jacobian *p);
static inline void selene_tobytes(unsigned char s[32], const selene_jacobian *p)
{
    selene_tobytes_x64(s, p);
}
#endif

#endif // HELIOSELENE_SELENE_TOBYTES_H
