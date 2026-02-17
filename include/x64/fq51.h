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

#ifndef HELIOSELENE_X64_FQ51_H
#define HELIOSELENE_X64_FQ51_H

#include <cstdint>

static const uint64_t FQ51_MASK = (1ULL << 51) - 1;

/*
 * q = 2^255 - gamma, where gamma = 85737960593035654572250192257530476641
 * gamma is 127 bits, fitting in 3 radix-2^51 limbs.
 *
 * gamma in radix-2^51:
 *   GAMMA_51[0] = 0x12D8D86D83861
 *   GAMMA_51[1] = 0x269135294F229
 *   GAMMA_51[2] = 0x102021F
 *
 * 2*gamma in radix-2^51 (128 bits, 3 limbs):
 *   TWO_GAMMA_51[0] = 0x25B1B0DB070C2
 *   TWO_GAMMA_51[1] = 0x4D226A529E452
 *   TWO_GAMMA_51[2] = 0x204043E
 */
static const uint64_t GAMMA_51[3] = {0x12D8D86D83861ULL, 0x269135294F229ULL, 0x102021FULL};

static const uint64_t TWO_GAMMA_51[3] = {0x25B1B0DB070C2ULL, 0x4D226A529E452ULL, 0x204043EULL};

/*
 * q in radix-2^51:
 *   Q_51[0] = 0x6D2727927C79F
 *   Q_51[1] = 0x596ECAD6B0DD6
 *   Q_51[2] = 0x7FFFFFEFDFDE0
 *   Q_51[3] = 0x7FFFFFFFFFFFF
 *   Q_51[4] = 0x7FFFFFFFFFFFF
 */
static const uint64_t Q_51[5] =
    {0x6D2727927C79FULL, 0x596ECAD6B0DD6ULL, 0x7FFFFFEFDFDE0ULL, 0x7FFFFFFFFFFFFULL, 0x7FFFFFFFFFFFFULL};

/*
 * Carry-propagate a field element so every limb is ≤ 51 bits.
 * Uses gamma-fold for the carry out of limb 4.
 */
static inline void fq51_carry(fq_fe h, const fq_fe f)
{
    int64_t d0 = (int64_t)f[0], d1 = (int64_t)f[1], d2 = (int64_t)f[2];
    int64_t d3 = (int64_t)f[3], d4 = (int64_t)f[4];
    int64_t carry;
    carry = d0 >> 51;
    d1 += carry;
    d0 -= carry << 51;
    carry = d1 >> 51;
    d2 += carry;
    d1 -= carry << 51;
    carry = d2 >> 51;
    d3 += carry;
    d2 -= carry << 51;
    carry = d3 >> 51;
    d4 += carry;
    d3 -= carry << 51;
    carry = d4 >> 51;
    d4 -= carry << 51;
    d0 += carry * (int64_t)GAMMA_51[0];
    d1 += carry * (int64_t)GAMMA_51[1];
    d2 += carry * (int64_t)GAMMA_51[2];
    carry = d0 >> 51;
    d1 += carry;
    d0 -= carry << 51;
    carry = d1 >> 51;
    d2 += carry;
    d1 -= carry << 51;
    carry = d2 >> 51;
    d3 += carry;
    d2 -= carry << 51;
    carry = d3 >> 51;
    d4 += carry;
    d3 -= carry << 51;
    h[0] = (uint64_t)d0;
    h[1] = (uint64_t)d1;
    h[2] = (uint64_t)d2;
    h[3] = (uint64_t)d3;
    h[4] = (uint64_t)d4;
}

#endif // HELIOSELENE_X64_FQ51_H
