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

void fuzz_verification_equation()
{
    std::cout << std::endl << "=== Fuzz: Verification Equation ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 19);

    /* Ran */
    for (int trial = 0; trial < 25; trial++)
    {
        std::string label = "ran_verify[" + std::to_string(trial) + "]";
        auto L = random_ran_point(rng);
        auto R = random_ran_point(rng);
        auto P = random_ran_point(rng);
        auto x = random_ran_scalar(rng);

        /* Scalar prerequisites */
        auto x_inv = x.invert();
        check_true((label + " x*xinv==1").c_str(), x_inv.has_value() && x * x_inv.value() == RanScalar::one());
        check_true((label + " x.sq()==x*x").c_str(), x.sq() == x * x);

        auto x2 = x.sq();
        auto xi2 = x_inv.value().sq();

        /* P' = x^2 * L + P + x^{-2} * R */
        auto Pprime = L.scalar_mul_vartime(x2) + P + R.scalar_mul_vartime(xi2);

        /* P' - x^2*L - x^{-2}*R == P */
        auto check = Pprime + (-(L.scalar_mul_vartime(x2))) + (-(R.scalar_mul_vartime(xi2)));
        check_true((label + " verify_eq").c_str(), ran_points_equal(check, P));

        /* Also with negation approach */
        auto check2 = Pprime + (-L).scalar_mul_vartime(x2) + (-R).scalar_mul_vartime(xi2);
        check_true((label + " verify_neg").c_str(), ran_points_equal(check2, P));
    }

    /* Shaw */
    for (int trial = 0; trial < 25; trial++)
    {
        std::string label = "shaw_verify[" + std::to_string(trial) + "]";
        auto L = random_shaw_point(rng);
        auto R = random_shaw_point(rng);
        auto P = random_shaw_point(rng);
        auto x = random_shaw_scalar(rng);

        auto x_inv = x.invert();
        check_true((label + " x*xinv==1").c_str(), x_inv.has_value() && x * x_inv.value() == ShawScalar::one());
        check_true((label + " x.sq()==x*x").c_str(), x.sq() == x * x);

        auto x2 = x.sq();
        auto xi2 = x_inv.value().sq();

        auto Pprime = L.scalar_mul_vartime(x2) + P + R.scalar_mul_vartime(xi2);

        auto check = Pprime + (-(L.scalar_mul_vartime(x2))) + (-(R.scalar_mul_vartime(xi2)));
        check_true((label + " verify_eq").c_str(), shaw_points_equal(check, P));

        auto check2 = Pprime + (-L).scalar_mul_vartime(x2) + (-R).scalar_mul_vartime(xi2);
        check_true((label + " verify_neg").c_str(), shaw_points_equal(check2, P));
    }
}
