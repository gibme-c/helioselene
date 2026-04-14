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

#include "fq_ops.h"
#include "fuzz_tests/common.h"
#include "fuzz_tests/registry.h"
#include "ranshaw_platform.h"
#include "shaw.h"
#include "shaw_constants.h"
#include "shaw_ops.h"

/* Forward-declared explicitly since shaw_scalarmult_vartime.h only exposes
 * the _x64 symbol when RANSHAW_SIMD is off; the default library build
 * defines RANSHAW_SIMD and routes through the dispatch table. The fuzz
 * harness needs the raw _x64 entry point to bypass dispatch. */
void shaw_scalarmult_vartime_x64(shaw_jacobian *r, const unsigned char scalar[32], const shaw_jacobian *p);

/*
 * Differential fuzz for shaw_scalarmult_vartime_x64: compare the packed
 * 4x64 wNAF rewrite against the always-compiled 5x51 unpacked reference
 * on toolchains where both are present. On MSVC / non-BMI2 / portable
 * builds the two entry points alias, so this harness is a no-op.
 *
 * Edge cases cover scalar=0, scalar=1, scalar=p-1 (forces -P via wNAF
 * digit selection), and identity input point. The random sweep uses
 * G-multiples as on-curve points without paying the rejection cost of
 * random-decompression.
 */

#if RANSHAW_PLATFORM_64BIT && (defined(__GNUC__) || defined(__clang__)) && defined(__BMI2__) && RANSHAW_HAVE_INT128
#define RANSHAW_SCALARMULT_PACKED_DIFF_ACTIVE 1
#endif

#ifdef RANSHAW_SCALARMULT_PACKED_DIFF_ACTIVE

extern "C" void
    shaw_scalarmult_vartime_x64_unpacked(shaw_jacobian *r, const unsigned char scalar[32], const shaw_jacobian *p);

static bool compare_scalarmult(const char *label, const unsigned char scalar[32], const shaw_jacobian *p)
{
    shaw_jacobian got, want;
    shaw_scalarmult_vartime_x64(&got, scalar, p);
    shaw_scalarmult_vartime_x64_unpacked(&want, scalar, p);

    unsigned char got_bytes[32], want_bytes[32];
    shaw_tobytes(got_bytes, &got);
    shaw_tobytes(want_bytes, &want);
    return check_bytes(label, want_bytes, got_bytes, 32);
}

static void make_random_scalar(unsigned char out[32], xoshiro256ss &rng)
{
    rng.fill_bytes(out, 32);
    out[31] &= 0x7F;
}

static void make_generator(shaw_jacobian *g)
{
    fq_copy(g->X, SHAW_GX);
    fq_copy(g->Y, SHAW_GY);
    fq_1(g->Z);
}

#endif /* RANSHAW_SCALARMULT_PACKED_DIFF_ACTIVE */

void fuzz_scalarmult_vartime_packed_diff()
{
    std::cout << std::endl << "=== Fuzz: shaw_scalarmult_vartime packed vs unpacked diff ===" << std::endl;

#ifndef RANSHAW_SCALARMULT_PACKED_DIFF_ACTIVE
    std::cout << "(skipped: FQ51_HAVE_ADX_MUL not defined on this toolchain; packed and unpacked paths coincide)"
              << std::endl;
    return;
#else
    xoshiro256ss rng;
    rng.seed(global_seed + 419);

    shaw_jacobian G;
    make_generator(&G);

    /* scalar=0 on G: result must be identity (early return path). */
    {
        unsigned char s[32] = {};
        compare_scalarmult("edge: scalar=0 on G", s, &G);
    }

    /* scalar=1 on G: result must be G (single-digit wNAF, start=0). */
    {
        unsigned char s[32] = {};
        s[0] = 0x01;
        compare_scalarmult("edge: scalar=1 on G", s, &G);
    }

    /* scalar=p-1 on G: result must be -G (wNAF produces a negative
     * starting digit). Ran base field prime p = 2^255 - 19. */
    {
        static const unsigned char P_MINUS_1[32] = {
            0xEC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F,
        };
        compare_scalarmult("edge: scalar=p-1 on G", P_MINUS_1, &G);
    }

    /* scalar=15 on G: exercises the highest positive wNAF digit at
     * start, picking table[7]=15G directly. */
    {
        unsigned char s[32] = {};
        s[0] = 15;
        compare_scalarmult("edge: scalar=15 on G", s, &G);
    }

    /* Random scalars on G. */
    for (int iter = 0; iter < 64; iter++)
    {
        unsigned char s[32];
        make_random_scalar(s, rng);

        std::string label = "scalarmult_packed_diff[scalar_rand, P=G, iter=" + std::to_string(iter) + "]";
        compare_scalarmult(label.c_str(), s, &G);
    }

    /* Random scalars on random on-curve points (P = random * G). */
    for (int iter = 0; iter < 64; iter++)
    {
        unsigned char s_pt[32];
        make_random_scalar(s_pt, rng);

        shaw_jacobian P;
        shaw_scalarmult_vartime_x64_unpacked(&P, s_pt, &G);

        unsigned char s[32];
        make_random_scalar(s, rng);

        std::string label = "scalarmult_packed_diff[scalar_rand, P_rand, iter=" + std::to_string(iter) + "]";
        compare_scalarmult(label.c_str(), s, &P);
    }

    /* Identity input point (scalarmult of identity = identity for any
     * scalar, regardless of wNAF structure). */
    {
        unsigned char s[32];
        make_random_scalar(s, rng);
        shaw_jacobian id;
        shaw_identity(&id);
        compare_scalarmult("edge: identity input, rand scalar", s, &id);
    }
#endif
}
