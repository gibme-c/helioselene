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
 * @file fq_is_qr.h
 * @brief Constant-time Legendre symbol test for F_q.
 *
 * Returns 1 if z is a quadratic residue mod q (by convention z = 0 is treated
 * as a QR since sqrt(0) = 0 exists), 0 if z is a non-residue. Constant-time
 * regardless of input. The x64 backend uses Bernstein-Yang positive-divsteps
 * with Jacobi-bit tracking (~same cost profile as fq_invert). The portable
 * backend delegates to fq_sqrt (no speedup but correct).
 */

#ifndef RANSHAW_FQ_IS_QR_H
#define RANSHAW_FQ_IS_QR_H

#include "fq.h"

#if RANSHAW_PLATFORM_64BIT
int fq_is_qr_x64(const fq_fe z);
static inline int fq_is_qr(const fq_fe z)
{
    return fq_is_qr_x64(z);
}
#else
int fq_is_qr_portable(const fq_fe z);
static inline int fq_is_qr(const fq_fe z)
{
    return fq_is_qr_portable(z);
}
#endif

#endif // RANSHAW_FQ_IS_QR_H
