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

void fuzz_cross_curve_cycle()
{
    std::cout << std::endl << "=== Fuzz: Cross-Curve Cycle ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 6);

    for (int i = 0; i < 250; i++)
    {
        std::string label = "cycle[" + std::to_string(i) + "]";

        /* Ran point -> x-coord bytes (Fp element) -> Shaw scalar (Fq reinterpretation) */
        auto P = random_ran_point(rng);
        auto x_bytes = P.x_coordinate_bytes();

        /* x_bytes is a canonical Fp element; Shaw scalars are Fp elements, so from_bytes should succeed */
        auto sel_s = ShawScalar::from_bytes(x_bytes.data());
        check_true((label + " hp_to_ss").c_str(), sel_s.has_value());

        if (sel_s.has_value())
        {
            /* Use it in a Shaw operation */
            auto Q = ShawPoint::generator().scalar_mul_vartime(sel_s.value());
            check_true((label + " Q_valid").c_str(), !Q.is_identity() || sel_s.value().is_zero());

            /* Extract Q's x-coord -> Ran scalar */
            auto qx = Q.x_coordinate_bytes();
            auto hel_s = RanScalar::from_bytes(qx.data());
            /* This might fail if the Fq x-coord value >= q (Ran scalar field), but it should usually succeed */
            if (hel_s.has_value())
            {
                /* Round-trip the scalar through bytes */
                auto hb = hel_s.value().to_bytes();
                auto hel_s2 = RanScalar::from_bytes(hb.data());
                check_true((label + " hs_rt").c_str(), hel_s2.has_value() && hel_s2.value() == hel_s.value());
            }
        }

        /* Wei25519 bridge check */
        auto wei_s = shaw_scalar_from_wei25519_x(x_bytes.data());
        if (wei_s.has_value())
        {
            auto wb = wei_s.value().to_bytes();
            auto wei_s2 = ShawScalar::from_bytes(wb.data());
            check_true((label + " wei_rt").c_str(), wei_s2.has_value() && wei_s2.value() == wei_s.value());
        }
    }
}

/* ======================================================================
 * 7. fuzz_scalarmul_consistency — ~1,500 checks
 * ====================================================================== */
