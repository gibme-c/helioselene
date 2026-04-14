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
 * Differential fuzz for fp_invert against an algorithmically-distinct
 * Fermat exponentiation reference (z^(p-2) via square-and-multiply on the
 * public fp_mul / fp_sq primitives). Active on every backend: it cross-
 * checks divsteps inversion (x64 and portable) against Fermat exponentiation
 * running on the same backend's field-arithmetic primitives.
 *
 * Skips z = 0 because Fermat returns 0 there but divsteps produces an
 * undefined-but-deterministic non-zero output (the inverse of zero does
 * not exist; both "answers" are conventional).
 */

/*
 * p-2 in big-endian byte order (= 2^255 - 21).
 * Top bit at position 254; low byte is 0xeb (= 0xed - 2).
 */
static const uint8_t FP_P_MINUS_2_BE[32] = {
    0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xeb,
};

static void fp_invert_fermat_reference(fp_fe out, const fp_fe z)
{
    fp_fe acc;
    fp_1(acc);

    bool top_bit_seen = false;
    for (int byte = 0; byte < 32; byte++)
    {
        uint8_t b = FP_P_MINUS_2_BE[byte];
        for (int bit = 7; bit >= 0; bit--)
        {
            if (top_bit_seen)
            {
                fp_fe tmp;
                fp_sq(tmp, acc);
                fp_copy(acc, tmp);
            }
            if ((b >> bit) & 1)
            {
                if (!top_bit_seen)
                {
                    fp_copy(acc, z);
                    top_bit_seen = true;
                }
                else
                {
                    fp_fe tmp;
                    fp_mul(tmp, acc, z);
                    fp_copy(acc, tmp);
                }
            }
        }
    }
    fp_copy(out, acc);
}

static void check_fp_diff_pair(const char *label, const fp_fe actual, const fp_fe expected)
{
    uint8_t ba[32], bb[32];
    fp_tobytes(ba, actual);
    fp_tobytes(bb, expected);
    check_bytes(label, bb, ba, 32);
}

static void check_fp_inverse_property(const char *label, const fp_fe z, const fp_fe inv, const uint8_t one_bytes[32])
{
    fp_fe prod;
    fp_mul(prod, z, inv);
    uint8_t ba[32];
    fp_tobytes(ba, prod);
    check_bytes(label, one_bytes, ba, 32);
}

void fuzz_fp_invert_diff()
{
    std::cout << std::endl << "=== Fuzz: fp_invert diff ===" << std::endl;

    xoshiro256ss rng;
    rng.seed(global_seed + 223);

    /* Precompute canonical bytes of 1 once for the inverse-property check. */
    fp_fe one;
    fp_1(one);
    uint8_t one_bytes[32];
    fp_tobytes(one_bytes, one);

    /* Edge cases: 1, p-1, 2, 3. z = 0 excluded (see file header). */
    {
        fp_fe p_minus_1, two, three;
        fp_neg(p_minus_1, one);

        uint8_t bytes_two[32] = {0};
        bytes_two[0] = 2;
        fp_frombytes(two, bytes_two);
        uint8_t bytes_three[32] = {0};
        bytes_three[0] = 3;
        fp_frombytes(three, bytes_three);

        const struct
        {
            const char *name;
            const fp_fe *value;
        } cases[] = {
            {"fp_invert(1)", &one},
            {"fp_invert(2)", &two},
            {"fp_invert(3)", &three},
            {"fp_invert(p-1)", &p_minus_1},
        };

        for (const auto &c : cases)
        {
            fp_fe got, want;
            fp_invert(got, *c.value);
            fp_invert_fermat_reference(want, *c.value);
            std::string label = std::string(c.name) + " [diff vs Fermat]";
            check_fp_diff_pair(label.c_str(), got, want);
            std::string ilabel = std::string(c.name) + " [z * z^-1 == 1]";
            check_fp_inverse_property(ilabel.c_str(), *c.value, got, one_bytes);
        }
    }

    for (int i = 0; i < 4096; i++)
    {
        uint8_t bytes[32];
        rng.fill_bytes(bytes, 32);
        bytes[31] &= 0x7F;

        fp_fe z;
        fp_frombytes(z, bytes);

        uint8_t z_bytes[32];
        fp_tobytes(z_bytes, z);
        bool is_zero = true;
        for (int k = 0; k < 32; k++)
            if (z_bytes[k] != 0)
            {
                is_zero = false;
                break;
            }
        if (is_zero)
            continue;

        fp_fe got, want;
        fp_invert(got, z);
        fp_invert_fermat_reference(want, z);

        std::string dlabel = "fp_invert_diff[" + std::to_string(i) + "]";
        check_fp_diff_pair(dlabel.c_str(), got, want);

        std::string ilabel = "fp_invert*z==1[" + std::to_string(i) + "]";
        check_fp_inverse_property(ilabel.c_str(), z, got, one_bytes);
    }
}
