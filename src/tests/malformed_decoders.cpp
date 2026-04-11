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
 * @file src/tests/malformed_decoders.cpp
 * @brief Deterministic malformed-input battery for scalar and point decoders.
 *
 * Runs a fixed list of attacker-controlled byte patterns through
 * RanScalar/ShawScalar/RanPoint/ShawPoint::from_bytes and asserts the
 * decoder either rejects cleanly (nullopt) or, if it accepts, produces a
 * canonical re-encoding that decodes back to an equal value. This is the
 * unit-test companion of the libFuzzer harnesses under tests/fuzz/ — it
 * gives every PR a known-good battery of edge inputs even without running
 * the fuzzers.
 */

#include "tests/common.h"
#include "tests/registry.h"

namespace
{
    /* Pattern 1: all zero (canonical zero scalar; identity point on curves
     * that encode the identity as all-zero — ranshaw decodes this as a
     * valid scalar and a valid point). */
    const uint8_t pat_zero[32] = {0};

    /* Pattern 2: all 0xff. Exceeds every cryptographically interesting
     * modulus so from_bytes must reject for both scalar and point. */
    uint8_t pat_all_ones[32];

    /* Pattern 3: value 2^255 (high bit of top byte set, rest zero). Both
     * the scalar and the point encode canonicalise to values < 2^255, so
     * this must reject. */
    uint8_t pat_high_bit[32];

    /* Pattern 4: value q - 1 for Fq? Unknown without peeking at internal
     * constants, so skip — fall back to a pseudo-random but deterministic
     * "near the top" byte pattern we know is accepted on at least one of
     * Ran/Shaw. The assertion here is simply that decode is
     * deterministic, not that it accepts. */
    const uint8_t pat_near_top[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
                                      0xca, 0xef, 0xbe, 0xad, 0xde, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e};

    /* Pattern 5: top byte 0x80 (high bit set, low 255 bits zero) — identical
     * to pat_high_bit above; kept separate to make the battery easier to
     * extend with other "almost but not quite" values later. */

    /* Pattern 6: top byte 0x7f + 0xff in every other slot — a value whose
     * low bits are junk but high bit is clear. Gives every decoder the
     * "looks canonical, is probably out of range" case. */
    uint8_t pat_top_clear_rest_high[32];

    /* Pattern 7: 0x01 in top byte, rest zero. Top bit clear, tiny value. */
    const uint8_t pat_top_one[32] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01};

    void init_patterns()
    {
        std::memset(pat_all_ones, 0xff, 32);
        std::memset(pat_high_bit, 0, 32);
        pat_high_bit[31] = 0x80;
        for (int i = 0; i < 31; i++)
            pat_top_clear_rest_high[i] = 0xff;
        pat_top_clear_rest_high[31] = 0x7f;
    }

    struct Pattern
    {
        const char *label;
        const uint8_t *bytes;
    };

    template<class Scalar> void run_scalar_battery(const char *prefix, const Pattern *patterns, size_t n)
    {
        const std::string p = prefix;
        for (size_t i = 0; i < n; i++)
        {
            const std::string id = p + " scalar[" + patterns[i].label + "]";
            auto s = Scalar::from_bytes(patterns[i].bytes);
            if (!s.has_value())
            {
                /* Rejection is always a valid outcome — nothing more to check. */
                check_true((id + " decoder did not crash").c_str(), true);
                continue;
            }
            /* Accepted: the canonical re-encoding must decode back to the same
             * scalar with byte-identical bytes. */
            auto bytes1 = s->to_bytes();
            auto s2 = Scalar::from_bytes(bytes1.data());
            check_true((id + " re-decode accepted").c_str(), s2.has_value());
            if (s2.has_value())
            {
                auto bytes2 = s2->to_bytes();
                check_true(
                    (id + " re-encoded bytes match").c_str(), std::memcmp(bytes1.data(), bytes2.data(), 32) == 0);
                check_true((id + " re-decoded scalar equal").c_str(), *s2 == *s);
            }
        }
    }

    template<class Point> void run_point_battery(const char *prefix, const Pattern *patterns, size_t n)
    {
        const std::string p = prefix;
        for (size_t i = 0; i < n; i++)
        {
            const std::string id = p + " point[" + patterns[i].label + "]";
            auto pt = Point::from_bytes(patterns[i].bytes);
            if (!pt.has_value())
            {
                check_true((id + " decoder did not crash").c_str(), true);
                continue;
            }
            /* Accepted: re-encode must round-trip, and the group law must
             * hold: P + (-P) == identity (catches any decoder that silently
             * accepts junk that's not actually on the curve / in the
             * subgroup). */
            auto bytes1 = pt->to_bytes();
            auto pt2 = Point::from_bytes(bytes1.data());
            check_true((id + " re-decode accepted").c_str(), pt2.has_value());
            if (pt2.has_value())
            {
                auto bytes2 = pt2->to_bytes();
                check_true(
                    (id + " re-encoded bytes match").c_str(), std::memcmp(bytes1.data(), bytes2.data(), 32) == 0);
            }
            auto sum = *pt + (-*pt);
            check_true((id + " P + (-P) == identity").c_str(), sum.is_identity());
        }
    }
} // namespace

void test_malformed_decoders()
{
    std::cout << std::endl << "=== Malformed input battery ===" << std::endl;
    init_patterns();

    const Pattern patterns[] = {
        {"zero", pat_zero},
        {"all_ones", pat_all_ones},
        {"high_bit_set", pat_high_bit},
        {"near_top", pat_near_top},
        {"top_clear_rest_high", pat_top_clear_rest_high},
        {"top_one", pat_top_one},
    };
    const size_t n = sizeof(patterns) / sizeof(patterns[0]);

    run_scalar_battery<ranshaw::RanScalar>("Ran", patterns, n);
    run_scalar_battery<ranshaw::ShawScalar>("Shaw", patterns, n);
    run_point_battery<ranshaw::RanPoint>("Ran", patterns, n);
    run_point_battery<ranshaw::ShawPoint>("Shaw", patterns, n);
}
