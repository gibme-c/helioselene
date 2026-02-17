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

#ifndef HELIOSELENE_HELIOS_MSM_VARTIME_H
#define HELIOSELENE_HELIOS_MSM_VARTIME_H

/**
 * @file helios_msm_vartime.h
 * @brief Variable-time multi-scalar multiplication for Helios.
 *
 * Computes Q = s_0*P_0 + s_1*P_1 + ... + s_{n-1}*P_{n-1}.
 * Uses Straus (interleaved) for n <= 32, Pippenger (bucket) for n > 32.
 * Variable-time only: all MSM use cases involve public data.
 */

#include "helios.h"

#if HELIOSELENE_SIMD
#include "helioselene_dispatch.h"
static inline void
    helios_msm_vartime(helios_jacobian *result, const unsigned char *scalars, const helios_jacobian *points, size_t n)
{
    helioselene_get_dispatch().helios_msm_vartime(result, scalars, points, n);
}
#elif HELIOSELENE_PLATFORM_64BIT
void helios_msm_vartime_x64(
    helios_jacobian *result,
    const unsigned char *scalars,
    const helios_jacobian *points,
    size_t n);
static inline void
    helios_msm_vartime(helios_jacobian *result, const unsigned char *scalars, const helios_jacobian *points, size_t n)
{
    helios_msm_vartime_x64(result, scalars, points, n);
}
#else
void helios_msm_vartime_portable(
    helios_jacobian *result,
    const unsigned char *scalars,
    const helios_jacobian *points,
    size_t n);
static inline void
    helios_msm_vartime(helios_jacobian *result, const unsigned char *scalars, const helios_jacobian *points, size_t n)
{
    helios_msm_vartime_portable(result, scalars, points, n);
}
#endif

#endif // HELIOSELENE_HELIOS_MSM_VARTIME_H
