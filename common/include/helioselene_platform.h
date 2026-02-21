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
 * @file helioselene_platform.h
 * @brief Compile-time platform detection and 128-bit multiplication support.
 *
 * Detects x86-64 vs ARM64 vs 32-bit, and selects between __int128 (GCC/Clang) or
 * _umul128 intrinsic (MSVC) for 64x64->128 multiplication. FORCE_PORTABLE overrides
 * 64-bit detection to force the 32-bit radix-2^25.5 backend.
 */

#ifndef HELIOSELENE_PLATFORM_H
#define HELIOSELENE_PLATFORM_H

#if defined(__x86_64__) || defined(_M_X64)
#define HELIOSELENE_PLATFORM_X64 1
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define HELIOSELENE_PLATFORM_ARM64 1
#endif

#if !FORCE_PORTABLE && (HELIOSELENE_PLATFORM_X64 || HELIOSELENE_PLATFORM_ARM64)
#define HELIOSELENE_PLATFORM_64BIT 1
#endif

#if defined(__SIZEOF_INT128__)
#define HELIOSELENE_HAVE_INT128 1
typedef unsigned __int128 helioselene_uint128;
#elif defined(_M_X64)
#define HELIOSELENE_HAVE_UMUL128 1
#include <intrin.h>
#endif

#endif // HELIOSELENE_PLATFORM_H
