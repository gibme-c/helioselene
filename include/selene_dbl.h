#ifndef HELIOSELENE_SELENE_DBL_H
#define HELIOSELENE_SELENE_DBL_H

#include "selene.h"

#if HELIOSELENE_PLATFORM_64BIT
void selene_dbl_x64(selene_jacobian *r, const selene_jacobian *p);
static inline void selene_dbl(selene_jacobian *r, const selene_jacobian *p)
{
    selene_dbl_x64(r, p);
}
#endif

#endif // HELIOSELENE_SELENE_DBL_H
