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

void fuzz_point_arithmetic()
{
    std::cout << std::endl << "=== Fuzz: Point Arithmetic ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 3);

    for (int i = 0; i < 250; i++)
    {
        std::string label = "ran_pt[" + std::to_string(i) + "]";

        auto P = random_ran_point(rng);
        auto Q = random_ran_point(rng);
        auto R = random_ran_point(rng);
        auto I = RanPoint::identity();

        check_true((label + " P+Q==Q+P").c_str(), ran_points_equal(P + Q, Q + P));
        check_true((label + " P+P==dbl").c_str(), ran_points_equal(P + P, P.dbl()));
        check_true((label + " (P+Q)+R==P+(Q+R)").c_str(), ran_points_equal((P + Q) + R, P + (Q + R)));
        check_true((label + " P+I==P").c_str(), ran_points_equal(P + I, P));
        check_true((label + " I+P==P").c_str(), ran_points_equal(I + P, P));
    }

    for (int i = 0; i < 250; i++)
    {
        std::string label = "shaw_pt[" + std::to_string(i) + "]";

        auto P = random_shaw_point(rng);
        auto Q = random_shaw_point(rng);
        auto R = random_shaw_point(rng);
        auto I = ShawPoint::identity();

        check_true((label + " P+Q==Q+P").c_str(), shaw_points_equal(P + Q, Q + P));
        check_true((label + " P+P==dbl").c_str(), shaw_points_equal(P + P, P.dbl()));
        check_true((label + " (P+Q)+R==P+(Q+R)").c_str(), shaw_points_equal((P + Q) + R, P + (Q + R)));
        check_true((label + " P+I==P").c_str(), shaw_points_equal(P + I, P));
        check_true((label + " I+P==P").c_str(), shaw_points_equal(I + P, P));
    }
}

/* ======================================================================
 * 4. fuzz_ipa_edge_cases — ~120
 * ====================================================================== */


void fuzz_ipa_edge_cases()
{
    std::cout << std::endl << "=== Fuzz: IPA Edge Cases ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 4);

    /* Ran */
    for (int i = 0; i < 10; i++)
    {
        std::string label = "ran_ipa[" + std::to_string(i) + "]";
        auto G = random_ran_point(rng);
        auto s = random_ran_scalar(rng);
        auto I = RanPoint::identity();

        /* zero * G == identity (CT) */
        check_true((label + " 0*G==I ct").c_str(), G.scalar_mul(RanScalar::zero()).is_identity());
        /* zero * G == identity (vartime) */
        check_true((label + " 0*G==I vt").c_str(), G.scalar_mul_vartime(RanScalar::zero()).is_identity());
        /* s * identity == identity (vartime — CT path doesn't support identity base) */
        check_true((label + " s*I==I vt").c_str(), I.scalar_mul_vartime(s).is_identity());
        /* P + (-P) == identity */
        check_true((label + " P+(-P)==I").c_str(), (G + (-G)).is_identity());
        /* 1 * G == G */
        check_true((label + " 1*G==G").c_str(), ran_points_equal(G.scalar_mul(RanScalar::one()), G));
        /* -(-P) == P */
        check_true((label + " -(-P)==P").c_str(), ran_points_equal(-(-G), G));
        /* MSM n=1 */
        auto msm1 = RanPoint::multi_scalar_mul(&s, &G, 1);
        auto sm1 = G.scalar_mul_vartime(s);
        check_true((label + " msm1==sm").c_str(), ran_points_equal(msm1, sm1));
    }

    /* Shaw */
    for (int i = 0; i < 10; i++)
    {
        std::string label = "shaw_ipa[" + std::to_string(i) + "]";
        auto G = random_shaw_point(rng);
        auto s = random_shaw_scalar(rng);
        auto I = ShawPoint::identity();

        check_true((label + " 0*G==I ct").c_str(), G.scalar_mul(ShawScalar::zero()).is_identity());
        check_true((label + " 0*G==I vt").c_str(), G.scalar_mul_vartime(ShawScalar::zero()).is_identity());
        check_true((label + " s*I==I vt").c_str(), I.scalar_mul_vartime(s).is_identity());
        check_true((label + " P+(-P)==I").c_str(), (G + (-G)).is_identity());
        check_true((label + " 1*G==G").c_str(), shaw_points_equal(G.scalar_mul(ShawScalar::one()), G));
        check_true((label + " -(-P)==P").c_str(), shaw_points_equal(-(-G), G));
        auto msm1 = ShawPoint::multi_scalar_mul(&s, &G, 1);
        auto sm1 = G.scalar_mul_vartime(s);
        check_true((label + " msm1==sm").c_str(), shaw_points_equal(msm1, sm1));
    }
}

/* ======================================================================
 * 5. fuzz_serialization_roundtrip — ~2,000 checks
 * ====================================================================== */


void fuzz_operator_plus_regression()
{
    std::cout << std::endl << "=== Fuzz: Operator+ Regression ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 18);

    /* Ran */
    for (int i = 0; i < 250; i++)
    {
        std::string label = "ran_opadd[" + std::to_string(i) + "]";
        auto P = random_ran_point(rng);
        auto I = RanPoint::identity();

        /* P + P == P.dbl() */
        check_true((label + " P+P==dbl").c_str(), ran_points_equal(P + P, P.dbl()));
        /* P + (-P) == identity */
        check_true((label + " P+(-P)==I").c_str(), (P + (-P)).is_identity());
        /* P + identity == P */
        check_true((label + " P+I==P").c_str(), ran_points_equal(P + I, P));
        /* identity + P == P */
        check_true((label + " I+P==P").c_str(), ran_points_equal(I + P, P));
        /* identity + identity == identity */
        check_true((label + " I+I==I").c_str(), (I + I).is_identity());

        /* P + Q where P != ±Q: verify via scalar-based method */
        auto Q = random_ran_point(rng);
        auto PQ = P + Q;
        /* Verify PQ - P == Q, i.e. PQ + (-P) == Q */
        auto diff = PQ + (-P);
        check_true((label + " PQ-P==Q").c_str(), ran_points_equal(diff, Q));
    }

    /* Shaw */
    for (int i = 0; i < 250; i++)
    {
        std::string label = "shaw_opadd[" + std::to_string(i) + "]";
        auto P = random_shaw_point(rng);
        auto I = ShawPoint::identity();

        check_true((label + " P+P==dbl").c_str(), shaw_points_equal(P + P, P.dbl()));
        check_true((label + " P+(-P)==I").c_str(), (P + (-P)).is_identity());
        check_true((label + " P+I==P").c_str(), shaw_points_equal(P + I, P));
        check_true((label + " I+P==P").c_str(), shaw_points_equal(I + P, P));
        check_true((label + " I+I==I").c_str(), (I + I).is_identity());

        auto Q = random_shaw_point(rng);
        auto PQ = P + Q;
        auto diff = PQ + (-P);
        check_true((label + " PQ-P==Q").c_str(), shaw_points_equal(diff, Q));
    }
}

/* ======================================================================
 * 19. fuzz_verification_equation — ~500 checks
 * ====================================================================== */
