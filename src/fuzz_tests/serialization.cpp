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

void fuzz_serialization_roundtrip()
{
    std::cout << std::endl << "=== Fuzz: Serialization Round-trip ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 5);

    for (int i = 0; i < 250; i++)
    {
        std::string label = "ran_ser[" + std::to_string(i) + "]";

        /* Point round-trip */
        auto P = random_ran_point(rng);
        auto pb = P.to_bytes();
        auto P2 = RanPoint::from_bytes(pb.data());
        check_true((label + " pt_rt").c_str(), P2.has_value());
        if (P2.has_value())
        {
            auto pb2 = P2.value().to_bytes();
            check_bytes((label + " pt_bytes").c_str(), pb.data(), pb2.data(), 32);
        }

        /* Scalar round-trip */
        auto s = random_ran_scalar(rng);
        auto sb = s.to_bytes();
        auto s2 = RanScalar::from_bytes(sb.data());
        check_true((label + " sc_rt").c_str(), s2.has_value() && s2.value() == s);

        /* x_coordinate_bytes bit 255 clear */
        auto xb = P.x_coordinate_bytes();
        check_true((label + " x_bit255").c_str(), (xb[31] & 0x80) == 0);
    }

    /* Identity serialization: to_bytes produces all-zeros, from_bytes rejects it (not on-curve) */
    {
        auto I = RanPoint::identity();
        auto ib = I.to_bytes();
        unsigned char zero32[32] = {0};
        check_bytes("ran identity_bytes", zero32, ib.data(), 32);
    }

    for (int i = 0; i < 250; i++)
    {
        std::string label = "shaw_ser[" + std::to_string(i) + "]";

        auto P = random_shaw_point(rng);
        auto pb = P.to_bytes();
        auto P2 = ShawPoint::from_bytes(pb.data());
        check_true((label + " pt_rt").c_str(), P2.has_value());
        if (P2.has_value())
        {
            auto pb2 = P2.value().to_bytes();
            check_bytes((label + " pt_bytes").c_str(), pb.data(), pb2.data(), 32);
        }

        auto s = random_shaw_scalar(rng);
        auto sb = s.to_bytes();
        auto s2 = ShawScalar::from_bytes(sb.data());
        check_true((label + " sc_rt").c_str(), s2.has_value() && s2.value() == s);

        auto xb = P.x_coordinate_bytes();
        check_true((label + " x_bit255").c_str(), (xb[31] & 0x80) == 0);
    }

    {
        auto I = ShawPoint::identity();
        auto ib = I.to_bytes();
        unsigned char zero32[32] = {0};
        check_bytes("shaw identity_bytes", zero32, ib.data(), 32);
    }
}

/* ======================================================================
 * 6. fuzz_cross_curve_cycle — ~1,000 checks
 * ====================================================================== */
