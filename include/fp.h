#ifndef HELIOSELENE_FP_H
#define HELIOSELENE_FP_H

#include "helioselene_platform.h"

#include <cstdint>

#if HELIOSELENE_PLATFORM_64BIT
typedef uint64_t fp_fe[5];
#else
typedef int32_t fp_fe[10];
#endif

#endif // HELIOSELENE_FP_H
