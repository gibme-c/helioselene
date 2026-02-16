#ifndef HELIOSELENE_HELIOS_H
#define HELIOSELENE_HELIOS_H

#include "fp.h"

typedef struct HeliosJacobian
{
    fp_fe X;
    fp_fe Y;
    fp_fe Z;
} helios_jacobian;

typedef struct HeliosAffine
{
    fp_fe x;
    fp_fe y;
} helios_affine;

#endif // HELIOSELENE_HELIOS_H
