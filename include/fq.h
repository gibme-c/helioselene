#ifndef HELIOSELENE_FQ_H
#define HELIOSELENE_FQ_H

#include "helioselene_platform.h"

#include <cstdint>

#if HELIOSELENE_PLATFORM_64BIT
typedef uint64_t fq_fe[5];
#else
typedef int32_t fq_fe[10];
#endif

#endif // HELIOSELENE_FQ_H
