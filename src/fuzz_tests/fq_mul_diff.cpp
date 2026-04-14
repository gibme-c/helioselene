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

#include "fuzz_tests/common.h"
#include "fuzz_tests/registry.h"
#include "ranshaw_platform.h"

/*
 * Differential fuzz for fq_mul and fq_sq against an algorithmically-distinct
 * C reference. Prompted by the 795ccd7 carry-propagation bug in the ADX asm
 * multi-stage Crandall fold, which a 5M-input differential caught but the
 * indirect functional tests had missed.
 *
 * Reference path: pack 5×51 → 4×64, compute via a C fallback
 * (fq64_mul_c / fq64_sq on GCC/Clang with __int128; fq64_mul_c_umul128 /
 * fq64_sq_c_umul128 on MSVC using _umul128 + _addcarry_u64), unpack,
 * post-normalize. The harness forces this path and compares against
 * whichever backend fq_mul currently dispatches to (ADX asm, 5×5
 * schoolbook, AVX2, IFMA, ...).
 *
 * Also includes a self-consistency check: fq_sq(a) == fq_mul(a, a) in the
 * active backend, catching any sq/mul divergence independent of the
 * reference.
 */
/*
 * RANSHAW_PLATFORM_64BIT guards against FORCE_PORTABLE builds where fq_fe is
 * a 10×25.5 representation rather than 5×51. In that case the 4×64 helpers
 * in <x64/fq51_inline.h> would read garbage from fq_fe[]; the harness sits
 * out cleanly instead.
 */
#if RANSHAW_PLATFORM_64BIT
#if (defined(__GNUC__) || defined(__clang__)) && defined(__BMI2__) && RANSHAW_HAVE_INT128
#define RANSHAW_FQ_MUL_DIFF_ACTIVE 1
#define RANSHAW_FQ_MUL_DIFF_REF_GNU 1
#include "x64/fq51.h"
#include "x64/fq51_inline.h"
#elif RANSHAW_HAVE_UMUL128 && !RANSHAW_HAVE_INT128
#define RANSHAW_FQ_MUL_DIFF_ACTIVE 1
#define RANSHAW_FQ_MUL_DIFF_REF_MSVC 1
#include "x64/fq51.h"
#include "x64/fq51_inline.h"
#endif
#endif /* RANSHAW_PLATFORM_64BIT */

#ifdef RANSHAW_FQ_MUL_DIFF_ACTIVE

/* Post-normalize matches fq51_mul_inline's GCC/Clang tail (fq51_inline.h
 * lines ~1210-1240): single carry chain, gamma fold, final carry chain to
 * 51-bit limbs. Kept inline here rather than factored out to keep the
 * reference self-contained and decoupled from future edits to the
 * production inline body. */
static void fq_post_normalize_5x51(fq_fe h)
{
#if RANSHAW_FQ_NATIVE64
    /* Native 4x64: the reference product is already a valid fq_fe (< 2^256).
     * Canonically reduce; the diff comparison goes through fq_tobytes anyway. */
    fq64_reduce_canonical(h, h);
#else
    const uint64_t M = FQ51_MASK;
    uint64_t c = h[0] >> 51;
    h[0] &= M;
    h[1] += c;
    c = h[1] >> 51;
    h[1] &= M;
    h[2] += c;
    c = h[2] >> 51;
    h[2] &= M;
    h[3] += c;
    c = h[3] >> 51;
    h[3] &= M;
    h[4] += c;
    c = h[4] >> 51;
    h[4] &= M;
    h[0] += c * GAMMA_51[0];
    h[1] += c * GAMMA_51[1];
    h[2] += c * GAMMA_51[2];
    c = h[0] >> 51;
    h[0] &= M;
    h[1] += c;
    c = h[1] >> 51;
    h[1] &= M;
    h[2] += c;
    c = h[2] >> 51;
    h[2] &= M;
    h[3] += c;
    c = h[3] >> 51;
    h[3] &= M;
    h[4] += c;
#endif
}

static void fq_mul_ref(fq_fe h, const fq_fe a, const fq_fe b)
{
    uint64_t pa[4], pb[4], out[4];
    fq51_normalize_and_pack(pa, a);
    fq51_normalize_and_pack(pb, b);
#ifdef RANSHAW_FQ_MUL_DIFF_REF_GNU
    fq64_mul_c(out, pa, pb);
#else /* RANSHAW_FQ_MUL_DIFF_REF_MSVC */
    fq64_mul_c_umul128(out, pa, pb);
#endif
    fq64_to_fq51(h, out);
    fq_post_normalize_5x51(h);
}

static void fq_sq_ref(fq_fe h, const fq_fe a)
{
    uint64_t pa[4], out[4];
    fq51_normalize_and_pack(pa, a);
#ifdef RANSHAW_FQ_MUL_DIFF_REF_GNU
    /* Mul-based squaring on GCC/Clang: fq64_mul_c(a,a) provides an
     * algorithmically-independent check against fq_sq, which dispatches
     * to fq64_sq (asm if __ADX__, else the __int128 C fallback). */
    fq64_mul_c(out, pa, pa);
#else /* RANSHAW_FQ_MUL_DIFF_REF_MSVC */
    /* Direct sq primitive on MSVC: exercises fq64_sq_c_umul128 before
     * Unit 4 wires it into fq_sq. At Unit 3 stage, active fq_sq still
     * uses the 5×5 MSVC schoolbook, so this is a real cross-algorithm
     * diff (5-limb radix-2^51 vs 4-limb radix-2^64). */
    fq64_sq_c_umul128(out, pa);
#endif
    fq64_to_fq51(h, out);
    fq_post_normalize_5x51(h);
}

static void check_diff_pair(const char *label, const fq_fe actual, const fq_fe expected)
{
    uint8_t ba[32], bb[32];
    fq_tobytes(ba, actual);
    fq_tobytes(bb, expected);
    check_bytes(label, bb, ba, 32);
}

#endif /* RANSHAW_FQ_MUL_DIFF_ACTIVE */

void fuzz_fq_mul_diff()
{
    std::cout << std::endl << "=== Fuzz: fq_mul / fq_sq diff ===" << std::endl;

#ifndef RANSHAW_FQ_MUL_DIFF_ACTIVE
    std::cout << "(skipped: no independent fq_mul C reference on this toolchain)" << std::endl;
    return;
#else
    xoshiro256ss rng;
    rng.seed(global_seed + 117);

    /* Edge cases: 0, 1, and the (q-1) = -1 boundary. */
    {
        fq_fe zero, one, q_minus_1, a, b;
        fq_0(zero);
        fq_1(one);
        fq_sub(q_minus_1, zero, one);

        fq_mul(a, zero, zero);
        fq_mul_ref(b, zero, zero);
        check_diff_pair("fq_mul(0, 0)", a, b);

        fq_mul(a, one, zero);
        fq_mul_ref(b, one, zero);
        check_diff_pair("fq_mul(1, 0)", a, b);

        fq_mul(a, one, one);
        fq_mul_ref(b, one, one);
        check_diff_pair("fq_mul(1, 1)", a, b);

        fq_mul(a, q_minus_1, q_minus_1);
        fq_mul_ref(b, q_minus_1, q_minus_1);
        check_diff_pair("fq_mul(q-1, q-1)", a, b);

        fq_sq(a, zero);
        fq_sq_ref(b, zero);
        check_diff_pair("fq_sq(0)", a, b);

        fq_sq(a, one);
        fq_sq_ref(b, one);
        check_diff_pair("fq_sq(1)", a, b);

        fq_sq(a, q_minus_1);
        fq_sq_ref(b, q_minus_1);
        check_diff_pair("fq_sq(q-1)", a, b);
    }

    /* 4096 random canonical-input differentials. Budget is generous: even at
     * 4k iterations this finishes in well under a second on all backends,
     * and the coverage lift over the prior indirect tests (which only hit
     * fq_mul through curve operations and scalarmults) is significant. */
    for (int i = 0; i < 4096; i++)
    {
        uint8_t ba[32], bb[32];
        rng.fill_bytes(ba, 32);
        ba[31] &= 0x7F;
        rng.fill_bytes(bb, 32);
        bb[31] &= 0x7F;

        fq_fe a, b, got, want;
        fq_frombytes(a, ba);
        fq_frombytes(b, bb);

        fq_mul(got, a, b);
        fq_mul_ref(want, a, b);
        std::string mlabel = "fq_mul_diff[" + std::to_string(i) + "]";
        check_diff_pair(mlabel.c_str(), got, want);

        fq_sq(got, a);
        fq_sq_ref(want, a);
        std::string slabel = "fq_sq_diff[" + std::to_string(i) + "]";
        check_diff_pair(slabel.c_str(), got, want);

        /* Self-consistency: fq_sq(a) must equal fq_mul(a, a) in the active
         * backend. Catches any sq/mul path divergence regardless of what
         * the reference implementation looks like. */
        fq_fe via_mul;
        fq_mul(via_mul, a, a);
        std::string xlabel = "fq_sq==mul(a,a)[" + std::to_string(i) + "]";
        check_diff_pair(xlabel.c_str(), got, via_mul);
    }
#endif
}
