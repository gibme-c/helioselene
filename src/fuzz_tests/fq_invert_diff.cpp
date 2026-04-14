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

/*
 * Differential fuzz for fq_invert against an algorithmically-distinct
 * Fermat exponentiation reference (z^(q-2) via square-and-multiply on the
 * public fq_mul / fq_sq primitives). Active on every backend: it cross-
 * checks divsteps inversion (x64 and portable) against Fermat exponentiation
 * running on the same backend's field-arithmetic primitives.
 *
 * Skips z = 0 because Fermat returns z^(q-2) = 0 there but the divsteps
 * algorithm produces an undefined-but-deterministic non-zero output (the
 * inverse of zero does not exist; both "answers" are conventional). The
 * self-consistency check (fq_mul(z, fq_invert(z)) == 1) is also gated on
 * z != 0 for the same reason.
 */

/*
 * q-2 in big-endian byte order. Used by the Fermat reference's
 * square-and-multiply scan from the most-significant set bit downward.
 *
 * q = 2^255 - gamma; q-2 high half is 2^127 - 1 (all ones), low half is
 * gamma's two's-complement minus 2.
 */
static const uint8_t FQ_Q_MINUS_2_BE[32] = {
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x4b, 0xb1, 0xeb, 0x0e, 0x39, 0x73, 0x0f, 0xa7, 0x71, 0x68, 0x46, 0x45, 0xec, 0x70, 0xf8, 0x5d,
};

static void fq_invert_fermat_reference(fq_fe out, const fq_fe z)
{
    /*
     * Square-and-multiply z^(q-2) using only the public fq_mul / fq_sq.
     * Algorithmically independent from divsteps (Fermat exponent vs
     * extended-GCD): any divergence between them indicates a real bug in
     * one path, not in the helpers they share.
     */
    fq_fe acc;
    /* Start with acc = 1; first set bit promotes it to z. */
    fq_1(acc);

    bool top_bit_seen = false;
    for (int byte = 0; byte < 32; byte++)
    {
        uint8_t b = FQ_Q_MINUS_2_BE[byte];
        for (int bit = 7; bit >= 0; bit--)
        {
            if (top_bit_seen)
            {
                fq_fe tmp;
                fq_sq(tmp, acc);
                fq_copy(acc, tmp);
            }
            if ((b >> bit) & 1)
            {
                if (!top_bit_seen)
                {
                    fq_copy(acc, z);
                    top_bit_seen = true;
                }
                else
                {
                    fq_fe tmp;
                    fq_mul(tmp, acc, z);
                    fq_copy(acc, tmp);
                }
            }
        }
    }
    fq_copy(out, acc);
}

static void check_fq_diff_pair(const char *label, const fq_fe actual, const fq_fe expected)
{
    uint8_t ba[32], bb[32];
    fq_tobytes(ba, actual);
    fq_tobytes(bb, expected);
    check_bytes(label, bb, ba, 32);
}

static void check_fq_inverse_property(const char *label, const fq_fe z, const fq_fe inv, const uint8_t one_bytes[32])
{
    /* z * z^{-1} == 1 in F_q. */
    fq_fe prod;
    fq_mul(prod, z, inv);
    uint8_t ba[32];
    fq_tobytes(ba, prod);
    check_bytes(label, one_bytes, ba, 32);
}

void fuzz_fq_invert_diff()
{
    std::cout << std::endl << "=== Fuzz: fq_invert diff ===" << std::endl;

    xoshiro256ss rng;
    rng.seed(global_seed + 211);

    /* Precompute canonical bytes of 1 once for the inverse-property check. */
    fq_fe one;
    fq_1(one);
    uint8_t one_bytes[32];
    fq_tobytes(one_bytes, one);

    /* Edge cases: 1, q-1, gamma, and a few small values. z = 0 is excluded
     * because Fermat returns 0 while divsteps produces an undefined output. */
    {
        fq_fe q_minus_1, two, three;
        fq_neg(q_minus_1, one);

        /* Build two and three via byte deserialization to keep dependency on
         * field-add machinery out of the test setup. */
        uint8_t bytes_two[32] = {0};
        bytes_two[0] = 2;
        fq_frombytes(two, bytes_two);
        uint8_t bytes_three[32] = {0};
        bytes_three[0] = 3;
        fq_frombytes(three, bytes_three);

        const struct
        {
            const char *name;
            const fq_fe *value;
        } cases[] = {
            {"fq_invert(1)", &one},
            {"fq_invert(2)", &two},
            {"fq_invert(3)", &three},
            {"fq_invert(q-1)", &q_minus_1},
        };

        for (const auto &c : cases)
        {
            fq_fe got, want;
            fq_invert(got, *c.value);
            fq_invert_fermat_reference(want, *c.value);
            std::string label = std::string(c.name) + " [diff vs Fermat]";
            check_fq_diff_pair(label.c_str(), got, want);
            std::string ilabel = std::string(c.name) + " [z * z^-1 == 1]";
            check_fq_inverse_property(ilabel.c_str(), *c.value, got, one_bytes);
        }
    }

    /*
     * 4096 random canonical-input differentials. Budget mirrors fq_mul_diff:
     * even at 4k iterations the run finishes in well under a second on the
     * x64 backends (and a few seconds on portable, which does ~750 divsteps
     * + ~510 Fermat field ops per input).
     */
    for (int i = 0; i < 4096; i++)
    {
        uint8_t bytes[32];
        rng.fill_bytes(bytes, 32);
        bytes[31] &= 0x7F; /* clamp to 255 bits before frombytes */

        fq_fe z;
        fq_frombytes(z, bytes);

        /* Skip z that canonicalize to 0 (extremely unlikely with random
         * input but defended against for completeness). */
        uint8_t z_bytes[32];
        fq_tobytes(z_bytes, z);
        bool is_zero = true;
        for (int k = 0; k < 32; k++)
            if (z_bytes[k] != 0)
            {
                is_zero = false;
                break;
            }
        if (is_zero)
            continue;

        fq_fe got, want;
        fq_invert(got, z);
        fq_invert_fermat_reference(want, z);

        std::string dlabel = "fq_invert_diff[" + std::to_string(i) + "]";
        check_fq_diff_pair(dlabel.c_str(), got, want);

        std::string ilabel = "fq_invert*z==1[" + std::to_string(i) + "]";
        check_fq_inverse_property(ilabel.c_str(), z, got, one_bytes);
    }
}
