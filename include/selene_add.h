#ifndef HELIOSELENE_SELENE_ADD_H
#define HELIOSELENE_SELENE_ADD_H

#include "selene.h"

#if HELIOSELENE_PLATFORM_64BIT
void selene_add_x64(selene_jacobian *r, const selene_jacobian *p, const selene_jacobian *q);
static inline void selene_add(selene_jacobian *r, const selene_jacobian *p, const selene_jacobian *q)
{
    selene_add_x64(r, p, q);
}
#endif

#endif // HELIOSELENE_SELENE_ADD_H
