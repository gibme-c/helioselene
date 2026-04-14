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
 * @file fq51_chain.h
 * @brief x64 (radix-2^51) implementation of F_q addition chains with Crandall reduction.
 */

#ifndef RANSHAW_X64_FQ51_CHAIN_H
#define RANSHAW_X64_FQ51_CHAIN_H

#if defined(_MSC_VER)

/*
 * MSVC's c2.dll optimizer goes superlinear on long chains of __forceinline
 * field operations (fq_sqrt alone triggers 252 squarings + 50 multiplications).
 * Use non-inline function calls for chain operations to keep compile times sane.
 */
#include "fq.h"

void fq_mul_x64(fq_fe h, const fq_fe f, const fq_fe g);
void fq_sq_x64(fq_fe h, const fq_fe f);
void fq_sq2_x64(fq_fe h, const fq_fe f);
void fq_sqn_x64(fq_fe h, const fq_fe f, int n);

#define fq51_chain_mul fq_mul_x64
#define fq51_chain_sq fq_sq_x64
#define fq51_chain_sq2 fq_sq2_x64
#define fq51_chain_sqn fq_sqn_x64

#else

#include "fq.h"

/*
 * Native radix-2^64 chain ops route through the non-inline x64 wrappers (which
 * call fq64_mul/sq once). Long chains (fq_sqrt: 252 sq + 50 mul) must NOT
 * force-inline the large fq64_mul MULX asm at every step: inlining dozens of
 * back-to-back asm blocks exhausts the register allocator ("impossible
 * constraints"). The hot point-op paths call fq64_* directly and stay inlined;
 * only these addition chains use the call form.
 */
void fq_mul_x64(fq_fe h, const fq_fe f, const fq_fe g);
void fq_sq_x64(fq_fe h, const fq_fe f);
void fq_sq2_x64(fq_fe h, const fq_fe f);
void fq_sqn_x64(fq_fe h, const fq_fe f, int n);

#define fq51_chain_mul fq_mul_x64
#define fq51_chain_sq fq_sq_x64
#define fq51_chain_sq2 fq_sq2_x64
#define fq51_chain_sqn fq_sqn_x64

#endif /* _MSC_VER */

#endif // RANSHAW_X64_FQ51_CHAIN_H
