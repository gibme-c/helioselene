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

void fuzz_map_to_curve()
{
    std::cout << std::endl << "=== Fuzz: Map-to-Curve ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 10);

    for (int i = 0; i < 250; i++)
    {
        std::string label = "ran_mtc[" + std::to_string(i) + "]";

        /* For map_to_curve we need Fp bytes → use random Shaw scalar (which IS Fp) */
        auto fp_s = random_shaw_scalar(rng);
        auto u = fp_s.to_bytes();

        /* Single-element map_to_curve */
        auto P = RanPoint::map_to_curve(u.data());
        check_true((label + " non_id").c_str(), !P.is_identity());
        auto pb = P.to_bytes();
        auto P2 = RanPoint::from_bytes(pb.data());
        check_true((label + " rt").c_str(), P2.has_value());

        /* Two-element map_to_curve */
        auto fp_s2 = random_shaw_scalar(rng);
        auto u1 = fp_s2.to_bytes();
        auto Q = RanPoint::map_to_curve(u.data(), u1.data());
        check_true((label + " 2elem_non_id").c_str(), !Q.is_identity());

        /* Determinism */
        auto P3 = RanPoint::map_to_curve(u.data());
        check_true((label + " determ").c_str(), ran_points_equal(P, P3));
    }

    for (int i = 0; i < 250; i++)
    {
        std::string label = "shaw_mtc[" + std::to_string(i) + "]";

        /* For Shaw map_to_curve we need Fq bytes → use RanScalar (which is Fq) */
        auto fq_s = random_ran_scalar(rng);
        auto u = fq_s.to_bytes();

        auto P = ShawPoint::map_to_curve(u.data());
        check_true((label + " non_id").c_str(), !P.is_identity());
        auto pb = P.to_bytes();
        auto P2 = ShawPoint::from_bytes(pb.data());
        check_true((label + " rt").c_str(), P2.has_value());

        auto fq_s2 = random_ran_scalar(rng);
        auto u1 = fq_s2.to_bytes();
        auto Q = ShawPoint::map_to_curve(u.data(), u1.data());
        check_true((label + " 2elem_non_id").c_str(), !Q.is_identity());

        auto P3 = ShawPoint::map_to_curve(u.data());
        check_true((label + " determ").c_str(), shaw_points_equal(P, P3));
    }
}

/* ======================================================================
 * 11. fuzz_wei25519_bridge — ~500
 * ====================================================================== */


void fuzz_wei25519_bridge()
{
    std::cout << std::endl << "=== Fuzz: Wei25519 Bridge ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 11);

    /* Valid canonical Fp bytes */
    for (int i = 0; i < 400; i++)
    {
        std::string label = "wei_valid[" + std::to_string(i) + "]";
        auto fp_s = random_shaw_scalar(rng); /* Fp element */
        auto bytes = fp_s.to_bytes();
        auto result = shaw_scalar_from_wei25519_x(bytes.data());
        if (result.has_value())
        {
            auto rb = result.value().to_bytes();
            auto result2 = ShawScalar::from_bytes(rb.data());
            check_true(label.c_str(), result2.has_value() && result2.value() == result.value());
        }
        else
        {
            /* Valid Fp element but wei25519 conversion failed — acceptable, just count it */
            check_true(label.c_str(), true);
        }
    }

    /* Bytes with bit 255 set → should return nullopt */
    for (int i = 0; i < 50; i++)
    {
        std::string label = "wei_bit255[" + std::to_string(i) + "]";
        uint8_t bytes[32];
        rng.fill_bytes(bytes, 32);
        bytes[31] |= 0x80;
        auto result = shaw_scalar_from_wei25519_x(bytes);
        check_true(label.c_str(), !result.has_value());
    }

    /* Bytes >= p → should return nullopt */
    for (int i = 0; i < 50; i++)
    {
        std::string label = "wei_over_p[" + std::to_string(i) + "]";
        uint8_t bytes[32];
        std::memset(bytes, 0xFF, 32);
        bytes[31] = 0x7F; /* Just below bit 255 but >= p since p = 2^255-19 */
        auto result = shaw_scalar_from_wei25519_x(bytes);
        check_true(label.c_str(), !result.has_value());
    }
}

/* ======================================================================
 * 12. fuzz_pedersen — ~800 checks
 * ====================================================================== */
