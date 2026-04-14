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

#include "x64/fq_sqn.h"

#include "x64/fq51_inline.h"

void fq_sqn_x64(fq_fe h, const fq_fe f, int n)
{
#if RANSHAW_FQ_NATIVE64
    /* Square on a local accumulator (kept in registers/stack slots) rather
     * than writing through the output pointer each iteration: force-inlining
     * the MULX squaring asm with the in/out pointer aliased in a loop exhausts
     * the register allocator ('impossible constraints'). */
    uint64_t a[4] = {f[0], f[1], f[2], f[3]};
    for (int i = 0; i < n; i++)
        fq64_sq(a, a);
    h[0] = a[0];
    h[1] = a[1];
    h[2] = a[2];
    h[3] = a[3];
#else
    fq51_sq_inline(h, f);
    for (int i = 1; i < n; i++)
        fq51_sq_inline(h, h);
#endif
}
