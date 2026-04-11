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

void test_ran_divisor()
{
    std::cout << std::endl << "=== Ran divisor ===" << std::endl;
    unsigned char buf[32];

    ran_jacobian G;
    fp_copy(G.X, RAN_GX);
    fp_copy(G.Y, RAN_GY);
    fp_1(G.Z);

    /* Get a few affine points on the curve */
    ran_jacobian G2, G3, G4;
    ran_dbl(&G2, &G);
    ran_add(&G3, &G2, &G);
    ran_dbl(&G4, &G2);

    ran_affine pts[3];
    ran_to_affine(&pts[0], &G);
    ran_to_affine(&pts[1], &G2);
    ran_to_affine(&pts[2], &G3);

    /* Compute divisor */
    ran_divisor d;
    ran_compute_divisor(&d, pts, 3);

    /* Evaluate at each point: should give 0 */
    for (int i = 0; i < 3; i++)
    {
        fp_fe val;
        ran_evaluate_divisor(val, &d, pts[i].x, pts[i].y);
        fp_tobytes(buf, val);
        std::string name = "divisor eval at point " + std::to_string(i) + " == 0";
        check_bytes(name.c_str(), zero_bytes, buf, 32);
    }

    /* Evaluate at a different point: should NOT be 0 */
    {
        ran_affine p4;
        ran_to_affine(&p4, &G4);
        fp_fe val;
        ran_evaluate_divisor(val, &d, p4.x, p4.y);
        fp_tobytes(buf, val);
        check_nonzero("divisor eval at non-member != 0", std::memcmp(buf, zero_bytes, 32) != 0 ? 1 : 0);
    }

    /* Single point divisor */
    {
        ran_divisor d1;
        ran_compute_divisor(&d1, pts, 1);
        fp_fe val;
        ran_evaluate_divisor(val, &d1, pts[0].x, pts[0].y);
        fp_tobytes(buf, val);
        check_bytes("single-point divisor eval == 0", zero_bytes, buf, 32);
    }
}


void test_shaw_divisor()
{
    std::cout << std::endl << "=== Shaw divisor ===" << std::endl;
    unsigned char buf[32];

    shaw_jacobian G;
    fq_copy(G.X, SHAW_GX);
    fq_copy(G.Y, SHAW_GY);
    fq_1(G.Z);

    shaw_jacobian G2, G3;
    shaw_dbl(&G2, &G);
    shaw_add(&G3, &G2, &G);

    shaw_affine pts[2];
    shaw_to_affine(&pts[0], &G);
    shaw_to_affine(&pts[1], &G2);

    shaw_divisor d;
    shaw_compute_divisor(&d, pts, 2);

    for (int i = 0; i < 2; i++)
    {
        fq_fe val;
        shaw_evaluate_divisor(val, &d, pts[i].x, pts[i].y);
        fq_tobytes(buf, val);
        std::string name = "divisor eval at point " + std::to_string(i) + " == 0";
        check_bytes(name.c_str(), zero_bytes, buf, 32);
    }

    /* Non-member check */
    {
        shaw_affine p3;
        shaw_to_affine(&p3, &G3);
        fq_fe val;
        shaw_evaluate_divisor(val, &d, p3.x, p3.y);
        fq_tobytes(buf, val);
        check_nonzero("divisor eval at non-member != 0", std::memcmp(buf, zero_bytes, 32) != 0 ? 1 : 0);
    }
}

/* ========================================================================
 * Extended tests for pre-SIMD hardening
 * ======================================================================== */


void test_divisor_extended()
{
    std::cout << std::endl << "=== Divisor extended ===" << std::endl;
    unsigned char buf[32];

    /* Ran: 5-point divisor */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_jacobian pts_jac[6];
        ran_copy(&pts_jac[0], &G);
        ran_dbl(&pts_jac[1], &G);
        ran_add(&pts_jac[2], &pts_jac[1], &G);
        ran_dbl(&pts_jac[3], &pts_jac[1]);
        ran_add(&pts_jac[4], &pts_jac[3], &G);
        ran_add(&pts_jac[5], &pts_jac[4], &G); /* non-member */

        ran_affine pts[5], non_member;
        for (int i = 0; i < 5; i++)
            ran_to_affine(&pts[i], &pts_jac[i]);
        ran_to_affine(&non_member, &pts_jac[5]);

        ran_divisor d;
        ran_compute_divisor(&d, pts, 5);

        bool all_zero = true;
        for (int i = 0; i < 5; i++)
        {
            fp_fe val;
            ran_evaluate_divisor(val, &d, pts[i].x, pts[i].y);
            unsigned char vb[32];
            fp_tobytes(vb, val);
            if (std::memcmp(vb, zero_bytes, 32) != 0)
                all_zero = false;
        }
        ++tests_run;
        if (all_zero)
        {
            ++tests_passed;
            std::cout << "  PASS: ran 5-point divisor all evals == 0" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: ran 5-point divisor some eval != 0" << std::endl;
        }

        fp_fe val;
        ran_evaluate_divisor(val, &d, non_member.x, non_member.y);
        fp_tobytes(buf, val);
        check_nonzero("ran 5-point divisor non-member != 0", std::memcmp(buf, zero_bytes, 32) != 0 ? 1 : 0);
    }

    /* Shaw: single-point divisor */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_affine pt;
        shaw_to_affine(&pt, &G);

        shaw_divisor d;
        shaw_compute_divisor(&d, &pt, 1);

        fq_fe val;
        shaw_evaluate_divisor(val, &d, pt.x, pt.y);
        fq_tobytes(buf, val);
        check_bytes("shaw single-point divisor eval == 0", zero_bytes, buf, 32);
    }

    /* Shaw: 5-point divisor */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_jacobian pts_jac[6];
        shaw_copy(&pts_jac[0], &G);
        shaw_dbl(&pts_jac[1], &G);
        shaw_add(&pts_jac[2], &pts_jac[1], &G);
        shaw_dbl(&pts_jac[3], &pts_jac[1]);
        shaw_add(&pts_jac[4], &pts_jac[3], &G);
        shaw_add(&pts_jac[5], &pts_jac[4], &G);

        shaw_affine pts[5], non_member;
        for (int i = 0; i < 5; i++)
            shaw_to_affine(&pts[i], &pts_jac[i]);
        shaw_to_affine(&non_member, &pts_jac[5]);

        shaw_divisor d;
        shaw_compute_divisor(&d, pts, 5);

        bool all_zero = true;
        for (int i = 0; i < 5; i++)
        {
            fq_fe val;
            shaw_evaluate_divisor(val, &d, pts[i].x, pts[i].y);
            unsigned char vb[32];
            fq_tobytes(vb, val);
            if (std::memcmp(vb, zero_bytes, 32) != 0)
                all_zero = false;
        }
        ++tests_run;
        if (all_zero)
        {
            ++tests_passed;
            std::cout << "  PASS: shaw 5-point divisor all evals == 0" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: shaw 5-point divisor some eval != 0" << std::endl;
        }
    }
}


void test_eval_divisor()
{
    std::cout << std::endl << "=== Eval-domain divisor ===" << std::endl;
    unsigned char buf[32];

    ran_eval_divisor_init();
    shaw_eval_divisor_init();

    /* Test 1: fp_evals roundtrip — evaluate known poly at domain, interpolate back */
    {
        /* p(x) = 3x^2 + 5x + 7 */
        fp_poly p;
        p.coeffs.resize(3);
        fp_fe c7, c5, c3;
        unsigned char b7[32] = {7}, b5[32] = {5}, b3[32] = {3};
        fp_frombytes(c7, b7);
        fp_frombytes(c5, b5);
        fp_frombytes(c3, b3);
        fp_copy(p.coeffs[0].v, c7);
        fp_copy(p.coeffs[1].v, c5);
        fp_copy(p.coeffs[2].v, c3);

        /* Evaluate at domain points */
        fp_evals ev;
        ev.degree = 2;
        for (size_t i = 0; i < EVAL_DOMAIN_SIZE; i++)
        {
            fp_fe xi;
            unsigned char xb[32] = {};
            xb[0] = (unsigned char)(i & 0xff);
            if (i > 255)
                xb[1] = (unsigned char)((i >> 8) & 0xff);
            fp_frombytes(xi, xb);
            fp_fe tmp_ev;
            fp_poly_eval(tmp_ev, &p, xi);
            fp_evals_set(&ev, i, tmp_ev);
        }

        /* Interpolate back */
        fp_poly recovered;
        fp_evals_to_poly(&recovered, &ev);

        /* Check coefficients match */
        bool match = (recovered.coeffs.size() == 3);
        if (match)
        {
            for (size_t i = 0; i < 3; i++)
            {
                unsigned char eb[32], rb[32];
                fp_tobytes(eb, p.coeffs[i].v);
                fp_tobytes(rb, recovered.coeffs[i].v);
                if (std::memcmp(eb, rb, 32) != 0)
                    match = false;
            }
        }
        ++tests_run;
        if (match)
        {
            ++tests_passed;
            std::cout << "  PASS: fp_evals roundtrip" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: fp_evals roundtrip" << std::endl;
        }
    }

    /* Test 2: fp_evals_mul matches fp_poly_mul */
    {
        /* a(x) = 2x + 1, b(x) = x + 3 */
        fp_poly pa, pb, pc;
        pa.coeffs.resize(2);
        pb.coeffs.resize(2);
        fp_fe c1, c2, c3_val;
        unsigned char b1[32] = {1}, b2[32] = {2}, b3v[32] = {3};
        fp_frombytes(c1, b1);
        fp_frombytes(c2, b2);
        fp_frombytes(c3_val, b3v);
        fp_copy(pa.coeffs[0].v, c1);
        fp_copy(pa.coeffs[1].v, c2);
        fp_copy(pb.coeffs[0].v, c3_val);
        fp_copy(pb.coeffs[1].v, c1);

        /* Poly mul for reference */
        fp_poly_mul(&pc, &pa, &pb);

        /* Eval-domain multiplication */
        fp_evals ea, eb, ec;
        ea.degree = 1;
        eb.degree = 1;
        for (size_t i = 0; i < EVAL_DOMAIN_SIZE; i++)
        {
            fp_fe xi;
            unsigned char xb[32] = {};
            xb[0] = (unsigned char)(i & 0xff);
            fp_frombytes(xi, xb);
            fp_fe tmp_a, tmp_b;
            fp_poly_eval(tmp_a, &pa, xi);
            fp_poly_eval(tmp_b, &pb, xi);
            fp_evals_set(&ea, i, tmp_a);
            fp_evals_set(&eb, i, tmp_b);
        }
        fp_evals_mul(&ec, &ea, &eb);

        /* Convert back */
        fp_poly pc_eval;
        fp_evals_to_poly(&pc_eval, &ec);

        /* Compare */
        bool match = (pc_eval.coeffs.size() == pc.coeffs.size());
        if (match)
        {
            for (size_t i = 0; i < pc.coeffs.size(); i++)
            {
                unsigned char eb2[32], rb[32];
                fp_tobytes(eb2, pc.coeffs[i].v);
                fp_tobytes(rb, pc_eval.coeffs[i].v);
                if (std::memcmp(eb2, rb, 32) != 0)
                    match = false;
            }
        }
        ++tests_run;
        if (match)
        {
            ++tests_passed;
            std::cout << "  PASS: fp_evals_mul matches poly_mul" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: fp_evals_mul matches poly_mul" << std::endl;
        }
    }

    /* Test 3: fp_evals_div_linear */
    {
        /* f(x) = (x-300)(x-400)(x-500) evaluated at domain, then divide by (x-300) */
        fp_fe r300, r400, r500;
        unsigned char b300[32] = {}, b400[32] = {}, b500[32] = {};
        b300[0] = 0x2c;
        b300[1] = 0x01; /* 300 */
        b400[0] = 0x90;
        b400[1] = 0x01; /* 400 */
        b500[0] = 0xf4;
        b500[1] = 0x01; /* 500 */
        fp_frombytes(r300, b300);
        fp_frombytes(r400, b400);
        fp_frombytes(r500, b500);

        /* Build (x-300)(x-400)(x-500) via roots */
        fp_fe roots[3];
        fp_copy(roots[0], r300);
        fp_copy(roots[1], r400);
        fp_copy(roots[2], r500);
        fp_poly f;
        fp_poly_from_roots(&f, roots, 3);

        /* Evaluate at domain */
        fp_evals ef;
        ef.degree = 3;
        for (size_t i = 0; i < EVAL_DOMAIN_SIZE; i++)
        {
            fp_fe xi;
            unsigned char xb[32] = {};
            xb[0] = (unsigned char)(i & 0xff);
            fp_frombytes(xi, xb);
            fp_fe tmp_f;
            fp_poly_eval(tmp_f, &f, xi);
            fp_evals_set(&ef, i, tmp_f);
        }

        /* Divide by (x - 300) */
        fp_evals eq;
        fp_evals_div_linear(&eq, &ef, r300);

        /* Expected: (x-400)(x-500) */
        fp_fe roots2[2];
        fp_copy(roots2[0], r400);
        fp_copy(roots2[1], r500);
        fp_poly expected;
        fp_poly_from_roots(&expected, roots2, 2);

        /* Convert eq back to poly and compare */
        fp_poly got;
        fp_evals_to_poly(&got, &eq);

        bool match = (got.coeffs.size() == expected.coeffs.size());
        if (match)
        {
            for (size_t i = 0; i < expected.coeffs.size(); i++)
            {
                unsigned char eb2[32], gb[32];
                fp_tobytes(eb2, expected.coeffs[i].v);
                fp_tobytes(gb, got.coeffs[i].v);
                if (std::memcmp(eb2, gb, 32) != 0)
                    match = false;
            }
        }
        ++tests_run;
        if (match)
        {
            ++tests_passed;
            std::cout << "  PASS: fp_evals_div_linear" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: fp_evals_div_linear" << std::endl;
        }
    }

    /* Test 4: eval_divisor_from_point — verify vanishes at point */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_affine pt;
        ran_to_affine(&pt, &G);

        ran_eval_divisor ed;
        ran_eval_divisor_from_point(&ed, &pt);

        ran_divisor d;
        ran_eval_divisor_to_divisor(&d, &ed);

        fp_fe val;
        ran_evaluate_divisor(val, &d, pt.x, pt.y);
        fp_tobytes(buf, val);
        check_bytes("eval_divisor_from_point vanishes at P", zero_bytes, buf, 32);

        /* Also matches ran_compute_divisor for 1 point */
        ran_divisor d_ref;
        ran_compute_divisor(&d_ref, &pt, 1);
        unsigned char ab[32], rb[32];
        fp_tobytes(ab, d.a.coeffs[0].v);
        fp_tobytes(rb, d_ref.a.coeffs[0].v);
        check_bytes("eval_divisor_from_point matches compute_divisor a[0]", rb, ab, 32);
        fp_tobytes(ab, d.b.coeffs[0].v);
        fp_tobytes(rb, d_ref.b.coeffs[0].v);
        check_bytes("eval_divisor_from_point matches compute_divisor b[0]", rb, ab, 32);
    }

    /* Test 5: eval_divisor_mul — multiply two single-point divisors */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);
        ran_jacobian G2;
        ran_dbl(&G2, &G);

        ran_affine p1, p2;
        ran_to_affine(&p1, &G);
        ran_to_affine(&p2, &G2);

        ran_eval_divisor ed1, ed2, ed_prod;
        ran_eval_divisor_from_point(&ed1, &p1);
        ran_eval_divisor_from_point(&ed2, &p2);
        ran_eval_divisor_mul(&ed_prod, &ed1, &ed2);

        ran_divisor d;
        ran_eval_divisor_to_divisor(&d, &ed_prod);

        /* Should vanish at both points */
        fp_fe val;
        ran_evaluate_divisor(val, &d, p1.x, p1.y);
        fp_tobytes(buf, val);
        check_bytes("eval_divisor_mul vanishes at P1", zero_bytes, buf, 32);

        ran_evaluate_divisor(val, &d, p2.x, p2.y);
        fp_tobytes(buf, val);
        check_bytes("eval_divisor_mul vanishes at P2", zero_bytes, buf, 32);

        /* Should NOT vanish at a third point */
        ran_jacobian G3;
        ran_add(&G3, &G2, &G);
        ran_affine p3;
        ran_to_affine(&p3, &G3);
        ran_evaluate_divisor(val, &d, p3.x, p3.y);
        fp_tobytes(buf, val);
        check_nonzero("eval_divisor_mul nonzero at P3", std::memcmp(buf, zero_bytes, 32) != 0 ? 1 : 0);
    }

    /* Test 6: fq_evals roundtrip */
    {
        fq_poly p;
        p.coeffs.resize(2);
        fq_fe c1, c2;
        unsigned char b1[32] = {1}, b2[32] = {2};
        fq_frombytes(c1, b1);
        fq_frombytes(c2, b2);
        fq_copy(p.coeffs[0].v, c1);
        fq_copy(p.coeffs[1].v, c2);

        fq_evals ev;
        ev.degree = 1;
        for (size_t i = 0; i < EVAL_DOMAIN_SIZE; i++)
        {
            fq_fe xi;
            unsigned char xb[32] = {};
            xb[0] = (unsigned char)(i & 0xff);
            fq_frombytes(xi, xb);
            fq_fe tmp_ev;
            fq_poly_eval(tmp_ev, &p, xi);
            fq_evals_set(&ev, i, tmp_ev);
        }

        fq_poly recovered;
        fq_evals_to_poly(&recovered, &ev);

        bool match = (recovered.coeffs.size() == 2);
        if (match)
        {
            for (size_t i = 0; i < 2; i++)
            {
                unsigned char eb2[32], rb[32];
                fq_tobytes(eb2, p.coeffs[i].v);
                fq_tobytes(rb, recovered.coeffs[i].v);
                if (std::memcmp(eb2, rb, 32) != 0)
                    match = false;
            }
        }
        ++tests_run;
        if (match)
        {
            ++tests_passed;
            std::cout << "  PASS: fq_evals roundtrip" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: fq_evals roundtrip" << std::endl;
        }
    }

    /* Test 7: shaw eval_divisor_from_point */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_affine pt;
        shaw_to_affine(&pt, &G);

        shaw_eval_divisor ed;
        shaw_eval_divisor_from_point(&ed, &pt);

        shaw_divisor d;
        shaw_eval_divisor_to_divisor(&d, &ed);

        fq_fe val;
        shaw_evaluate_divisor(val, &d, pt.x, pt.y);
        fq_tobytes(buf, val);
        check_bytes("shaw eval_divisor_from_point vanishes at P", zero_bytes, buf, 32);
    }

    /* Test 8: shaw eval_divisor_mul */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);
        shaw_jacobian G2;
        shaw_dbl(&G2, &G);

        shaw_affine p1, p2;
        shaw_to_affine(&p1, &G);
        shaw_to_affine(&p2, &G2);

        shaw_eval_divisor ed1, ed2, ed_prod;
        shaw_eval_divisor_from_point(&ed1, &p1);
        shaw_eval_divisor_from_point(&ed2, &p2);
        shaw_eval_divisor_mul(&ed_prod, &ed1, &ed2);

        shaw_divisor d;
        shaw_eval_divisor_to_divisor(&d, &ed_prod);

        fq_fe val;
        shaw_evaluate_divisor(val, &d, p1.x, p1.y);
        fq_tobytes(buf, val);
        check_bytes("shaw eval_divisor_mul vanishes at P1", zero_bytes, buf, 32);

        shaw_evaluate_divisor(val, &d, p2.x, p2.y);
        fq_tobytes(buf, val);
        check_bytes("shaw eval_divisor_mul vanishes at P2", zero_bytes, buf, 32);
    }

    /* Test 9: ran eval divisor merge (2 single-point divisors) */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);
        ran_jacobian G2;
        ran_dbl(&G2, &G);

        ran_affine p1, p2;
        ran_to_affine(&p1, &G);
        ran_to_affine(&p2, &G2);

        ran_jacobian G3;
        ran_add(&G3, &G, &G2);
        ran_affine sum;
        ran_to_affine(&sum, &G3);

        ran_eval_divisor ed1, ed2, merged;
        ran_eval_divisor_from_point(&ed1, &p1);
        ran_eval_divisor_from_point(&ed2, &p2);
        ran_eval_divisor_merge(&merged, &ed1, &ed2, &p1, &p2, &sum);

        ran_divisor d;
        ran_eval_divisor_to_divisor(&d, &merged);

        /* Should vanish at both points */
        fp_fe val;
        ran_evaluate_divisor(val, &d, p1.x, p1.y);
        fp_tobytes(buf, val);
        check_bytes("ran merge vanishes at P1", zero_bytes, buf, 32);

        ran_evaluate_divisor(val, &d, p2.x, p2.y);
        fp_tobytes(buf, val);
        check_bytes("ran merge vanishes at P2", zero_bytes, buf, 32);
    }

    /* Test 10: fq_evals_div_linear */
    {
        fq_fe r300, r400;
        unsigned char b300[32] = {}, b400[32] = {};
        b300[0] = 0x2c;
        b300[1] = 0x01;
        b400[0] = 0x90;
        b400[1] = 0x01;
        fq_frombytes(r300, b300);
        fq_frombytes(r400, b400);

        fq_fe roots[2];
        fq_copy(roots[0], r300);
        fq_copy(roots[1], r400);
        fq_poly f;
        fq_poly_from_roots(&f, roots, 2);

        fq_evals ef;
        ef.degree = 2;
        for (size_t i = 0; i < EVAL_DOMAIN_SIZE; i++)
        {
            fq_fe xi;
            unsigned char xb[32] = {};
            xb[0] = (unsigned char)(i & 0xff);
            fq_frombytes(xi, xb);
            fq_fe tmp_f;
            fq_poly_eval(tmp_f, &f, xi);
            fq_evals_set(&ef, i, tmp_f);
        }

        fq_evals eq;
        fq_evals_div_linear(&eq, &ef, r300);

        /* Expected: (x-400) = x - 400 */
        fq_fe roots2[1];
        fq_copy(roots2[0], r400);
        fq_poly expected;
        fq_poly_from_roots(&expected, roots2, 1);

        fq_poly got;
        fq_evals_to_poly(&got, &eq);

        bool match = (got.coeffs.size() == expected.coeffs.size());
        if (match)
        {
            for (size_t i = 0; i < expected.coeffs.size(); i++)
            {
                unsigned char eb2[32], gb[32];
                fq_tobytes(eb2, expected.coeffs[i].v);
                fq_tobytes(gb, got.coeffs[i].v);
                if (std::memcmp(eb2, gb, 32) != 0)
                    match = false;
            }
        }
        ++tests_run;
        if (match)
        {
            ++tests_passed;
            std::cout << "  PASS: fq_evals_div_linear" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: fq_evals_div_linear" << std::endl;
        }
    }
}
