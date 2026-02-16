#ifndef HELIOSELENE_SELENE_H
#define HELIOSELENE_SELENE_H

#include "fq.h"

typedef struct SeleneJacobian
{
    fq_fe X;
    fq_fe Y;
    fq_fe Z;
} selene_jacobian;

typedef struct SeleneAffine
{
    fq_fe x;
    fq_fe y;
} selene_affine;

#endif // HELIOSELENE_SELENE_H
