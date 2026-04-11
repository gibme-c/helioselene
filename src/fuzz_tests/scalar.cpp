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

void fuzz_scalar_arithmetic()
{
    std::cout << std::endl << "=== Fuzz: Scalar Arithmetic ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 1);

    for (int i = 0; i < 1000; i++)
    {
        std::string label = "ran_scalar_arith[" + std::to_string(i) + "]";

        auto a = random_ran_scalar(rng);
        auto b = random_ran_scalar(rng);
        auto c = random_ran_scalar(rng);

        /* Commutativity of add */
        check_true((label + " a+b==b+a").c_str(), a + b == b + a);
        /* Commutativity of mul */
        check_true((label + " a*b==b*a").c_str(), a * b == b * a);
        /* Associativity of add */
        check_true((label + " (a+b)+c==a+(b+c)").c_str(), (a + b) + c == a + (b + c));
        /* Distributivity */
        check_true((label + " a*(b+c)==a*b+a*c").c_str(), a * (b + c) == a * b + a * c);
        /* Identity */
        check_true((label + " a+0==a").c_str(), a + RanScalar::zero() == a);
        check_true((label + " a*1==a").c_str(), a * RanScalar::one() == a);
        /* Inverse */
        check_true((label + " a+(-a)==0").c_str(), (a + (-a)).is_zero());
        /* Square */
        check_true((label + " sq==a*a").c_str(), a.sq() == a * a);
        /* muladd */
        check_true((label + " muladd").c_str(), RanScalar::muladd(a, b, c) == a * b + c);
        /* Inversion */
        if (!a.is_zero())
        {
            auto inv = a.invert();
            check_true((label + " a*inv==1").c_str(), inv.has_value() && a * inv.value() == RanScalar::one());
        }
    }

    for (int i = 0; i < 1000; i++)
    {
        std::string label = "shaw_scalar_arith[" + std::to_string(i) + "]";

        auto a = random_shaw_scalar(rng);
        auto b = random_shaw_scalar(rng);
        auto c = random_shaw_scalar(rng);

        check_true((label + " a+b==b+a").c_str(), a + b == b + a);
        check_true((label + " a*b==b*a").c_str(), a * b == b * a);
        check_true((label + " (a+b)+c==a+(b+c)").c_str(), (a + b) + c == a + (b + c));
        check_true((label + " a*(b+c)==a*b+a*c").c_str(), a * (b + c) == a * b + a * c);
        check_true((label + " a+0==a").c_str(), a + ShawScalar::zero() == a);
        check_true((label + " a*1==a").c_str(), a * ShawScalar::one() == a);
        check_true((label + " a+(-a)==0").c_str(), (a + (-a)).is_zero());
        check_true((label + " sq==a*a").c_str(), a.sq() == a * a);
        check_true((label + " muladd").c_str(), ShawScalar::muladd(a, b, c) == a * b + c);
        if (!a.is_zero())
        {
            auto inv = a.invert();
            check_true((label + " a*inv==1").c_str(), inv.has_value() && a * inv.value() == ShawScalar::one());
        }
    }
}

/* ======================================================================
 * 2. fuzz_scalar_edge_cases — ~100
 * ====================================================================== */


void fuzz_scalar_edge_cases()
{
    std::cout << std::endl << "=== Fuzz: Scalar Edge Cases ===" << std::endl;

    /* Ran */
    {
        auto z = RanScalar::zero();
        auto o = RanScalar::one();

        check_true("ran zero+zero==zero", (z + z).is_zero());
        check_true("ran one*one==one", o * o == o);
        check_true("ran zero.invert()==nullopt", !z.invert().has_value());
        check_true("ran -zero==zero", (-z).is_zero());
        check_true("ran one.invert()==one", o.invert().has_value() && o.invert().value() == o);

        /* reduce_wide all-zero */
        uint8_t all_zero[64] = {0};
        check_true("ran reduce_wide(0)==zero", RanScalar::reduce_wide(all_zero).is_zero());

        /* reduce_wide all-0xFF */
        uint8_t all_ff[64];
        std::memset(all_ff, 0xFF, 64);
        auto rff = RanScalar::reduce_wide(all_ff);
        check_true("ran reduce_wide(ff) != zero", !rff.is_zero());

        /* from_bytes with value >= modulus should fail */
        uint8_t over[32];
        std::memset(over, 0xFF, 32);
        check_true("ran from_bytes(>=q)==nullopt", !RanScalar::from_bytes(over).has_value());
    }

    /* Shaw */
    {
        auto z = ShawScalar::zero();
        auto o = ShawScalar::one();

        check_true("shaw zero+zero==zero", (z + z).is_zero());
        check_true("shaw one*one==one", o * o == o);
        check_true("shaw zero.invert()==nullopt", !z.invert().has_value());
        check_true("shaw -zero==zero", (-z).is_zero());
        check_true("shaw one.invert()==one", o.invert().has_value() && o.invert().value() == o);

        uint8_t all_zero[64] = {0};
        check_true("shaw reduce_wide(0)==zero", ShawScalar::reduce_wide(all_zero).is_zero());

        uint8_t all_ff[64];
        std::memset(all_ff, 0xFF, 64);
        auto rff = ShawScalar::reduce_wide(all_ff);
        check_true("shaw reduce_wide(ff) != zero", !rff.is_zero());

        uint8_t over[32];
        std::memset(over, 0xFF, 32);
        check_true("shaw from_bytes(>=p)==nullopt", !ShawScalar::from_bytes(over).has_value());
    }
}

/* ======================================================================
 * 3. fuzz_point_arithmetic — ~2,000 checks
 * ====================================================================== */


void fuzz_reduce_wide_distribution()
{
    std::cout << std::endl << "=== Fuzz: reduce_wide Distribution ===" << std::endl;

    constexpr int N_ITERS = 8192;
    constexpr int N_BUCKETS = 8;
    const double expected_per_bucket = (double)N_ITERS / (double)N_BUCKETS;

    xoshiro256ss rng;
    rng.seed(global_seed + 22);

    auto chi_square = [&](const int counts[N_BUCKETS])
    {
        double chisq = 0.0;
        for (int i = 0; i < N_BUCKETS; i++)
        {
            double d = (double)counts[i] - expected_per_bucket;
            chisq += (d * d) / expected_per_bucket;
        }
        return chisq;
    };

    /* Chi-square 7 d.o.f.: p > 0.001 requires chisq < 24.32 (upper tail).
     * That gives us tons of headroom on a healthy uniform distribution
     * while still catching a hard bucketing bias. */
    constexpr double CHI2_THRESHOLD = 24.32;

    /* Ran reduce_wide */
    {
        int counts[N_BUCKETS] = {0};
        for (int iter = 0; iter < N_ITERS; iter++)
        {
            unsigned char wide[64];
            for (int i = 0; i < 64; i++)
                wide[i] = (unsigned char)(rng.next() & 0xFF);

            fq_fe out;
            ran_scalar_reduce_wide(out, wide);

            unsigned char out_bytes[32];
            fq_tobytes(out_bytes, out);
            counts[out_bytes[0] & (N_BUCKETS - 1)]++;
        }
        double chisq = chi_square(counts);
        check_true("fuzz: ran_scalar_reduce_wide mod-8 uniformity", chisq < CHI2_THRESHOLD);
    }

    /* Shaw reduce_wide */
    {
        int counts[N_BUCKETS] = {0};
        for (int iter = 0; iter < N_ITERS; iter++)
        {
            unsigned char wide[64];
            for (int i = 0; i < 64; i++)
                wide[i] = (unsigned char)(rng.next() & 0xFF);

            fp_fe out;
            shaw_scalar_reduce_wide(out, wide);

            unsigned char out_bytes[32];
            fp_tobytes(out_bytes, out);
            counts[out_bytes[0] & (N_BUCKETS - 1)]++;
        }
        double chisq = chi_square(counts);
        check_true("fuzz: shaw_scalar_reduce_wide mod-8 uniformity", chisq < CHI2_THRESHOLD);
    }
}
