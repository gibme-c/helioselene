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
 * @file helios_madd.h
 * @brief Mixed addition for Helios: Jacobian + affine input, saving one multiplication over full Jacobian addition.
 */

#ifndef HELIOSELENE_HELIOS_MADD_H
#define HELIOSELENE_HELIOS_MADD_H

#include "helios.h"

#if HELIOSELENE_PLATFORM_64BIT
void helios_madd_x64(helios_jacobian *r, const helios_jacobian *p, const helios_affine *q);
static inline void helios_madd(helios_jacobian *r, const helios_jacobian *p, const helios_affine *q)
{
    helios_madd_x64(r, p, q);
}
#else
void helios_madd_portable(helios_jacobian *r, const helios_jacobian *p, const helios_affine *q);
static inline void helios_madd(helios_jacobian *r, const helios_jacobian *p, const helios_affine *q)
{
    helios_madd_portable(r, p, q);
}
#endif

#endif // HELIOSELENE_HELIOS_MADD_H
