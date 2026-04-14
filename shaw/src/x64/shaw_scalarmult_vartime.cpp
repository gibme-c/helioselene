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
 * @file x64/shaw_scalarmult_vartime.cpp
 * @brief x64 variable-time scalar multiplication for Shaw using wNAF w=5.
 *
 * Under FQ51_HAVE_ADX_MUL, hold the 8-entry precomputed odd-multiples
 * table and the running accumulator in 4x64 packed form across the
 * whole wNAF main loop; pack the input point once at entry, unpack the
 * final result once at exit. Falls back to the 5x51 unpacked reference
 * on MSVC / non-BMI2 builds. The unpacked body is also always compiled
 * and exposed as shaw_scalarmult_vartime_x64_unpacked for differential
 * fuzz.
 */

#include "shaw_scalarmult_vartime.h"

#include "fq_ops.h"
#include "fq_utils.h"
#include "ranshaw_secure_erase.h"
#include "shaw.h"
#include "shaw_add.h"
#include "shaw_dbl.h"
#include "shaw_madd.h"
#include "shaw_ops.h"
#include "x64/shaw_inner_packed.h"

#include <cstdint>
#include <cstring>

/*
 * Signed 5-bit wNAF recoding of a 256-bit scalar. Returns the number of
 * valid digits in naf[] (the highest 1 + position that was written).
 *
 * naf[i] is in {-15, -13, ..., -1, 0, +1, +3, ..., +15}. Nonzero entries
 * are spaced at least 5 positions apart so that each can be applied as
 * a single table lookup after an intervening run of doublings.
 */
static int wnaf_encode(int8_t naf[257], const unsigned char scalar[32])
{
    uint32_t bits[9] = {0};
    for (int i = 0; i < 32; i++)
        bits[i / 4] |= (uint32_t)scalar[i] << ((i % 4) * 8);

    int pos = 0;
    int highest = 0;

    for (int i = 0; i <= 256; i++)
        naf[i] = 0;

    while (pos <= 256)
    {
        if (!((bits[pos / 32] >> (pos % 32)) & 1))
        {
            pos++;
            continue;
        }

        int word_idx = pos / 32;
        int bit_idx = pos % 32;
        int32_t val = (int32_t)((bits[word_idx] >> bit_idx) & 0x1f);
        if (bit_idx > 27 && word_idx + 1 < 9)
            val |= (int32_t)((bits[word_idx + 1] << (32 - bit_idx)) & 0x1f);

        if (val > 16)
            val -= 32;

        naf[pos] = (int8_t)val;
        highest = pos + 1;

        {
            int wi = pos / 32;
            int bi = pos % 32;
            if (val > 0)
            {
                uint64_t sub = (uint64_t)(uint32_t)val << bi;
                uint32_t borrow = 0;
                for (int k = wi; k < 9 && (sub || borrow); k++)
                {
                    uint64_t lo = (k == wi) ? (sub & 0xffffffffULL) : ((k == wi + 1) ? (sub >> 32) : 0);
                    lo += borrow;
                    borrow = (bits[k] < lo) ? 1 : 0;
                    bits[k] -= (uint32_t)lo;
                }
            }
            else
            {
                uint64_t add = (uint64_t)(uint32_t)(-val) << bi;
                uint32_t carry = 0;
                for (int k = wi; k < 9 && (add || carry); k++)
                {
                    uint64_t lo = (k == wi) ? (add & 0xffffffffULL) : ((k == wi + 1) ? (add >> 32) : 0);
                    uint64_t sum = (uint64_t)bits[k] + lo + carry;
                    bits[k] = (uint32_t)sum;
                    carry = (uint32_t)(sum >> 32);
                }
            }
        }

        pos += 5;
    }

    ranshaw_secure_erase(bits, sizeof(bits));
    return highest;
}

// ============================================================================
// Unpacked 5x51 wNAF (fallback on MSVC / non-BMI2; also used verbatim by the
// shaw_scalarmult_vartime_x64_unpacked test hook).
// ============================================================================

static void scalarmult_vartime_unpacked(shaw_jacobian *r, const unsigned char scalar[32], const shaw_jacobian *p)
{
    shaw_jacobian table[8];
    shaw_jacobian p2;

    shaw_copy(&table[0], p);
    shaw_dbl(&p2, p);

    for (int i = 1; i < 8; i++)
        shaw_add(&table[i], &table[i - 1], &p2);

    int8_t naf[257];
    int top = wnaf_encode(naf, scalar);

    if (top == 0)
    {
        ranshaw_secure_erase(naf, sizeof(naf));
        ranshaw_secure_erase(table, sizeof(table));
        ranshaw_secure_erase(&p2, sizeof(p2));
        shaw_identity(r);
        return;
    }

    int start = top - 1;
    while (start >= 0 && naf[start] == 0)
        start--;

    if (start < 0)
    {
        ranshaw_secure_erase(naf, sizeof(naf));
        ranshaw_secure_erase(table, sizeof(table));
        ranshaw_secure_erase(&p2, sizeof(p2));
        shaw_identity(r);
        return;
    }

    int8_t d = naf[start];
    int idx = ((d < 0) ? -d : d) / 2;
    shaw_copy(r, &table[idx]);
    if (d < 0)
        shaw_neg(r, r);

    for (int i = start - 1; i >= 0; i--)
    {
        shaw_dbl(r, r);

        if (naf[i] != 0)
        {
            d = naf[i];
            idx = ((d < 0) ? -d : d) / 2;
            if (d > 0)
                shaw_add(r, r, &table[idx]);
            else
            {
                shaw_jacobian neg_pt;
                shaw_neg(&neg_pt, &table[idx]);
                shaw_add(r, r, &neg_pt);
            }
        }
    }

    ranshaw_secure_erase(naf, sizeof(naf));
    ranshaw_secure_erase(table, sizeof(table));
    ranshaw_secure_erase(&p2, sizeof(p2));
}

// ============================================================================
// Packed 4x64 wNAF (GCC/Clang BMI2+ADX only)
// ============================================================================

#if defined(FQ51_HAVE_ADX_MUL)

namespace
{

    static inline void unpack_jac(shaw_jacobian *out, const packed_jac *in)
    {
        unpack_and_normalize(out->X, in->X);
        unpack_and_normalize(out->Y, in->Y);
        unpack_and_normalize(out->Z, in->Z);
    }

    static void scalarmult_vartime_packed(shaw_jacobian *r, const unsigned char scalar[32], const shaw_jacobian *p)
    {
        /* Packed precomputed table of odd multiples: table[k] = (2k+1)*P
         * for k=0..7, i.e. {P, 3P, 5P, 7P, 9P, 11P, 13P, 15P}. Built as
         * table[0]=P, p2=2P, table[k]=table[k-1]+p2. On a prime-order curve
         * each intermediate sum is between non-identity distinct points
         * (the only torsion is identity itself), so the raw incomplete
         * shaw_add_x64_packed is safe for construction. If P is identity
         * the raw formula returns identity for every operand combination;
         * the main loop's shaw_add_safe_packed then handles it. */
        packed_jac table[8];
        packed_jac p2;

        pack_jac(&table[0], p);
        shaw_dbl_x64_packed(p2.X, p2.Y, p2.Z, table[0].X, table[0].Y, table[0].Z);

        for (int i = 1; i < 8; i++)
        {
            shaw_add_x64_packed(
                table[i].X, table[i].Y, table[i].Z, table[i - 1].X, table[i - 1].Y, table[i - 1].Z, p2.X, p2.Y, p2.Z);
        }

        int8_t naf[257];
        int top = wnaf_encode(naf, scalar);

        if (top == 0)
        {
            ranshaw_secure_erase(naf, sizeof(naf));
            ranshaw_secure_erase(table, sizeof(table));
            ranshaw_secure_erase(&p2, sizeof(p2));
            shaw_identity(r);
            return;
        }

        int start = top - 1;
        while (start >= 0 && naf[start] == 0)
            start--;

        if (start < 0)
        {
            ranshaw_secure_erase(naf, sizeof(naf));
            ranshaw_secure_erase(table, sizeof(table));
            ranshaw_secure_erase(&p2, sizeof(p2));
            shaw_identity(r);
            return;
        }

        packed_jac acc;
        bool acc_is_identity = false;

        int8_t d = naf[start];
        int idx = ((d < 0) ? -d : d) / 2;
        if (d > 0)
        {
            copy_packed(&acc, &table[idx]);
        }
        else
        {
            shaw_jac_neg_x64_packed(acc.X, acc.Y, acc.Z, table[idx].X, table[idx].Y, table[idx].Z);
        }
        /* If the starting table entry is identity (only when input P is
         * identity), propagate the flag so the shaw_add_safe_packed calls in
         * the main loop take the identity shortcut. */
        if (is_identity_packed(&acc))
            acc_is_identity = true;

        for (int i = start - 1; i >= 0; i--)
        {
            if (!acc_is_identity)
                shaw_dbl_x64_packed(acc.X, acc.Y, acc.Z, acc.X, acc.Y, acc.Z);

            if (naf[i] != 0)
            {
                d = naf[i];
                idx = ((d < 0) ? -d : d) / 2;

                packed_jac pt;
                if (d > 0)
                {
                    copy_packed(&pt, &table[idx]);
                }
                else
                {
                    shaw_jac_neg_x64_packed(pt.X, pt.Y, pt.Z, table[idx].X, table[idx].Y, table[idx].Z);
                }

                if (acc_is_identity)
                {
                    copy_packed(&acc, &pt);
                    acc_is_identity = is_identity_packed(&acc);
                }
                else
                {
                    int is_id = shaw_add_safe_packed(&acc, &acc, &pt);
                    if (is_id)
                        acc_is_identity = true;
                }
            }
        }

        ranshaw_secure_erase(naf, sizeof(naf));
        ranshaw_secure_erase(table, sizeof(table));
        ranshaw_secure_erase(&p2, sizeof(p2));

        if (acc_is_identity)
            shaw_identity(r);
        else
            unpack_jac(r, &acc);
    }

} /* anonymous namespace */

#endif /* FQ51_HAVE_ADX_MUL */

// ============================================================================
// Public API (x64) + test-internal unpacked entry point
// ============================================================================

/*
 * Test-internal entry point that always runs the 5x51 unpacked path,
 * regardless of FQ51_HAVE_ADX_MUL. Used by
 * fuzz_scalarmult_vartime_packed_diff to differentially validate the
 * packed rewrite. On MSVC / non-BMI2 builds it coincides with
 * shaw_scalarmult_vartime_x64. Declared extern "C" so the fuzz harness
 * can reach it without pulling in a C++ header.
 */
extern "C" void
    shaw_scalarmult_vartime_x64_unpacked(shaw_jacobian *r, const unsigned char scalar[32], const shaw_jacobian *p)
{
    scalarmult_vartime_unpacked(r, scalar, p);
}

void shaw_scalarmult_vartime_x64(shaw_jacobian *r, const unsigned char scalar[32], const shaw_jacobian *p)
{
#if defined(FQ51_HAVE_ADX_MUL)
    scalarmult_vartime_packed(r, scalar, p);
#else
    scalarmult_vartime_unpacked(r, scalar, p);
#endif
}
