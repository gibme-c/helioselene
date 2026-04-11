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

#ifdef RANSHAW_ECFFT
#include "ecfft_fp.h"
#include "ecfft_fq.h"

void test_ecfft()
{
    std::cout << std::endl << "=== ECFFT ===" << std::endl;

    /* ---- Fp ECFFT ---- */
    {
        /* Test init */
        ecfft_fp_ctx ctx = {};
        ecfft_fp_init(&ctx);
        check_int("fp ecfft domain_size", (int)ECFFT_FP_DOMAIN_SIZE, (int)ctx.domain_size);
        check_int("fp ecfft log_n", (int)ECFFT_FP_LOG_DOMAIN, (int)ctx.log_n);

        /* Test ENTER/EXIT round-trip with small polynomial */
        /* Polynomial: f(x) = 3 + 2x (degree 1, needs domain >= 2) */
        {
            fp_fe data[16];
            unsigned char buf3[32] = {0x03};
            unsigned char buf2[32] = {0x02};
            fp_frombytes(data[0], buf3);
            fp_frombytes(data[1], buf2);
            for (size_t i = 2; i < 16; i++)
                fp_0(data[i]);

            /* Save original coefficients */
            fp_fe orig0, orig1;
            fp_copy(orig0, data[0]);
            fp_copy(orig1, data[1]);

            /* Find the level where n == 16 */
            size_t enter_level = 0;
            for (size_t lv = 0; lv < ctx.log_n; lv++)
                if (ctx.levels[lv].n == 16)
                {
                    enter_level = lv;
                    break;
                }

            /* ENTER: coeff -> eval */
            ecfft_fp_enter(data, 16, &ctx);

            /* Verify: data[0] should be f(s[0]) = 3 + 2*s[0] */
            fp_fe expected, t;
            fp_frombytes(expected, buf3); /* 3 */
            fp_mul(t, orig1, ctx.levels[enter_level].s[0].v);
            fp_add(expected, expected, t);
            fp_fe zero;
            fp_0(zero);
            fp_sub(expected, expected, zero); /* normalize */

            unsigned char exp_b[32], act_b[32];
            fp_tobytes(exp_b, expected);
            fp_tobytes(act_b, data[0]);
            check_bytes("fp ecfft enter: f(s[0]) correct", exp_b, act_b, 32);

            /* EXIT: eval -> coeff */
            ecfft_fp_exit(data, 16, &ctx);

            /* Should recover original coefficients */
            fp_tobytes(exp_b, orig0);
            fp_tobytes(act_b, data[0]);
            check_bytes("fp ecfft enter/exit round-trip: coeff[0]", exp_b, act_b, 32);

            fp_tobytes(exp_b, orig1);
            fp_tobytes(act_b, data[1]);
            check_bytes("fp ecfft enter/exit round-trip: coeff[1]", exp_b, act_b, 32);

            /* Coefficients 2..15 should be zero */
            unsigned char zero_b[32] = {0};
            fp_tobytes(act_b, data[2]);
            check_bytes("fp ecfft enter/exit round-trip: coeff[2]==0", zero_b, act_b, 32);
        }

        /* Test ECFFT polynomial multiplication: (1 + x)(1 + x) = 1 + 2x + x^2 */
        {
            fp_fe a[2], b[2], result[16];
            unsigned char buf1[32] = {0x01};
            fp_frombytes(a[0], buf1);
            fp_frombytes(a[1], buf1);
            fp_frombytes(b[0], buf1);
            fp_frombytes(b[1], buf1);

            size_t result_len = 0;
            ecfft_fp_poly_mul(result, &result_len, a, 2, b, 2, &ctx);
            check_int("fp ecfft mul: result_len", 3, (int)result_len);

            /* Expected: [1, 2, 1] */
            unsigned char act[32];

            unsigned char one[32] = {0x01};
            unsigned char two[32] = {0x02};

            fp_tobytes(act, result[0]);
            check_bytes("fp ecfft mul: (1+x)^2 coeff[0]=1", one, act, 32);

            fp_tobytes(act, result[1]);
            check_bytes("fp ecfft mul: (1+x)^2 coeff[1]=2", two, act, 32);

            fp_tobytes(act, result[2]);
            check_bytes("fp ecfft mul: (1+x)^2 coeff[2]=1", one, act, 32);
        }

        /* Test ECFFT multiply matches schoolbook for degree-4 * degree-4 */
        {
            fp_fe a[5], b[5];
            for (int i = 0; i < 5; i++)
            {
                unsigned char buf[32] = {};
                buf[0] = (unsigned char)(i + 1);
                fp_frombytes(a[i], buf);
                buf[0] = (unsigned char)(i + 6);
                fp_frombytes(b[i], buf);
            }

            /* ECFFT multiply */
            fp_fe ecfft_result[16];
            size_t ecfft_len = 0;
            ecfft_fp_poly_mul(ecfft_result, &ecfft_len, a, 5, b, 5, &ctx);

            /* Verify via evaluation: A(x)*B(x) should equal result(x) at test point */
            check_int("fp ecfft mul deg4: result_len", 9, (int)ecfft_len);

            fp_poly pa, pb, pc;
            pa.coeffs.resize(5);
            pb.coeffs.resize(5);
            pc.coeffs.resize(ecfft_len);
            for (size_t i = 0; i < 5; i++)
            {
                fp_copy(pa.coeffs[i].v, a[i]);
                fp_copy(pb.coeffs[i].v, b[i]);
            }
            for (size_t i = 0; i < ecfft_len; i++)
                fp_copy(pc.coeffs[i].v, ecfft_result[i]);

            fp_fe test_x;
            unsigned char xbuf[32] = {0x37};
            fp_frombytes(test_x, xbuf);

            fp_fe va, vb, vc, vab;
            fp_poly_eval(va, &pa, test_x);
            fp_poly_eval(vb, &pb, test_x);
            fp_mul(vab, va, vb);
            fp_poly_eval(vc, &pc, test_x);

            unsigned char eb[32], ab[32];
            fp_tobytes(eb, vab);
            fp_tobytes(ab, vc);
            check_bytes("fp ecfft mul deg4: C(x)==A(x)*B(x)", eb, ab, 32);
        }

        /* Test dispatch integration: init global context, verify poly_mul uses it */
        {
            ecfft_global_init();

            fp_poly pa, pb, pc_ecfft;
            pa.coeffs.resize(9);
            pb.coeffs.resize(9);
            for (size_t i = 0; i < 9; i++)
            {
                unsigned char buf[32] = {};
                buf[0] = (unsigned char)(i + 1);
                fp_frombytes(pa.coeffs[i].v, buf);
                buf[0] = (unsigned char)(i + 10);
                fp_frombytes(pb.coeffs[i].v, buf);
            }

            /* This should use ECFFT since both are >= ECFFT_THRESHOLD */
            fp_poly_mul(&pc_ecfft, &pa, &pb);

            /* Verify by evaluating at a test point */
            fp_fe test_x;
            unsigned char test_buf[32] = {0x42};
            fp_frombytes(test_x, test_buf);

            fp_fe val_a, val_b, val_c, val_ab;
            fp_poly_eval(val_a, &pa, test_x);
            fp_poly_eval(val_b, &pb, test_x);
            fp_mul(val_ab, val_a, val_b);
            fp_poly_eval(val_c, &pc_ecfft, test_x);

            unsigned char eb[32], ab[32];
            fp_tobytes(eb, val_ab);
            fp_tobytes(ab, val_c);
            check_bytes("fp ecfft dispatch: C(x) == A(x)*B(x)", eb, ab, 32);
        }
    }

    /* ---- Fq ECFFT ---- */
    {
        ecfft_fq_ctx ctx = {};
        ecfft_fq_init(&ctx);

        /* Test ECFFT polynomial multiplication: (1 + x)(1 + x) = 1 + 2x + x^2 */
        {
            fq_fe a[2], b[2], result[16];
            unsigned char buf1[32] = {0x01};
            fq_frombytes(a[0], buf1);
            fq_frombytes(a[1], buf1);
            fq_frombytes(b[0], buf1);
            fq_frombytes(b[1], buf1);

            size_t result_len = 0;
            ecfft_fq_poly_mul(result, &result_len, a, 2, b, 2, &ctx);
            check_int("fq ecfft mul: result_len", 3, (int)result_len);

            unsigned char act[32];
            unsigned char one[32] = {0x01};
            unsigned char two[32] = {0x02};

            fq_tobytes(act, result[0]);
            check_bytes("fq ecfft mul: (1+x)^2 coeff[0]=1", one, act, 32);

            fq_tobytes(act, result[1]);
            check_bytes("fq ecfft mul: (1+x)^2 coeff[1]=2", two, act, 32);

            fq_tobytes(act, result[2]);
            check_bytes("fq ecfft mul: (1+x)^2 coeff[2]=1", one, act, 32);
        }

        /* Test ECFFT multiply matches schoolbook for degree-4 * degree-4 */
        {
            fq_fe a[5], b[5];
            for (int i = 0; i < 5; i++)
            {
                unsigned char buf[32] = {};
                buf[0] = (unsigned char)(i + 1);
                fq_frombytes(a[i], buf);
                buf[0] = (unsigned char)(i + 6);
                fq_frombytes(b[i], buf);
            }

            fq_fe ecfft_result[16];
            size_t ecfft_len = 0;
            ecfft_fq_poly_mul(ecfft_result, &ecfft_len, a, 5, b, 5, &ctx);

            check_int("fq ecfft mul deg4: result_len", 9, (int)ecfft_len);

            fq_poly pa, pb, pc;
            pa.coeffs.resize(5);
            pb.coeffs.resize(5);
            pc.coeffs.resize(ecfft_len);
            for (size_t i = 0; i < 5; i++)
            {
                fq_copy(pa.coeffs[i].v, a[i]);
                fq_copy(pb.coeffs[i].v, b[i]);
            }
            for (size_t i = 0; i < ecfft_len; i++)
                fq_copy(pc.coeffs[i].v, ecfft_result[i]);

            fq_fe test_x;
            unsigned char xbuf[32] = {0x37};
            fq_frombytes(test_x, xbuf);

            fq_fe va, vb, vc, vab;
            fq_poly_eval(va, &pa, test_x);
            fq_poly_eval(vb, &pb, test_x);
            fq_mul(vab, va, vb);
            fq_poly_eval(vc, &pc, test_x);

            unsigned char eb[32], ab[32];
            fq_tobytes(eb, vab);
            fq_tobytes(ab, vc);
            check_bytes("fq ecfft mul deg4: C(x)==A(x)*B(x)", eb, ab, 32);
        }

        /* Test dispatch integration */
        {
            ecfft_global_init();

            fq_poly pa, pb, pc_ecfft;
            pa.coeffs.resize(9);
            pb.coeffs.resize(9);
            for (size_t i = 0; i < 9; i++)
            {
                unsigned char buf[32] = {};
                buf[0] = (unsigned char)(i + 1);
                fq_frombytes(pa.coeffs[i].v, buf);
                buf[0] = (unsigned char)(i + 10);
                fq_frombytes(pb.coeffs[i].v, buf);
            }

            fq_poly_mul(&pc_ecfft, &pa, &pb);

            fq_fe test_x;
            unsigned char test_buf[32] = {0x42};
            fq_frombytes(test_x, test_buf);

            fq_fe val_a, val_b, val_c, val_ab;
            fq_poly_eval(val_a, &pa, test_x);
            fq_poly_eval(val_b, &pb, test_x);
            fq_mul(val_ab, val_a, val_b);
            fq_poly_eval(val_c, &pc_ecfft, test_x);

            unsigned char eb[32], ab[32];
            fq_tobytes(eb, val_ab);
            fq_tobytes(ab, val_c);
            check_bytes("fq ecfft dispatch: C(x) == A(x)*B(x)", eb, ab, 32);
        }
    }
}
#endif /* RANSHAW_ECFFT */
