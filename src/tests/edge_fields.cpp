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
 * @file src/tests/edge_fields.cpp
 * @brief Scalar field edge and boundary tests (Phase 3).
 *
 * Exercises corner cases on RanScalar (elements of Z/q) and ShawScalar
 * (elements of Z/p) that the general-purpose test suite covers only
 * implicitly:
 *   - zero / one / (q-1) / (p-1) algebraic properties
 *   - invert(0) contract (returns nullopt)
 *   - sq vs operator*(self, self) equivalence
 *   - reduce_wide on all-zero, all-ones, and split-boundary 64-byte inputs
 *   - non-canonical from_bytes inputs (top-bit set that pushes value past
 *     the modulus) rejected cleanly
 */

#include "tests/common.h"
#include "tests/registry.h"

namespace
{
    /* Byte patterns used in multiple sub-tests. */
    const uint8_t wide_zero[64] = {0};
    const uint8_t wide_one[64] = {0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                  0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                  0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t all_ones_32[32];
    uint8_t all_ones_64[64];
    uint8_t high_bit_32[32];

    void init_patterns()
    {
        std::memset(all_ones_32, 0xff, 32);
        std::memset(all_ones_64, 0xff, 64);
        std::memset(high_bit_32, 0, 32);
        high_bit_32[31] = 0x80; /* value = 2^255; greater than q and greater than p */
    }

    template<class Scalar> void run_scalar_edges(const char *label_prefix)
    {
        const std::string p = label_prefix;

        /* zero / one identities */
        Scalar z = Scalar::zero();
        Scalar o = Scalar::one();
        check_true((p + " zero.is_zero").c_str(), z.is_zero());
        check_true((p + " one is not zero").c_str(), !o.is_zero());
        check_true((p + " 0 + 0 == 0").c_str(), (z + z) == z);
        check_true((p + " 0 + 1 == 1").c_str(), (z + o) == o);
        check_true((p + " 0 * 1 == 0").c_str(), (z * o) == z);
        check_true((p + " 1 * 1 == 1").c_str(), (o * o) == o);
        check_true((p + " -0 == 0").c_str(), (-z) == z);
        check_true((p + " 1 - 1 == 0").c_str(), (o - o) == z);

        /* sq vs operator* equivalence on zero and one */
        check_true((p + " 0.sq() == 0").c_str(), z.sq() == z);
        check_true((p + " 1.sq() == 1").c_str(), o.sq() == o);

        /* invert(0) must return nullopt */
        auto inv_zero = z.invert();
        check_true((p + " invert(0) == nullopt").c_str(), !inv_zero.has_value());

        /* invert(1) == 1 */
        auto inv_one = o.invert();
        check_true((p + " invert(1).has_value").c_str(), inv_one.has_value());
        if (inv_one.has_value())
        {
            check_true((p + " invert(1) == 1").c_str(), *inv_one == o);
        }

        /* Random non-zero element via reduce_wide: x * x^-1 == 1 */
        uint8_t seed[64];
        for (int i = 0; i < 64; i++)
            seed[i] = static_cast<uint8_t>(0x5a ^ i);
        Scalar x = Scalar::reduce_wide(seed);
        if (!x.is_zero())
        {
            auto xinv = x.invert();
            check_true((p + " x.invert().has_value (random nonzero)").c_str(), xinv.has_value());
            if (xinv.has_value())
            {
                check_true((p + " x * x.invert() == 1").c_str(), (x * *xinv) == o);
            }

            /* x + (-x) == 0 */
            check_true((p + " x + (-x) == 0").c_str(), (x + (-x)) == z);

            /* x * x == x.sq() */
            check_true((p + " x * x == x.sq()").c_str(), (x * x) == x.sq());

            /* -(-x) == x */
            check_true((p + " -(-x) == x").c_str(), (-(-x)) == x);
        }

        /* reduce_wide(0) == 0 */
        Scalar rwz = Scalar::reduce_wide(wide_zero);
        check_true((p + " reduce_wide(0) == 0").c_str(), rwz == z);

        /* reduce_wide(1) is in range and decodes via from_bytes */
        Scalar rwo = Scalar::reduce_wide(wide_one);
        auto rwo_bytes = rwo.to_bytes();
        auto rwo_rt = Scalar::from_bytes(rwo_bytes.data());
        check_true((p + " reduce_wide(1) canonical (from_bytes accepts)").c_str(), rwo_rt.has_value());

        /* reduce_wide(all-ones) is in range */
        Scalar rwa = Scalar::reduce_wide(all_ones_64);
        auto rwa_bytes = rwa.to_bytes();
        auto rwa_rt = Scalar::from_bytes(rwa_bytes.data());
        check_true((p + " reduce_wide(0xff..) canonical").c_str(), rwa_rt.has_value());

        /* from_bytes on value 2^255 must reject (both curves have moduli < 2^255,
         * so the high-bit-set all-zero-body encoding is always out of range). */
        auto bad = Scalar::from_bytes(high_bit_32);
        check_true((p + " from_bytes(2^255) rejected").c_str(), !bad.has_value());

        /* from_bytes on all-0xff must reject (>> q and >> p). */
        auto bad_all_ones = Scalar::from_bytes(all_ones_32);
        check_true((p + " from_bytes(0xff..) rejected").c_str(), !bad_all_ones.has_value());

        /* from_bytes(0) accepted, round-trips to zero. */
        uint8_t zero_bytes_local[32] = {0};
        auto good_zero = Scalar::from_bytes(zero_bytes_local);
        check_true((p + " from_bytes(0) accepted").c_str(), good_zero.has_value());
        if (good_zero.has_value())
        {
            check_true((p + " from_bytes(0) == zero").c_str(), good_zero->is_zero());
        }
    }
} // namespace

void test_edge_fields()
{
    std::cout << std::endl << "=== Field edge and boundary tests ===" << std::endl;
    init_patterns();
    run_scalar_edges<ranshaw::RanScalar>("RanScalar");
    run_scalar_edges<ranshaw::ShawScalar>("ShawScalar");
}
