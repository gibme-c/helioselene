#ifndef HELIOSELENE_X64_MUL128_H
#define HELIOSELENE_X64_MUL128_H

#include "helioselene_platform.h"

#include <cstdint>

#if HELIOSELENE_HAVE_INT128

static inline helioselene_uint128 mul64(uint64_t a, uint64_t b)
{
    return (helioselene_uint128)a * b;
}

#elif HELIOSELENE_HAVE_UMUL128

struct helioselene_uint128_emu
{
    uint64_t lo;
    uint64_t hi;
};

static inline helioselene_uint128_emu mul64(uint64_t a, uint64_t b)
{
    helioselene_uint128_emu r;
    r.lo = _umul128(a, b, &r.hi);
    return r;
}

static inline helioselene_uint128_emu operator+(helioselene_uint128_emu a, helioselene_uint128_emu b)
{
    helioselene_uint128_emu r;
    r.lo = a.lo + b.lo;
    r.hi = a.hi + b.hi + (r.lo < a.lo ? 1 : 0);
    return r;
}

static inline helioselene_uint128_emu operator+(helioselene_uint128_emu a, uint64_t b)
{
    helioselene_uint128_emu r;
    r.lo = a.lo + b;
    r.hi = a.hi + (r.lo < a.lo ? 1 : 0);
    return r;
}

static inline helioselene_uint128_emu &operator+=(helioselene_uint128_emu &a, helioselene_uint128_emu b)
{
    a = a + b;
    return a;
}

static inline helioselene_uint128_emu &operator+=(helioselene_uint128_emu &a, uint64_t b)
{
    a = a + b;
    return a;
}

static inline uint64_t shr128(helioselene_uint128_emu v, int shift)
{
    if (shift == 0)
        return v.lo;
    if (shift < 64)
        return (v.lo >> shift) | (v.hi << (64 - shift));
    return v.hi >> (shift - 64);
}

static inline uint64_t lo128(helioselene_uint128_emu v)
{
    return v.lo;
}

#endif // HELIOSELENE_HAVE_UMUL128

#endif // HELIOSELENE_X64_MUL128_H
