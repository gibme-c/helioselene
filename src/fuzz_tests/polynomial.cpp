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

void fuzz_polynomial()
{
    std::cout << std::endl << "=== Fuzz: Polynomial Arithmetic ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 14);

    static const unsigned char zero32[32] = {0};

    for (int i = 0; i < 250; i++)
    {
        std::string label = "fp_poly[" + std::to_string(i) + "]";
        size_t deg_a = 1 + (rng.next() % 16);
        size_t deg_b = 1 + (rng.next() % 16);

        /* Build coefficient arrays */
        std::vector<uint8_t> a_coeffs(deg_a * 32);
        std::vector<uint8_t> b_coeffs(deg_b * 32);
        for (size_t j = 0; j < deg_a; j++)
        {
            auto s = random_shaw_scalar(rng); /* Fp elements */
            auto sb = s.to_bytes();
            std::memcpy(&a_coeffs[j * 32], sb.data(), 32);
        }
        for (size_t j = 0; j < deg_b; j++)
        {
            auto s = random_shaw_scalar(rng);
            auto sb = s.to_bytes();
            std::memcpy(&b_coeffs[j * 32], sb.data(), 32);
        }

        auto A = FpPolynomial::from_coefficients(a_coeffs.data(), deg_a);
        auto B = FpPolynomial::from_coefficients(b_coeffs.data(), deg_b);

        /* Random evaluation point */
        auto x_s = random_shaw_scalar(rng);
        auto x = x_s.to_bytes();

        /* Eval consistency: (A*B)(x) == A(x) * B(x) */
        auto AB = A * B;
        auto ab_x = AB.evaluate(x.data());
        auto a_x = A.evaluate(x.data());
        auto b_x = B.evaluate(x.data());

        /* Multiply a_x * b_x via scalar */
        auto sa = ShawScalar::from_bytes(a_x.data());
        auto sb = ShawScalar::from_bytes(b_x.data());
        if (sa.has_value() && sb.has_value())
        {
            auto expected = (sa.value() * sb.value()).to_bytes();
            check_bytes((label + " mul_eval").c_str(), expected.data(), ab_x.data(), 32);
        }

        /* Add consistency: (A+B)(x) == A(x) + B(x) */
        auto ApB = A + B;
        auto apb_x = ApB.evaluate(x.data());
        if (sa.has_value() && sb.has_value())
        {
            auto expected = (sa.value() + sb.value()).to_bytes();
            check_bytes((label + " add_eval").c_str(), expected.data(), apb_x.data(), 32);
        }

        /* divmod: A = Q*B + R at random point */
        if (deg_a >= deg_b)
        {
            auto qr = A.divmod(B);
            auto q_x = qr.first.evaluate(x.data());
            auto r_x = qr.second.evaluate(x.data());

            auto sq = ShawScalar::from_bytes(q_x.data());
            auto sr = ShawScalar::from_bytes(r_x.data());
            if (sq.has_value() && sr.has_value() && sb.has_value() && sa.has_value())
            {
                auto expected = (sq.value() * sb.value() + sr.value()).to_bytes();
                check_bytes((label + " divmod").c_str(), expected.data(), a_x.data(), 32);
            }
        }
    }

    /* from_roots: each root evaluates to zero */
    for (int i = 0; i < 50; i++)
    {
        std::string label = "fp_roots[" + std::to_string(i) + "]";
        size_t n = 2 + (rng.next() % 8);
        std::vector<uint8_t> roots(n * 32);
        for (size_t j = 0; j < n; j++)
        {
            auto s = random_shaw_scalar(rng);
            auto sb = s.to_bytes();
            std::memcpy(&roots[j * 32], sb.data(), 32);
        }
        auto P = FpPolynomial::from_roots(roots.data(), n);
        bool all_zero = true;
        for (size_t j = 0; j < n; j++)
        {
            auto ev = P.evaluate(&roots[j * 32]);
            if (std::memcmp(ev.data(), zero32, 32) != 0)
                all_zero = false;
        }
        check_true(label.c_str(), all_zero);
    }

    /* Fq polynomial: same tests */
    for (int i = 0; i < 250; i++)
    {
        std::string label = "fq_poly[" + std::to_string(i) + "]";
        size_t deg_a = 1 + (rng.next() % 16);
        size_t deg_b = 1 + (rng.next() % 16);

        std::vector<uint8_t> a_coeffs(deg_a * 32);
        std::vector<uint8_t> b_coeffs(deg_b * 32);
        for (size_t j = 0; j < deg_a; j++)
        {
            auto s = random_ran_scalar(rng); /* Fq elements */
            auto sb = s.to_bytes();
            std::memcpy(&a_coeffs[j * 32], sb.data(), 32);
        }
        for (size_t j = 0; j < deg_b; j++)
        {
            auto s = random_ran_scalar(rng);
            auto sb = s.to_bytes();
            std::memcpy(&b_coeffs[j * 32], sb.data(), 32);
        }

        auto A = FqPolynomial::from_coefficients(a_coeffs.data(), deg_a);
        auto B = FqPolynomial::from_coefficients(b_coeffs.data(), deg_b);

        auto x_s = random_ran_scalar(rng);
        auto x = x_s.to_bytes();

        auto AB = A * B;
        auto ab_x = AB.evaluate(x.data());
        auto a_x = A.evaluate(x.data());
        auto b_x = B.evaluate(x.data());

        auto sa = RanScalar::from_bytes(a_x.data());
        auto sb = RanScalar::from_bytes(b_x.data());
        if (sa.has_value() && sb.has_value())
        {
            auto expected = (sa.value() * sb.value()).to_bytes();
            check_bytes((label + " mul_eval").c_str(), expected.data(), ab_x.data(), 32);
        }

        auto ApB = A + B;
        auto apb_x = ApB.evaluate(x.data());
        if (sa.has_value() && sb.has_value())
        {
            auto expected = (sa.value() + sb.value()).to_bytes();
            check_bytes((label + " add_eval").c_str(), expected.data(), apb_x.data(), 32);
        }

        if (deg_a >= deg_b)
        {
            auto qr = A.divmod(B);
            auto q_x = qr.first.evaluate(x.data());
            auto r_x = qr.second.evaluate(x.data());
            auto sq = RanScalar::from_bytes(q_x.data());
            auto sr = RanScalar::from_bytes(r_x.data());
            if (sq.has_value() && sr.has_value() && sb.has_value() && sa.has_value())
            {
                auto expected = (sq.value() * sb.value() + sr.value()).to_bytes();
                check_bytes((label + " divmod").c_str(), expected.data(), a_x.data(), 32);
            }
        }
    }

    for (int i = 0; i < 50; i++)
    {
        std::string label = "fq_roots[" + std::to_string(i) + "]";
        size_t n = 2 + (rng.next() % 8);
        std::vector<uint8_t> roots(n * 32);
        for (size_t j = 0; j < n; j++)
        {
            auto s = random_ran_scalar(rng);
            auto sb = s.to_bytes();
            std::memcpy(&roots[j * 32], sb.data(), 32);
        }
        auto P = FqPolynomial::from_roots(roots.data(), n);
        bool all_zero = true;
        for (size_t j = 0; j < n; j++)
        {
            auto ev = P.evaluate(&roots[j * 32]);
            if (std::memcmp(ev.data(), zero32, 32) != 0)
                all_zero = false;
        }
        check_true(label.c_str(), all_zero);
    }
}

/* ======================================================================
 * 15. fuzz_polynomial_protocol_sizes — ~400
 * ====================================================================== */


void fuzz_polynomial_protocol_sizes()
{
    std::cout << std::endl << "=== Fuzz: Polynomial Protocol Sizes ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 15);

    static const unsigned char zero32[32] = {0};

    /* Fp: Karatsuba-range polys (degree 32-64) */
    for (int trial = 0; trial < 25; trial++)
    {
        std::string label = "fp_kara[" + std::to_string(trial) + "]";
        size_t deg_a = 32 + (rng.next() % 33);
        size_t deg_b = 32 + (rng.next() % 33);

        std::vector<uint8_t> ac(deg_a * 32), bc(deg_b * 32);
        for (size_t j = 0; j < deg_a; j++)
        {
            auto s = random_shaw_scalar(rng);
            auto sb = s.to_bytes();
            std::memcpy(&ac[j * 32], sb.data(), 32);
        }
        for (size_t j = 0; j < deg_b; j++)
        {
            auto s = random_shaw_scalar(rng);
            auto sb = s.to_bytes();
            std::memcpy(&bc[j * 32], sb.data(), 32);
        }

        auto A = FpPolynomial::from_coefficients(ac.data(), deg_a);
        auto B = FpPolynomial::from_coefficients(bc.data(), deg_b);
        auto AB = A * B;

        /* Verify at 3 random points */
        bool ok = true;
        for (int k = 0; k < 3; k++)
        {
            auto x_s = random_shaw_scalar(rng);
            auto x = x_s.to_bytes();
            auto ab_x = AB.evaluate(x.data());
            auto a_x = A.evaluate(x.data());
            auto b_x = B.evaluate(x.data());
            auto sa = ShawScalar::from_bytes(a_x.data());
            auto sb = ShawScalar::from_bytes(b_x.data());
            if (sa.has_value() && sb.has_value())
            {
                auto expected = (sa.value() * sb.value()).to_bytes();
                if (std::memcmp(expected.data(), ab_x.data(), 32) != 0)
                    ok = false;
            }
        }
        check_true(label.c_str(), ok);
    }

    /* Fp: from_roots with 16-32 roots */
    for (int trial = 0; trial < 25; trial++)
    {
        std::string label = "fp_roots_lg[" + std::to_string(trial) + "]";
        size_t n = 16 + (rng.next() % 17);
        std::vector<uint8_t> roots(n * 32);
        for (size_t j = 0; j < n; j++)
        {
            auto s = random_shaw_scalar(rng);
            auto sb = s.to_bytes();
            std::memcpy(&roots[j * 32], sb.data(), 32);
        }
        auto P = FpPolynomial::from_roots(roots.data(), n);
        /* Check 3 random roots */
        bool ok = true;
        for (size_t k = 0; k < 3 && k < n; k++)
        {
            size_t idx = rng.next() % n;
            auto ev = P.evaluate(&roots[idx * 32]);
            if (std::memcmp(ev.data(), zero32, 32) != 0)
                ok = false;
        }
        check_true(label.c_str(), ok);
    }

    /* Fp: interpolation */
    for (int trial = 0; trial < 25; trial++)
    {
        std::string label = "fp_interp[" + std::to_string(trial) + "]";
        size_t n = 8 + (rng.next() % 9);
        std::vector<uint8_t> xs(n * 32), ys(n * 32);
        for (size_t j = 0; j < n; j++)
        {
            auto sx = random_shaw_scalar(rng);
            auto sy = random_shaw_scalar(rng);
            auto xb = sx.to_bytes();
            auto yb = sy.to_bytes();
            std::memcpy(&xs[j * 32], xb.data(), 32);
            std::memcpy(&ys[j * 32], yb.data(), 32);
        }
        auto P = FpPolynomial::interpolate(xs.data(), ys.data(), n);
        bool ok = true;
        for (size_t j = 0; j < n; j++)
        {
            auto ev = P.evaluate(&xs[j * 32]);
            if (std::memcmp(ev.data(), &ys[j * 32], 32) != 0)
                ok = false;
        }
        check_true(label.c_str(), ok);
    }

    /* Fq: same patterns */
    for (int trial = 0; trial < 25; trial++)
    {
        std::string label = "fq_kara[" + std::to_string(trial) + "]";
        size_t deg_a = 32 + (rng.next() % 33);
        size_t deg_b = 32 + (rng.next() % 33);

        std::vector<uint8_t> ac(deg_a * 32), bc(deg_b * 32);
        for (size_t j = 0; j < deg_a; j++)
        {
            auto s = random_ran_scalar(rng);
            auto sb = s.to_bytes();
            std::memcpy(&ac[j * 32], sb.data(), 32);
        }
        for (size_t j = 0; j < deg_b; j++)
        {
            auto s = random_ran_scalar(rng);
            auto sb = s.to_bytes();
            std::memcpy(&bc[j * 32], sb.data(), 32);
        }

        auto A = FqPolynomial::from_coefficients(ac.data(), deg_a);
        auto B = FqPolynomial::from_coefficients(bc.data(), deg_b);
        auto AB = A * B;

        bool ok = true;
        for (int k = 0; k < 3; k++)
        {
            auto x_s = random_ran_scalar(rng);
            auto x = x_s.to_bytes();
            auto ab_x = AB.evaluate(x.data());
            auto a_x = A.evaluate(x.data());
            auto b_x = B.evaluate(x.data());
            auto sa = RanScalar::from_bytes(a_x.data());
            auto sb = RanScalar::from_bytes(b_x.data());
            if (sa.has_value() && sb.has_value())
            {
                auto expected = (sa.value() * sb.value()).to_bytes();
                if (std::memcmp(expected.data(), ab_x.data(), 32) != 0)
                    ok = false;
            }
        }
        check_true(label.c_str(), ok);
    }

    for (int trial = 0; trial < 25; trial++)
    {
        std::string label = "fq_roots_lg[" + std::to_string(trial) + "]";
        size_t n = 16 + (rng.next() % 17);
        std::vector<uint8_t> roots(n * 32);
        for (size_t j = 0; j < n; j++)
        {
            auto s = random_ran_scalar(rng);
            auto sb = s.to_bytes();
            std::memcpy(&roots[j * 32], sb.data(), 32);
        }
        auto P = FqPolynomial::from_roots(roots.data(), n);
        bool ok = true;
        for (size_t k = 0; k < 3 && k < n; k++)
        {
            size_t idx = rng.next() % n;
            auto ev = P.evaluate(&roots[idx * 32]);
            if (std::memcmp(ev.data(), zero32, 32) != 0)
                ok = false;
        }
        check_true(label.c_str(), ok);
    }

    for (int trial = 0; trial < 25; trial++)
    {
        std::string label = "fq_interp[" + std::to_string(trial) + "]";
        size_t n = 8 + (rng.next() % 9);
        std::vector<uint8_t> xs(n * 32), ys(n * 32);
        for (size_t j = 0; j < n; j++)
        {
            auto sx = random_ran_scalar(rng);
            auto sy = random_ran_scalar(rng);
            auto xb = sx.to_bytes();
            auto yb = sy.to_bytes();
            std::memcpy(&xs[j * 32], xb.data(), 32);
            std::memcpy(&ys[j * 32], yb.data(), 32);
        }
        auto P = FqPolynomial::interpolate(xs.data(), ys.data(), n);
        bool ok = true;
        for (size_t j = 0; j < n; j++)
        {
            auto ev = P.evaluate(&xs[j * 32]);
            if (std::memcmp(ev.data(), &ys[j * 32], 32) != 0)
                ok = false;
        }
        check_true(label.c_str(), ok);
    }
}

/* ======================================================================
 * 16. fuzz_divisor — ~600 checks
 * ====================================================================== */
