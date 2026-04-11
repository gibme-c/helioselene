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

#ifdef RANSHAW_ECFFT

void fuzz_ecfft_poly_mul()
{
    std::cout << std::endl << "=== Fuzz: ECFFT Polynomial Multiplication ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 20);

    /* Initialize ECFFT contexts */
    ecfft_fp_ctx fp_ctx;
    ecfft_fq_ctx fq_ctx;
    ecfft_fp_init(&fp_ctx);
    ecfft_fq_init(&fq_ctx);

    /* (a) Enter/exit round-trip — Fp */
    for (int trial = 0; trial < 50; trial++)
    {
        std::string label = "ecfft_fp_rt[" + std::to_string(trial) + "]";
        int deg = 4 + (rng.next() % 13); /* degree 4-16 */
        /* Need power-of-2 domain size */
        size_t n = 1;
        while (n < (size_t)(deg + 1))
            n <<= 1;
        if (n > fp_ctx.domain_size)
            continue;

        auto coeffs = std::make_unique<fp_fe[]>(n);
        auto saved = std::make_unique<fp_fe[]>(n);
        for (size_t j = 0; j < (size_t)(deg + 1); j++)
        {
            auto s = random_shaw_scalar(rng);
            auto sb = s.to_bytes();
            fp_frombytes(coeffs[j], sb.data());
            fp_copy(saved[j], coeffs[j]);
        }
        for (size_t j = (size_t)(deg + 1); j < n; j++)
        {
            fp_0(coeffs[j]);
            fp_0(saved[j]);
        }

        ecfft_fp_enter(coeffs.get(), n, &fp_ctx);
        ecfft_fp_exit(coeffs.get(), n, &fp_ctx);

        bool ok = true;
        for (size_t j = 0; j < n; j++)
        {
            unsigned char a_bytes[32], b_bytes[32];
            fp_tobytes(a_bytes, coeffs[j]);
            fp_tobytes(b_bytes, saved[j]);
            if (std::memcmp(a_bytes, b_bytes, 32) != 0)
                ok = false;
        }
        check_true(label.c_str(), ok);
    }

    /* (a) Enter/exit round-trip — Fq */
    for (int trial = 0; trial < 50; trial++)
    {
        std::string label = "ecfft_fq_rt[" + std::to_string(trial) + "]";
        int deg = 4 + (rng.next() % 13);
        size_t n = 1;
        while (n < (size_t)(deg + 1))
            n <<= 1;
        if (n > fq_ctx.domain_size)
            continue;

        auto coeffs = std::make_unique<fq_fe[]>(n);
        auto saved = std::make_unique<fq_fe[]>(n);
        for (size_t j = 0; j < (size_t)(deg + 1); j++)
        {
            auto s = random_ran_scalar(rng);
            auto sb = s.to_bytes();
            fq_frombytes(coeffs[j], sb.data());
            fq_copy(saved[j], coeffs[j]);
        }
        for (size_t j = (size_t)(deg + 1); j < n; j++)
        {
            fq_0(coeffs[j]);
            fq_0(saved[j]);
        }

        ecfft_fq_enter(coeffs.get(), n, &fq_ctx);
        ecfft_fq_exit(coeffs.get(), n, &fq_ctx);

        bool ok = true;
        for (size_t j = 0; j < n; j++)
        {
            unsigned char a_bytes[32], b_bytes[32];
            fq_tobytes(a_bytes, coeffs[j]);
            fq_tobytes(b_bytes, saved[j]);
            if (std::memcmp(a_bytes, b_bytes, 32) != 0)
                ok = false;
        }
        check_true(label.c_str(), ok);
    }

    /* (b) Small poly multiply via C++ API (schoolbook/Karatsuba, verify ECFFT compilation doesn't break them) */
    for (int trial = 0; trial < 100; trial++)
    {
        std::string label = "ecfft_small_fp[" + std::to_string(trial) + "]";
        size_t deg_a = 2 + (rng.next() % 15);
        size_t deg_b = 2 + (rng.next() % 15);

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

        auto x_s = random_shaw_scalar(rng);
        auto x = x_s.to_bytes();
        auto ab_x = AB.evaluate(x.data());
        auto a_x = A.evaluate(x.data());
        auto b_x = B.evaluate(x.data());
        auto sa = ShawScalar::from_bytes(a_x.data());
        auto sb = ShawScalar::from_bytes(b_x.data());
        bool ok = false;
        if (sa.has_value() && sb.has_value())
        {
            auto expected = (sa.value() * sb.value()).to_bytes();
            ok = std::memcmp(expected.data(), ab_x.data(), 32) == 0;
        }
        check_true(label.c_str(), ok);
    }

    for (int trial = 0; trial < 100; trial++)
    {
        std::string label = "ecfft_small_fq[" + std::to_string(trial) + "]";
        size_t deg_a = 2 + (rng.next() % 15);
        size_t deg_b = 2 + (rng.next() % 15);

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

        auto x_s = random_ran_scalar(rng);
        auto x = x_s.to_bytes();
        auto ab_x = AB.evaluate(x.data());
        auto a_x = A.evaluate(x.data());
        auto b_x = B.evaluate(x.data());
        auto sa = RanScalar::from_bytes(a_x.data());
        auto sb = RanScalar::from_bytes(b_x.data());
        bool ok = false;
        if (sa.has_value() && sb.has_value())
        {
            auto expected = (sa.value() * sb.value()).to_bytes();
            ok = std::memcmp(expected.data(), ab_x.data(), 32) == 0;
        }
        check_true(label.c_str(), ok);
    }

    /* (c) Karatsuba-threshold polys (deg 32-64) */
    for (int trial = 0; trial < 25; trial++)
    {
        std::string label = "ecfft_kara_fp[" + std::to_string(trial) + "]";
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

    for (int trial = 0; trial < 25; trial++)
    {
        std::string label = "ecfft_kara_fq[" + std::to_string(trial) + "]";
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

    /* (d) ECFFT-threshold polys (deg >= 1024) — generate from roots for guaranteed structure */
    for (int trial = 0; trial < 2; trial++)
    {
        std::string label = "ecfft_large_fp[" + std::to_string(trial) + "]";
        size_t n_roots = 1024;
        std::vector<uint8_t> roots_a(n_roots * 32), roots_b(n_roots * 32);
        for (size_t j = 0; j < n_roots; j++)
        {
            auto sa = random_shaw_scalar(rng);
            auto sb_val = random_shaw_scalar(rng);
            auto sab = sa.to_bytes();
            auto sbb = sb_val.to_bytes();
            std::memcpy(&roots_a[j * 32], sab.data(), 32);
            std::memcpy(&roots_b[j * 32], sbb.data(), 32);
        }

        auto A = FpPolynomial::from_roots(roots_a.data(), n_roots);
        auto B = FpPolynomial::from_roots(roots_b.data(), n_roots);
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

    for (int trial = 0; trial < 2; trial++)
    {
        std::string label = "ecfft_large_fq[" + std::to_string(trial) + "]";
        size_t n_roots = 1024;
        std::vector<uint8_t> roots_a(n_roots * 32), roots_b(n_roots * 32);
        for (size_t j = 0; j < n_roots; j++)
        {
            auto sa = random_ran_scalar(rng);
            auto sb_val = random_ran_scalar(rng);
            auto sab = sa.to_bytes();
            auto sbb = sb_val.to_bytes();
            std::memcpy(&roots_a[j * 32], sab.data(), 32);
            std::memcpy(&roots_b[j * 32], sbb.data(), 32);
        }

        auto A = FqPolynomial::from_roots(roots_a.data(), n_roots);
        auto B = FqPolynomial::from_roots(roots_b.data(), n_roots);

        auto AB = A * B;

        /* Verify at 3 random points */
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
}

#endif /* RANSHAW_ECFFT */

/* ======================================================================
 * 21. fuzz_all_path_cross_validation — ~2,900 checks
 *
 * For each (scalar, point) pair, compute via ALL 6 code paths and verify
 * they all agree:
 *   A. CT scalarmul (ground truth)
 *   B. Vartime wNAF
 *   C. MSM with n=1
 *   D. Pedersen commit (s*P + 0*G)
 *   E. Fixed-base CT scalarmul
 *   F. Fixed-base MSM (n=1)
 * ====================================================================== */

/* ======================================================================
 * reduce_wide distribution harness
 *
 * Samples random 64-byte inputs, reduces each mod q / mod p, and buckets
 * the low-byte residue mod 8. Runs a chi-square uniformity test against
 * the 1/8 expected share. A broken CT fix (e.g. a branch that skips one
 * correction term when lo_b = 0) shows up as a visible bias. The p-value
 * threshold is deliberately loose (p > 0.001) so the test doesn't flake
 * from ordinary RNG variance.
 * ====================================================================== */
