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

#ifndef HELIOSELENE_X64_FQ51_CHAIN_H
#define HELIOSELENE_X64_FQ51_CHAIN_H

#if defined(_MSC_VER)

#include "fq.h"

void fq_mul_x64(fq_fe h, const fq_fe f, const fq_fe g);
void fq_sq_x64(fq_fe h, const fq_fe f);
void fq_sqn_x64(fq_fe h, const fq_fe f, int n);

#define fq51_chain_mul fq_mul_x64
#define fq51_chain_sq fq_sq_x64
#define fq51_chain_sqn fq_sqn_x64

#else

#include "x64/fq51_inline.h"

#define fq51_chain_mul fq51_mul_inline
#define fq51_chain_sq fq51_sq_inline

static HELIOSELENE_FORCE_INLINE void fq51_sqn_inline(fq_fe h, const fq_fe f, int n)
{
    fq51_sq_inline(h, f);
    for (int i = 1; i < n; i++)
        fq51_sq_inline(h, h);
}

#define fq51_chain_sqn fq51_sqn_inline

#endif

#endif // HELIOSELENE_X64_FQ51_CHAIN_H
