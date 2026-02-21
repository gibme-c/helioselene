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
 * @file selene_scalarmult_vartime.h
 * @brief Variable-time Selene scalar multiplication using wNAF with w=5.
 */

#ifndef HELIOSELENE_SELENE_SCALARMULT_VARTIME_H
#define HELIOSELENE_SELENE_SCALARMULT_VARTIME_H

#include "selene.h"

#if HELIOSELENE_SIMD
#include "helioselene_dispatch.h"
static inline void
    selene_scalarmult_vartime(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p)
{
    helioselene_get_dispatch().selene_scalarmult_vartime(r, scalar, p);
}
#elif HELIOSELENE_PLATFORM_64BIT
void selene_scalarmult_vartime_x64(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p);
static inline void
    selene_scalarmult_vartime(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p)
{
    selene_scalarmult_vartime_x64(r, scalar, p);
}
#else
void selene_scalarmult_vartime_portable(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p);
static inline void
    selene_scalarmult_vartime(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p)
{
    selene_scalarmult_vartime_portable(r, scalar, p);
}
#endif

#endif // HELIOSELENE_SELENE_SCALARMULT_VARTIME_H
