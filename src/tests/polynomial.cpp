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

#include "tests/common.h"
#include "tests/registry.h"

void test_fp_poly()
{
    std::cout << std::endl << "=== F_p polynomial ===" << std::endl;
    unsigned char buf[32];

    /* (x+1)(x-1) = x^2 - 1 */
    {
        fp_poly a, b, r;
        a.coeffs.resize(2);
        fp_1(a.coeffs[0].v); /* 1 */
        fp_1(a.coeffs[1].v); /* x */

        b.coeffs.resize(2);
        fp_fe neg1;
        fp_fe one_fe;
        fp_1(one_fe);
        fp_neg(neg1, one_fe);
        std::memcpy(b.coeffs[0].v, neg1, sizeof(fp_fe));
        fp_1(b.coeffs[1].v);

        fp_poly_mul(&r, &a, &b);

        /* r should be [-1, 0, 1] (x^2 - 1) */
        check_int("(x+1)(x-1) degree", 3, (int)r.coeffs.size());

        fp_fe c0;
        std::memcpy(c0, r.coeffs[0].v, sizeof(fp_fe));
        fp_tobytes(buf, c0);
        unsigned char neg1_bytes[32];
        fp_tobytes(neg1_bytes, neg1);
        check_bytes("(x+1)(x-1) const coeff == -1", neg1_bytes, buf, 32);

        fp_fe c1;
        std::memcpy(c1, r.coeffs[1].v, sizeof(fp_fe));
        fp_tobytes(buf, c1);
        check_bytes("(x+1)(x-1) x coeff == 0", zero_bytes, buf, 32);

        fp_fe c2;
        std::memcpy(c2, r.coeffs[2].v, sizeof(fp_fe));
        fp_tobytes(buf, c2);
        check_bytes("(x+1)(x-1) x^2 coeff == 1", one_bytes, buf, 32);
    }

    /* Evaluate x^2-1 at x=3 should give 8 */
    {
        fp_poly p;
        p.coeffs.resize(3);
        fp_fe one_fe, neg1;
        fp_1(one_fe);
        fp_neg(neg1, one_fe);
        std::memcpy(p.coeffs[0].v, neg1, sizeof(fp_fe));
        fp_0(p.coeffs[1].v);
        fp_1(p.coeffs[2].v);

        unsigned char three_bytes[32] = {0x03};
        fp_fe x_val;
        fp_frombytes(x_val, three_bytes);

        fp_fe result;
        fp_poly_eval(result, &p, x_val);
        fp_tobytes(buf, result);
        unsigned char eight_bytes[32] = {0x08};
        check_bytes("eval x^2-1 at x=3 == 8", eight_bytes, buf, 32);
    }

    /* from_roots: roots=[2,3] -> (x-2)(x-3) = x^2-5x+6 */
    {
        unsigned char r1_bytes[32] = {0x02};
        unsigned char r2_bytes[32] = {0x03};
        fp_fe roots[2];
        fp_frombytes(roots[0], r1_bytes);
        fp_frombytes(roots[1], r2_bytes);

        fp_poly p;
        fp_poly_from_roots(&p, roots, 2);

        /* Evaluate at roots should give 0 */
        fp_fe val;
        fp_poly_eval(val, &p, roots[0]);
        fp_tobytes(buf, val);
        check_bytes("from_roots(2,3) eval at 2 == 0", zero_bytes, buf, 32);

        fp_poly_eval(val, &p, roots[1]);
        fp_tobytes(buf, val);
        check_bytes("from_roots(2,3) eval at 3 == 0", zero_bytes, buf, 32);
    }

    /* divmod: (x^2-1) / (x+1) == (x-1), remainder 0 */
    {
        fp_poly dividend, divisor_poly, q, rem;
        dividend.coeffs.resize(3);
        fp_fe one_fe, neg1;
        fp_1(one_fe);
        fp_neg(neg1, one_fe);
        std::memcpy(dividend.coeffs[0].v, neg1, sizeof(fp_fe));
        fp_0(dividend.coeffs[1].v);
        fp_1(dividend.coeffs[2].v);

        divisor_poly.coeffs.resize(2);
        fp_1(divisor_poly.coeffs[0].v);
        fp_1(divisor_poly.coeffs[1].v);

        fp_poly_divmod(&q, &rem, &dividend, &divisor_poly);

        /* q should be (x - 1): [-1, 1] */
        check_int("divmod quotient size", 2, (int)q.coeffs.size());
        fp_fe q0;
        std::memcpy(q0, q.coeffs[0].v, sizeof(fp_fe));
        fp_tobytes(buf, q0);
        unsigned char neg1_bytes[32];
        fp_tobytes(neg1_bytes, neg1);
        check_bytes("divmod quotient const == -1", neg1_bytes, buf, 32);

        fp_fe q1;
        std::memcpy(q1, q.coeffs[1].v, sizeof(fp_fe));
        fp_tobytes(buf, q1);
        check_bytes("divmod quotient x coeff == 1", one_bytes, buf, 32);

        /* remainder should be 0 */
        fp_fe r0;
        std::memcpy(r0, rem.coeffs[0].v, sizeof(fp_fe));
        fp_tobytes(buf, r0);
        check_bytes("divmod remainder == 0", zero_bytes, buf, 32);
    }
}


void test_fq_poly()
{
    std::cout << std::endl << "=== F_q polynomial ===" << std::endl;
    unsigned char buf[32];

    /* from_roots + eval at roots should give 0 */
    {
        unsigned char r1_bytes[32] = {0x05};
        unsigned char r2_bytes[32] = {0x07};
        unsigned char r3_bytes[32] = {0x0b};
        fq_fe roots[3];
        fq_frombytes(roots[0], r1_bytes);
        fq_frombytes(roots[1], r2_bytes);
        fq_frombytes(roots[2], r3_bytes);

        fq_poly p;
        fq_poly_from_roots(&p, roots, 3);

        for (int i = 0; i < 3; i++)
        {
            fq_fe val;
            fq_poly_eval(val, &p, roots[i]);
            fq_tobytes(buf, val);
            std::string name = "fq from_roots eval at root " + std::to_string(i) + " == 0";
            check_bytes(name.c_str(), zero_bytes, buf, 32);
        }
    }

    /* mul commutativity */
    {
        fq_poly a, b, ab, ba;
        a.coeffs.resize(2);
        fq_fe two, three;
        unsigned char two_b[32] = {0x02};
        unsigned char three_b[32] = {0x03};
        fq_frombytes(two, two_b);
        fq_frombytes(three, three_b);
        std::memcpy(a.coeffs[0].v, two, sizeof(fq_fe));
        std::memcpy(a.coeffs[1].v, three, sizeof(fq_fe));

        b.coeffs.resize(2);
        unsigned char five_b[32] = {0x05};
        unsigned char seven_b[32] = {0x07};
        fq_fe five, seven;
        fq_frombytes(five, five_b);
        fq_frombytes(seven, seven_b);
        std::memcpy(b.coeffs[0].v, five, sizeof(fq_fe));
        std::memcpy(b.coeffs[1].v, seven, sizeof(fq_fe));

        fq_poly_mul(&ab, &a, &b);
        fq_poly_mul(&ba, &b, &a);

        bool match = true;
        for (size_t i = 0; i < ab.coeffs.size(); i++)
        {
            unsigned char ab_c[32], ba_c[32];
            fq_tobytes(ab_c, ab.coeffs[i].v);
            fq_tobytes(ba_c, ba.coeffs[i].v);
            if (std::memcmp(ab_c, ba_c, 32) != 0)
                match = false;
        }
        ++tests_run;
        if (match)
        {
            ++tests_passed;
            std::cout << "  PASS: fq poly mul commutative" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: fq poly mul commutative" << std::endl;
        }
    }
}


void test_poly_extended()
{
    std::cout << std::endl << "=== Polynomial extended ===" << std::endl;
    unsigned char buf[32];

    /* Degree-0: constant * constant */
    {
        fp_poly a, b, r;
        a.coeffs.resize(1);
        unsigned char three_b[32] = {0x03};
        fp_frombytes(a.coeffs[0].v, three_b);
        b.coeffs.resize(1);
        unsigned char five_b[32] = {0x05};
        fp_frombytes(b.coeffs[0].v, five_b);
        fp_poly_mul(&r, &a, &b);
        check_int("deg-0 mul result size", 1, (int)r.coeffs.size());
        fp_fe c0;
        std::memcpy(c0, r.coeffs[0].v, sizeof(fp_fe));
        fp_tobytes(buf, c0);
        unsigned char fifteen_b[32] = {0x0f};
        check_bytes("3 * 5 == 15", fifteen_b, buf, 32);
    }

    /* eval(any_poly, 0) == constant coefficient */
    {
        fp_poly p;
        p.coeffs.resize(3);
        unsigned char c0_b[32] = {0x07};
        unsigned char c1_b[32] = {0x03};
        unsigned char c2_b[32] = {0x02};
        fp_frombytes(p.coeffs[0].v, c0_b);
        fp_frombytes(p.coeffs[1].v, c1_b);
        fp_frombytes(p.coeffs[2].v, c2_b);

        fp_fe zero_val, result;
        fp_0(zero_val);
        fp_poly_eval(result, &p, zero_val);
        fp_tobytes(buf, result);
        check_bytes("fp eval(poly, 0) == const coeff", c0_b, buf, 32);
    }

    /* Single root: from_roots([r], 1), eval at r == 0 */
    {
        unsigned char r_b[32] = {0x09};
        fp_fe root;
        fp_frombytes(root, r_b);
        fp_poly p;
        fp_poly_from_roots(&p, &root, 1);
        fp_fe val;
        fp_poly_eval(val, &p, root);
        fp_tobytes(buf, val);
        check_bytes("fp from_roots([9]) eval at 9 == 0", zero_bytes, buf, 32);
    }

    /* Many roots n=10: eval at each root == 0 */
    {
        fp_fe roots[10];
        for (int i = 0; i < 10; i++)
        {
            unsigned char rb[32] = {};
            rb[0] = (unsigned char)(i + 1);
            fp_frombytes(roots[i], rb);
        }
        fp_poly p;
        fp_poly_from_roots(&p, roots, 10);
        bool all_zero = true;
        for (int i = 0; i < 10; i++)
        {
            fp_fe val;
            fp_poly_eval(val, &p, roots[i]);
            unsigned char vb[32];
            fp_tobytes(vb, val);
            if (std::memcmp(vb, zero_bytes, 32) != 0)
                all_zero = false;
        }
        ++tests_run;
        if (all_zero)
        {
            ++tests_passed;
            std::cout << "  PASS: fp from_roots n=10 all evals == 0" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: fp from_roots n=10 some eval != 0" << std::endl;
        }
    }

    /* fq_poly_divmod: (x^2-1) / (x+1) == (x-1), remainder 0 */
    {
        fq_poly dividend, divisor_poly, q, rem;
        dividend.coeffs.resize(3);
        fq_fe one_fe, neg1;
        fq_1(one_fe);
        fq_neg(neg1, one_fe);
        std::memcpy(dividend.coeffs[0].v, neg1, sizeof(fq_fe));
        fq_0(dividend.coeffs[1].v);
        fq_1(dividend.coeffs[2].v);

        divisor_poly.coeffs.resize(2);
        fq_1(divisor_poly.coeffs[0].v);
        fq_1(divisor_poly.coeffs[1].v);

        fq_poly_divmod(&q, &rem, &dividend, &divisor_poly);

        check_int("fq divmod quotient size", 2, (int)q.coeffs.size());

        fq_fe q0;
        std::memcpy(q0, q.coeffs[0].v, sizeof(fq_fe));
        fq_tobytes(buf, q0);
        unsigned char neg1_bytes[32];
        fq_tobytes(neg1_bytes, neg1);
        check_bytes("fq divmod quotient const == -1", neg1_bytes, buf, 32);

        fq_fe q1;
        std::memcpy(q1, q.coeffs[1].v, sizeof(fq_fe));
        fq_tobytes(buf, q1);
        check_bytes("fq divmod quotient x coeff == 1", one_bytes, buf, 32);

        fq_fe r0;
        std::memcpy(r0, rem.coeffs[0].v, sizeof(fq_fe));
        fq_tobytes(buf, r0);
        check_bytes("fq divmod remainder == 0", zero_bytes, buf, 32);
    }

    /* Non-zero remainder: (x^2+1) / (x+1) */
    {
        fp_poly dividend, divisor_poly, q, rem;
        dividend.coeffs.resize(3);
        fp_1(dividend.coeffs[0].v); /* 1 */
        fp_0(dividend.coeffs[1].v); /* 0 */
        fp_1(dividend.coeffs[2].v); /* x^2 */

        divisor_poly.coeffs.resize(2);
        fp_1(divisor_poly.coeffs[0].v);
        fp_1(divisor_poly.coeffs[1].v);

        fp_poly_divmod(&q, &rem, &dividend, &divisor_poly);

        /* Quotient should be (x-1) */
        check_int("nonzero rem: quotient size", 2, (int)q.coeffs.size());

        /* Remainder should be 2 */
        fp_fe r0;
        std::memcpy(r0, rem.coeffs[0].v, sizeof(fp_fe));
        fp_tobytes(buf, r0);
        unsigned char two_b[32] = {0x02};
        check_bytes("(x^2+1)/(x+1) remainder == 2", two_b, buf, 32);
    }

    /* fq eval(poly, 0) == constant coefficient */
    {
        fq_poly p;
        p.coeffs.resize(3);
        unsigned char c0_b[32] = {0x0b};
        unsigned char c1_b[32] = {0x03};
        unsigned char c2_b[32] = {0x02};
        fq_frombytes(p.coeffs[0].v, c0_b);
        fq_frombytes(p.coeffs[1].v, c1_b);
        fq_frombytes(p.coeffs[2].v, c2_b);

        fq_fe zero_val, result;
        fq_0(zero_val);
        fq_poly_eval(result, &p, zero_val);
        fq_tobytes(buf, result);
        check_bytes("fq eval(poly, 0) == const coeff", c0_b, buf, 32);
    }
}


void test_poly_interpolate()
{
    std::cout << std::endl << "=== Polynomial interpolation ===" << std::endl;

    /* Fp: interpolate through 3 known points: (1,1), (2,4), (3,9) -> f(x) = x^2 */
    {
        fp_fe xs[3], ys[3];
        unsigned char x1[32] = {1}, x2[32] = {2}, x3[32] = {3};
        unsigned char y1[32] = {1}, y4[32] = {4}, y9[32] = {9};
        fp_frombytes(xs[0], x1);
        fp_frombytes(xs[1], x2);
        fp_frombytes(xs[2], x3);
        fp_frombytes(ys[0], y1);
        fp_frombytes(ys[1], y4);
        fp_frombytes(ys[2], y9);

        fp_poly out;
        fp_poly_interpolate(&out, xs, ys, 3);

        /* Verify evaluations: f(1)=1, f(2)=4, f(3)=9 */
        fp_fe result;
        fp_poly_eval(result, &out, xs[0]);
        unsigned char rb[32];
        fp_tobytes(rb, result);
        check_bytes("fp interp f(1)==1", y1, rb, 32);

        fp_poly_eval(result, &out, xs[1]);
        fp_tobytes(rb, result);
        check_bytes("fp interp f(2)==4", y4, rb, 32);

        fp_poly_eval(result, &out, xs[2]);
        fp_tobytes(rb, result);
        check_bytes("fp interp f(3)==9", y9, rb, 32);

        /* Degree check: 3 points -> degree 2 polynomial (3 coefficients) */
        check_int("fp interp degree == 2", 3, (int)out.coeffs.size());
    }

    /* Fq: interpolate through 3 known points: (1,2), (2,5), (3,10) -> f(x) = x^2 + 1 */
    {
        fq_fe xs[3], ys[3];
        unsigned char x1[32] = {1}, x2[32] = {2}, x3[32] = {3};
        unsigned char y2[32] = {2}, y5[32] = {5}, y10[32] = {10};
        fq_frombytes(xs[0], x1);
        fq_frombytes(xs[1], x2);
        fq_frombytes(xs[2], x3);
        fq_frombytes(ys[0], y2);
        fq_frombytes(ys[1], y5);
        fq_frombytes(ys[2], y10);

        fq_poly out;
        fq_poly_interpolate(&out, xs, ys, 3);

        fq_fe result;
        fq_poly_eval(result, &out, xs[0]);
        unsigned char rb[32];
        fq_tobytes(rb, result);
        check_bytes("fq interp f(1)==2", y2, rb, 32);

        fq_poly_eval(result, &out, xs[1]);
        fq_tobytes(rb, result);
        check_bytes("fq interp f(2)==5", y5, rb, 32);

        fq_poly_eval(result, &out, xs[2]);
        fq_tobytes(rb, result);
        check_bytes("fq interp f(3)==10", y10, rb, 32);

        check_int("fq interp degree == 2", 3, (int)out.coeffs.size());
    }

    /* Single-point interpolation */
    {
        fp_fe xs[1], ys[1];
        unsigned char x1[32] = {7}, y42[32] = {42};
        fp_frombytes(xs[0], x1);
        fp_frombytes(ys[0], y42);
        fp_poly out;
        fp_poly_interpolate(&out, xs, ys, 1);
        check_int("fp interp n=1 degree", 1, (int)out.coeffs.size());

        fp_fe result;
        fp_poly_eval(result, &out, xs[0]);
        unsigned char rb[32];
        fp_tobytes(rb, result);
        check_bytes("fp interp n=1 eval", y42, rb, 32);
    }
}


void test_karatsuba()
{
    std::cout << std::endl << "=== Karatsuba ===" << std::endl;

    /*
     * Verify Karatsuba matches schoolbook for polynomials built from roots.
     * Build two poly of degree 32+ by using from_roots, then multiply them.
     * Verify by evaluating at a test point.
     */

    /* Fp: build A from 33 roots, B from 33 roots, multiply via Karatsuba */
    {
        /* Create roots: just small integers */
        fp_fe roots_a[33], roots_b[33];
        for (int i = 0; i < 33; i++)
        {
            unsigned char buf[32] = {};
            buf[0] = (unsigned char)(i + 1);
            fp_frombytes(roots_a[i], buf);
            buf[0] = (unsigned char)(i + 34);
            fp_frombytes(roots_b[i], buf);
        }

        fp_poly A, B, C;
        fp_poly_from_roots(&A, roots_a, 33);
        fp_poly_from_roots(&B, roots_b, 33);

        /* C = A * B (will use Karatsuba since both have 34 coefficients >= 32) */
        fp_poly_mul(&C, &A, &B);

        /* Verify: eval C at root of A should be 0 (since A(root)=0, C=A*B, so C(root)=0) */
        fp_fe result;
        fp_poly_eval(result, &C, roots_a[0]);
        unsigned char rb[32];
        fp_tobytes(rb, result);
        check_bytes("fp karatsuba: C(root_a[0]) == 0", zero_bytes, rb, 32);

        fp_poly_eval(result, &C, roots_a[16]);
        fp_tobytes(rb, result);
        check_bytes("fp karatsuba: C(root_a[16]) == 0", zero_bytes, rb, 32);

        /* Verify degree: (33+33) = 66 roots -> degree 66 product */
        check_int("fp karatsuba degree", 67, (int)C.coeffs.size());

        /* Verify at a non-root point: C(0) should be A(0)*B(0) */
        fp_fe zero_pt, a_at_0, b_at_0, expected_c0, c_at_0;
        fp_0(zero_pt);
        fp_poly_eval(a_at_0, &A, zero_pt);
        fp_poly_eval(b_at_0, &B, zero_pt);
        fp_mul(expected_c0, a_at_0, b_at_0);
        fp_poly_eval(c_at_0, &C, zero_pt);

        unsigned char exp_bytes[32], act_bytes[32];
        fp_tobytes(exp_bytes, expected_c0);
        fp_tobytes(act_bytes, c_at_0);
        check_bytes("fp karatsuba: C(0) == A(0)*B(0)", exp_bytes, act_bytes, 32);
    }

    /* Fq: same test */
    {
        fq_fe roots_a[33], roots_b[33];
        for (int i = 0; i < 33; i++)
        {
            unsigned char buf[32] = {};
            buf[0] = (unsigned char)(i + 1);
            fq_frombytes(roots_a[i], buf);
            buf[0] = (unsigned char)(i + 34);
            fq_frombytes(roots_b[i], buf);
        }

        fq_poly A, B, C;
        fq_poly_from_roots(&A, roots_a, 33);
        fq_poly_from_roots(&B, roots_b, 33);
        fq_poly_mul(&C, &A, &B);

        fq_fe result;
        fq_poly_eval(result, &C, roots_a[0]);
        unsigned char rb[32];
        fq_tobytes(rb, result);
        check_bytes("fq karatsuba: C(root_a[0]) == 0", zero_bytes, rb, 32);

        check_int("fq karatsuba degree", 67, (int)C.coeffs.size());

        fq_fe zero_pt, a_at_0, b_at_0, expected_c0, c_at_0;
        fq_0(zero_pt);
        fq_poly_eval(a_at_0, &A, zero_pt);
        fq_poly_eval(b_at_0, &B, zero_pt);
        fq_mul(expected_c0, a_at_0, b_at_0);
        fq_poly_eval(c_at_0, &C, zero_pt);

        unsigned char exp_bytes[32], act_bytes[32];
        fq_tobytes(exp_bytes, expected_c0);
        fq_tobytes(act_bytes, c_at_0);
        check_bytes("fq karatsuba: C(0) == A(0)*B(0)", exp_bytes, act_bytes, 32);
    }

    /* Mixed sizes: one small (< threshold), one large (>= threshold) -> schoolbook */
    {
        fp_fe roots_a[5], roots_b[33];
        for (int i = 0; i < 5; i++)
        {
            unsigned char buf[32] = {};
            buf[0] = (unsigned char)(i + 1);
            fp_frombytes(roots_a[i], buf);
        }
        for (int i = 0; i < 33; i++)
        {
            unsigned char buf[32] = {};
            buf[0] = (unsigned char)(i + 10);
            fp_frombytes(roots_b[i], buf);
        }

        fp_poly A, B, C;
        fp_poly_from_roots(&A, roots_a, 5);
        fp_poly_from_roots(&B, roots_b, 33);
        fp_poly_mul(&C, &A, &B);

        fp_fe result;
        fp_poly_eval(result, &C, roots_a[2]);
        unsigned char rb[32];
        fp_tobytes(rb, result);
        check_bytes("fp mixed-size: C(root_a[2]) == 0", zero_bytes, rb, 32);

        check_int("fp mixed-size degree", 39, (int)C.coeffs.size());
    }
}
