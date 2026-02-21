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

/**
 * @file helioselene_dispatch.h
 * @brief Runtime dispatch table for SIMD-accelerated curve operations.
 *
 * Manages a 6-slot function pointer table: {helios,selene} x {scalarmult, scalarmult_vartime,
 * msm_vartime}. helioselene_init() populates slots based on CPUID (IFMA > AVX2 > x64 baseline).
 * helioselene_autotune() benchmarks all available backends and picks the fastest per-slot.
 * On non-SIMD platforms, init/autotune are no-ops (only one backend exists).
 */

#ifndef HELIOSELENE_DISPATCH_H
#define HELIOSELENE_DISPATCH_H

#include "helioselene_platform.h"

#if HELIOSELENE_SIMD

#include "helios.h"
#include "selene.h"

#include <cstddef>

struct helioselene_dispatch_table
{
    void (*helios_scalarmult)(helios_jacobian *, const unsigned char[32], const helios_jacobian *);
    void (*helios_scalarmult_vartime)(helios_jacobian *, const unsigned char[32], const helios_jacobian *);
    void (*helios_msm_vartime)(helios_jacobian *, const unsigned char *, const helios_jacobian *, size_t);
    void (*selene_scalarmult)(selene_jacobian *, const unsigned char[32], const selene_jacobian *);
    void (*selene_scalarmult_vartime)(selene_jacobian *, const unsigned char[32], const selene_jacobian *);
    void (*selene_msm_vartime)(selene_jacobian *, const unsigned char *, const selene_jacobian *, size_t);
};

const helioselene_dispatch_table &helioselene_get_dispatch();

void helioselene_init(void);

void helioselene_autotune(void);

#else

static inline void helioselene_init(void) {}
static inline void helioselene_autotune(void) {}

#endif // HELIOSELENE_SIMD

#endif // HELIOSELENE_DISPATCH_H
