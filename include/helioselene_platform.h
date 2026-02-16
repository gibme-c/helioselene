#ifndef HELIOSELENE_PLATFORM_H
#define HELIOSELENE_PLATFORM_H

#if defined(__x86_64__) || defined(_M_X64)
#define HELIOSELENE_PLATFORM_X64 1
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define HELIOSELENE_PLATFORM_ARM64 1
#endif

#if !HELIOSELENE_FORCE_REF10 && (HELIOSELENE_PLATFORM_X64 || HELIOSELENE_PLATFORM_ARM64)
#define HELIOSELENE_PLATFORM_64BIT 1
#endif

#if defined(__SIZEOF_INT128__)
#define HELIOSELENE_HAVE_INT128 1
typedef unsigned __int128 helioselene_uint128;
#elif defined(_M_X64)
#define HELIOSELENE_HAVE_UMUL128 1
#include <intrin.h>
#endif

#endif // HELIOSELENE_PLATFORM_H
