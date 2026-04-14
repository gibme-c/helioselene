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
 * @file fq.h
 * @brief Field element type for F_q where q = 2^255 - gamma (Crandall prime, gamma ~ 2^126).
 *
 * On 64-bit platforms: fq_fe is uint64_t[5] in radix-2^51 representation.
 * On 32-bit platforms: fq_fe is int32_t[10] in radix-2^25.5 (alternating 26/25-bit limbs).
 * Same layout as fp_fe, but reduction uses Crandall folding instead of the 2^255-19 shortcut.
 */

#ifndef RANSHAW_FQ_H
#define RANSHAW_FQ_H

#include "ranshaw_platform.h"

#include <cstdint>

/*
 * F_q representation gate.
 *
 * RANSHAW_FQ_NATIVE64: native radix-2^64 (uint64_t[4]). Selected only on the
 * GNU/Clang + BMI2 + __int128 toolchains that provide the hand-written 4x64
 * fq64_* primitives (ADX MULX/ADCX/ADOX, with __int128 fallbacks). On those
 * targets fq_fe IS the packed 4x64 form, so every standalone F_q op runs
 * native with no 5x51<->4x64 pack/unpack tax; value invariant < 2^256
 * (mul/sq fold to < 2q, add/sub stay < 2^256), canonicalized only at the
 * tobytes / iszero boundaries.
 *
 * Everything else (MSVC, ARM64, non-BMI2 x86, the forced-portable build)
 * keeps the original radix-2^51 uint64_t[5] (64-bit) or radix-2^25.5
 * int32_t[10] (portable) representation, which does not depend on the 4x64
 * primitives. The condition mirrors FQ51_HAVE_ADX_MUL in fq51_inline.h.
 */
#if defined(RANSHAW_FORCE_5X51) && RANSHAW_FORCE_5X51
/* Test override: force the radix-2^51 uint64_t[5] path on a 64-bit toolchain
 * that would otherwise use native 4x64, to exercise the non-native code on a
 * debuggable compiler. */
#define RANSHAW_FQ_NATIVE64 0
#elif RANSHAW_PLATFORM_64BIT && (defined(__GNUC__) || defined(__clang__)) && defined(__BMI2__) \
    && defined(__SIZEOF_INT128__)
#define RANSHAW_FQ_NATIVE64 1
#else
#define RANSHAW_FQ_NATIVE64 0
#endif

#if RANSHAW_FQ_NATIVE64
typedef uint64_t fq_fe[4];
#elif RANSHAW_PLATFORM_64BIT
typedef uint64_t fq_fe[5];
#else
typedef int32_t fq_fe[10];
#endif

#endif // RANSHAW_FQ_H
