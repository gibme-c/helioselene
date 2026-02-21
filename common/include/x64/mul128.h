// Copyright (c) 2025-2026, Brandon Lehmann
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
