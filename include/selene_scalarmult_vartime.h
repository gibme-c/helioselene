#ifndef HELIOSELENE_SELENE_SCALARMULT_VARTIME_H
#define HELIOSELENE_SELENE_SCALARMULT_VARTIME_H

#include "selene.h"

#if HELIOSELENE_PLATFORM_64BIT
void selene_scalarmult_vartime_x64(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p);
static inline void selene_scalarmult_vartime(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p)
{
    selene_scalarmult_vartime_x64(r, scalar, p);
}
#endif

#endif // HELIOSELENE_SELENE_SCALARMULT_VARTIME_H
