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

void test_ran_msm()
{
    std::cout << std::endl << "=== Ran MSM ===" << std::endl;
    unsigned char buf[32];

    ran_jacobian G;
    fp_copy(G.X, RAN_GX);
    fp_copy(G.Y, RAN_GY);
    fp_1(G.Z);

    /* msm([1], [G]) == G */
    ran_jacobian result;
    ran_msm_vartime(&result, one_bytes, &G, 1);
    ran_tobytes(buf, &result);
    check_bytes("msm([1], [G]) == G", tv::compressed_points::ran_g, buf, 32);

    /* msm([7], [G]) == 7*G */
    unsigned char seven_scalar[32] = {0x07};
    ran_msm_vartime(&result, seven_scalar, &G, 1);
    ran_tobytes(buf, &result);
    check_bytes("msm([7], [G]) == 7G", tv::compressed_points::ran_7g, buf, 32);

    /* msm([0], [G]) == identity */
    ran_msm_vartime(&result, zero_bytes, &G, 1);
    check_nonzero("msm([0], [G]) == identity", ran_is_identity(&result));

    /* msm([], []) == identity (n=0) */
    ran_msm_vartime(&result, nullptr, nullptr, 0);
    check_nonzero("msm([], []) == identity", ran_is_identity(&result));

    /* Linearity: msm([2, 5], [G, G]) == 7*G */
    unsigned char two_scalar[32] = {};
    two_scalar[0] = 0x02;
    unsigned char five_scalar[32] = {};
    five_scalar[0] = 0x05;
    unsigned char scalars_2_5[64];
    std::memcpy(scalars_2_5, two_scalar, 32);
    std::memcpy(scalars_2_5 + 32, five_scalar, 32);
    ran_jacobian points_2[2];
    ran_copy(&points_2[0], &G);
    ran_copy(&points_2[1], &G);
    ran_msm_vartime(&result, scalars_2_5, points_2, 2);
    ran_tobytes(buf, &result);
    check_bytes("msm([2,5], [G,G]) == 7G", tv::compressed_points::ran_7g, buf, 32);

    /* msm([a], [P]) == scalarmult_vartime(a, P) */
    unsigned char scalar_a[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
                                  0xca, 0xef, 0xbe, 0xad, 0xde, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                  0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    ran_jacobian sm_result;
    ran_scalarmult_vartime(&sm_result, scalar_a, &G);
    unsigned char sm_bytes[32];
    ran_tobytes(sm_bytes, &sm_result);
    ran_msm_vartime(&result, scalar_a, &G, 1);
    ran_tobytes(buf, &result);
    check_bytes("msm([a], [G]) == vartime(a, G)", sm_bytes, buf, 32);

    /* Two distinct points: msm([a, b], [G, 2G]) == a*G + b*2G */
    ran_jacobian G2;
    ran_dbl(&G2, &G);
    unsigned char scalar_b[32] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x0d, 0xf0, 0xad,
                                  0xba, 0xce, 0xfa, 0xed, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char scalars_ab[64];
    std::memcpy(scalars_ab, scalar_a, 32);
    std::memcpy(scalars_ab + 32, scalar_b, 32);
    ran_jacobian points_ab[2];
    ran_copy(&points_ab[0], &G);
    ran_copy(&points_ab[1], &G2);
    ran_msm_vartime(&result, scalars_ab, points_ab, 2);
    ran_tobytes(buf, &result);

    ran_jacobian aG, bG2, expected;
    ran_scalarmult_vartime(&aG, scalar_a, &G);
    ran_scalarmult_vartime(&bG2, scalar_b, &G2);
    ran_add(&expected, &aG, &bG2);
    unsigned char expected_bytes[32];
    ran_tobytes(expected_bytes, &expected);
    check_bytes("msm([a,b], [G,2G]) == a*G + b*2G", expected_bytes, buf, 32);

    /* n=8 (exercises Straus): all scalars=1, all points=G → sum = 8*G */
    {
        unsigned char scalars8[8 * 32] = {};
        ran_jacobian points8[8];
        for (int i = 0; i < 8; i++)
        {
            scalars8[i * 32] = 0x01;
            ran_copy(&points8[i], &G);
        }
        /* 8*G via repeated doubling */
        unsigned char eight_scalar[32] = {0x08};
        ran_jacobian eightG;
        ran_scalarmult_vartime(&eightG, eight_scalar, &G);
        ran_tobytes(expected_bytes, &eightG);
        ran_msm_vartime(&result, scalars8, points8, 8);
        ran_tobytes(buf, &result);
        check_bytes("msm n=8 (Straus)", expected_bytes, buf, 32);
    }

    /* n=33 (crosses Straus/Pippenger boundary): all scalars=1, all points=G → 33*G */
    {
        unsigned char scalars33[33 * 32] = {};
        ran_jacobian points33[33];
        for (int i = 0; i < 33; i++)
        {
            scalars33[i * 32] = 0x01;
            ran_copy(&points33[i], &G);
        }
        unsigned char thirtythree_scalar[32] = {33};
        ran_jacobian expected_pt;
        ran_scalarmult_vartime(&expected_pt, thirtythree_scalar, &G);
        ran_tobytes(expected_bytes, &expected_pt);
        ran_msm_vartime(&result, scalars33, points33, 33);
        ran_tobytes(buf, &result);
        check_bytes("msm n=33 (Pippenger)", expected_bytes, buf, 32);
    }

    /* All-zero scalars → identity */
    {
        unsigned char zero_scalars[4 * 32] = {};
        ran_jacobian points4[4];
        for (int i = 0; i < 4; i++)
            ran_copy(&points4[i], &G);
        ran_msm_vartime(&result, zero_scalars, points4, 4);
        check_nonzero("msm all-zero scalars == identity", ran_is_identity(&result));
    }
}


void test_shaw_msm()
{
    std::cout << std::endl << "=== Shaw MSM ===" << std::endl;
    unsigned char buf[32];

    shaw_jacobian G;
    fq_copy(G.X, SHAW_GX);
    fq_copy(G.Y, SHAW_GY);
    fq_1(G.Z);

    /* msm([1], [G]) == G */
    shaw_jacobian result;
    shaw_msm_vartime(&result, one_bytes, &G, 1);
    shaw_tobytes(buf, &result);
    check_bytes("msm([1], [G]) == G", tv::compressed_points::shaw_g, buf, 32);

    /* msm([7], [G]) == 7*G */
    unsigned char seven_scalar[32] = {0x07};
    shaw_msm_vartime(&result, seven_scalar, &G, 1);
    shaw_tobytes(buf, &result);
    check_bytes("msm([7], [G]) == 7G", tv::compressed_points::shaw_7g, buf, 32);

    /* msm([0], [G]) == identity */
    shaw_msm_vartime(&result, zero_bytes, &G, 1);
    check_nonzero("msm([0], [G]) == identity", shaw_is_identity(&result));

    /* msm([], []) == identity (n=0) */
    shaw_msm_vartime(&result, nullptr, nullptr, 0);
    check_nonzero("msm([], []) == identity", shaw_is_identity(&result));

    /* Linearity: msm([2, 5], [G, G]) == 7*G */
    unsigned char two_scalar[32] = {};
    two_scalar[0] = 0x02;
    unsigned char five_scalar[32] = {};
    five_scalar[0] = 0x05;
    unsigned char scalars_2_5[64];
    std::memcpy(scalars_2_5, two_scalar, 32);
    std::memcpy(scalars_2_5 + 32, five_scalar, 32);
    shaw_jacobian points_2[2];
    shaw_copy(&points_2[0], &G);
    shaw_copy(&points_2[1], &G);
    shaw_msm_vartime(&result, scalars_2_5, points_2, 2);
    shaw_tobytes(buf, &result);
    check_bytes("msm([2,5], [G,G]) == 7G", tv::compressed_points::shaw_7g, buf, 32);

    /* msm([a], [P]) == scalarmult_vartime(a, P) */
    unsigned char scalar_a[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
                                  0xca, 0xef, 0xbe, 0xad, 0xde, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                  0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    shaw_jacobian sm_result;
    shaw_scalarmult_vartime(&sm_result, scalar_a, &G);
    unsigned char sm_bytes[32];
    shaw_tobytes(sm_bytes, &sm_result);
    shaw_msm_vartime(&result, scalar_a, &G, 1);
    shaw_tobytes(buf, &result);
    check_bytes("msm([a], [G]) == vartime(a, G)", sm_bytes, buf, 32);

    /* Two distinct points: msm([a, b], [G, 2G]) == a*G + b*2G */
    {
        shaw_jacobian G2;
        shaw_dbl(&G2, &G);
        unsigned char scalar_b[32] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x0d, 0xf0, 0xad,
                                      0xba, 0xce, 0xfa, 0xed, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        unsigned char scalars_ab[64];
        std::memcpy(scalars_ab, scalar_a, 32);
        std::memcpy(scalars_ab + 32, scalar_b, 32);
        shaw_jacobian points_ab[2];
        shaw_copy(&points_ab[0], &G);
        shaw_copy(&points_ab[1], &G2);
        shaw_msm_vartime(&result, scalars_ab, points_ab, 2);
        shaw_tobytes(buf, &result);

        shaw_jacobian aG, bG2, expected;
        shaw_scalarmult_vartime(&aG, scalar_a, &G);
        shaw_scalarmult_vartime(&bG2, scalar_b, &G2);
        shaw_add(&expected, &aG, &bG2);
        unsigned char expected_bytes[32];
        shaw_tobytes(expected_bytes, &expected);
        check_bytes("msm([a,b], [G,2G]) == a*G + b*2G", expected_bytes, buf, 32);
    }

    /* n=8 (exercises Straus): all scalars=1, all points=G → sum = 8*G */
    {
        unsigned char scalars8[8 * 32] = {};
        shaw_jacobian points8[8];
        for (int i = 0; i < 8; i++)
        {
            scalars8[i * 32] = 0x01;
            shaw_copy(&points8[i], &G);
        }
        unsigned char eight_scalar[32] = {0x08};
        shaw_jacobian eightG;
        shaw_scalarmult_vartime(&eightG, eight_scalar, &G);
        unsigned char expected_bytes[32];
        shaw_tobytes(expected_bytes, &eightG);
        shaw_msm_vartime(&result, scalars8, points8, 8);
        shaw_tobytes(buf, &result);
        check_bytes("msm n=8 (Straus)", expected_bytes, buf, 32);
    }

    /* n=33 (crosses Straus/Pippenger boundary): all scalars=1, all points=G → 33*G */
    {
        unsigned char scalars33[33 * 32] = {};
        shaw_jacobian points33[33];
        for (int i = 0; i < 33; i++)
        {
            scalars33[i * 32] = 0x01;
            shaw_copy(&points33[i], &G);
        }
        unsigned char thirtythree_scalar[32] = {33};
        shaw_jacobian expected_pt;
        shaw_scalarmult_vartime(&expected_pt, thirtythree_scalar, &G);
        unsigned char expected_bytes[32];
        shaw_tobytes(expected_bytes, &expected_pt);
        shaw_msm_vartime(&result, scalars33, points33, 33);
        shaw_tobytes(buf, &result);
        check_bytes("msm n=33 (Pippenger)", expected_bytes, buf, 32);
    }

    /* All-zero scalars → identity */
    {
        unsigned char zero_scalars[4 * 32] = {};
        shaw_jacobian points4[4];
        for (int i = 0; i < 4; i++)
            shaw_copy(&points4[i], &G);
        shaw_msm_vartime(&result, zero_scalars, points4, 4);
        check_nonzero("msm all-zero scalars == identity", shaw_is_identity(&result));
    }
}

/* SSWU test vectors loaded from generated header */


void test_msm_extended()
{
    std::cout << std::endl << "=== MSM extended ===" << std::endl;
    unsigned char buf[32];

    /* Ran: MSM with identity in array */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_jacobian id;
        ran_identity(&id);

        unsigned char scalars[64];
        std::memcpy(scalars, one_bytes, 32);
        std::memcpy(scalars + 32, one_bytes, 32);
        ran_jacobian points[2];
        ran_copy(&points[0], &id);
        ran_copy(&points[1], &G);
        ran_jacobian result;
        ran_msm_vartime(&result, scalars, points, 2);
        ran_tobytes(buf, &result);
        check_bytes("ran msm([1,1],[id,G]) == G", tv::compressed_points::ran_g, buf, 32);
    }

    /* Ran: MSM all identities */
    {
        ran_jacobian id;
        ran_identity(&id);
        unsigned char scalars[64];
        std::memcpy(scalars, one_bytes, 32);
        std::memcpy(scalars + 32, one_bytes, 32);
        ran_jacobian points[2];
        ran_copy(&points[0], &id);
        ran_copy(&points[1], &id);
        ran_jacobian result;
        ran_msm_vartime(&result, scalars, points, 2);
        check_nonzero("ran msm all identities == identity", ran_is_identity(&result));
    }

    /* Ran: MSM n=64 (deep Pippenger) */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        unsigned char scalars[64 * 32] = {};
        ran_jacobian points[64];
        for (int i = 0; i < 64; i++)
        {
            /* scalar_i = i+1 */
            scalars[i * 32] = (unsigned char)(i + 1);
            ran_copy(&points[i], &G);
        }
        ran_jacobian result;
        ran_msm_vartime(&result, scalars, points, 64);
        /* Expected: sum(1..64)*G = 2080*G */
        unsigned char s2080[32] = {0x20, 0x08}; /* 2080 = 0x0820 LE */
        ran_jacobian expected;
        ran_scalarmult_vartime(&expected, s2080, &G);
        unsigned char r_bytes[32], e_bytes[32];
        ran_tobytes(r_bytes, &result);
        ran_tobytes(e_bytes, &expected);
        check_bytes("ran msm n=64 == 2080*G", e_bytes, r_bytes, 32);
    }

    /* Ran: MSM duplicate scalars+points: msm([a,a],[G,G]) == 2a*G */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        unsigned char s5[32] = {};
        s5[0] = 0x05;
        unsigned char scalars[64];
        std::memcpy(scalars, s5, 32);
        std::memcpy(scalars + 32, s5, 32);
        ran_jacobian points[2];
        ran_copy(&points[0], &G);
        ran_copy(&points[1], &G);
        ran_jacobian result;
        ran_msm_vartime(&result, scalars, points, 2);
        unsigned char s10[32] = {};
        s10[0] = 0x0a;
        ran_jacobian expected;
        ran_scalarmult_vartime(&expected, s10, &G);
        unsigned char r_bytes[32], e_bytes[32];
        ran_tobytes(r_bytes, &result);
        ran_tobytes(e_bytes, &expected);
        check_bytes("ran msm([5,5],[G,G]) == 10*G", e_bytes, r_bytes, 32);
    }

    /* Shaw: MSM with identity */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_jacobian id;
        shaw_identity(&id);

        unsigned char scalars[64];
        std::memcpy(scalars, one_bytes, 32);
        std::memcpy(scalars + 32, one_bytes, 32);
        shaw_jacobian points[2];
        shaw_copy(&points[0], &id);
        shaw_copy(&points[1], &G);
        shaw_jacobian result;
        shaw_msm_vartime(&result, scalars, points, 2);
        shaw_tobytes(buf, &result);
        check_bytes("shaw msm([1,1],[id,G]) == G", tv::compressed_points::shaw_g, buf, 32);
    }

    /* Shaw: MSM all identities */
    {
        shaw_jacobian id;
        shaw_identity(&id);
        unsigned char scalars[64];
        std::memcpy(scalars, one_bytes, 32);
        std::memcpy(scalars + 32, one_bytes, 32);
        shaw_jacobian points[2];
        shaw_copy(&points[0], &id);
        shaw_copy(&points[1], &id);
        shaw_jacobian result;
        shaw_msm_vartime(&result, scalars, points, 2);
        check_nonzero("shaw msm all identities == identity", shaw_is_identity(&result));
    }

    /* Shaw: MSM n=64 */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        unsigned char scalars[64 * 32] = {};
        shaw_jacobian points[64];
        for (int i = 0; i < 64; i++)
        {
            scalars[i * 32] = (unsigned char)(i + 1);
            shaw_copy(&points[i], &G);
        }
        shaw_jacobian result;
        shaw_msm_vartime(&result, scalars, points, 64);
        unsigned char s2080[32] = {0x20, 0x08};
        shaw_jacobian expected;
        shaw_scalarmult_vartime(&expected, s2080, &G);
        unsigned char r_bytes[32], e_bytes[32];
        shaw_tobytes(r_bytes, &result);
        shaw_tobytes(e_bytes, &expected);
        check_bytes("shaw msm n=64 == 2080*G", e_bytes, r_bytes, 32);
    }
}


void test_msm_fixed()
{
    std::cout << std::endl << "=== Fixed-base MSM ===" << std::endl;

    /* Ran: msm_fixed(s1*G + s2*2G) == scalarmult(s1, G) + scalarmult(s2, 2G) */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_jacobian G2;
        ran_dbl(&G2, &G);

        ran_affine table_g[16], table_g2[16];
        ran_scalarmult_fixed_precompute(table_g, &G);
        ran_scalarmult_fixed_precompute(table_g2, &G2);

        const ran_affine *tables[2] = {table_g, table_g2};
        unsigned char scalars[64] = {};
        scalars[0] = 0x07; /* s1 = 7 */
        scalars[32] = 0x05; /* s2 = 5 */

        ran_jacobian msm_result;
        ran_msm_fixed(&msm_result, scalars, tables, 2);

        /* Expected: 7*G + 5*(2G) = 7*G + 10*G = 17*G */
        unsigned char s17[32] = {0x11};
        ran_jacobian expected;
        ran_scalarmult(&expected, s17, &G);

        unsigned char mr[32], ex[32];
        ran_tobytes(mr, &msm_result);
        ran_tobytes(ex, &expected);
        check_bytes("ran msm_fixed: 7*G + 5*(2G) == 17*G", ex, mr, 32);
    }

    /* Shaw: msm_fixed(s1*G + s2*2G) */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_jacobian G2;
        shaw_dbl(&G2, &G);

        shaw_affine table_g[16], table_g2[16];
        shaw_scalarmult_fixed_precompute(table_g, &G);
        shaw_scalarmult_fixed_precompute(table_g2, &G2);

        const shaw_affine *tables[2] = {table_g, table_g2};
        unsigned char scalars[64] = {};
        scalars[0] = 0x07;
        scalars[32] = 0x05;

        shaw_jacobian msm_result;
        shaw_msm_fixed(&msm_result, scalars, tables, 2);

        unsigned char s17[32] = {0x11};
        shaw_jacobian expected;
        shaw_scalarmult(&expected, s17, &G);

        unsigned char mr[32], ex[32];
        shaw_tobytes(mr, &msm_result);
        shaw_tobytes(ex, &expected);
        check_bytes("shaw msm_fixed: 7*G + 5*(2G) == 17*G", ex, mr, 32);
    }

    /* Ran: msm_fixed with 3 points and larger scalars */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_jacobian G2, G3;
        ran_dbl(&G2, &G);
        ran_add(&G3, &G2, &G);

        ran_affine t1[16], t2[16], t3[16];
        ran_scalarmult_fixed_precompute(t1, &G);
        ran_scalarmult_fixed_precompute(t2, &G2);
        ran_scalarmult_fixed_precompute(t3, &G3);

        const ran_affine *tables[3] = {t1, t2, t3};
        unsigned char scalars[96] = {};
        scalars[0] = 0x03; /* s1 = 3 */
        scalars[32] = 0x05; /* s2 = 5 */
        scalars[64] = 0x07; /* s3 = 7 */

        ran_jacobian msm_result;
        ran_msm_fixed(&msm_result, scalars, tables, 3);

        /* Expected: 3*G + 5*(2G) + 7*(3G) = 3+10+21 = 34*G */
        unsigned char s34[32] = {0x22};
        ran_jacobian expected;
        ran_scalarmult(&expected, s34, &G);

        unsigned char mr[32], ex[32];
        ran_tobytes(mr, &msm_result);
        ran_tobytes(ex, &expected);
        check_bytes("ran msm_fixed: 3*G + 5*(2G) + 7*(3G) == 34*G", ex, mr, 32);
    }

    /* Shaw: msm_fixed n=1 fallback */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_affine table[16];
        shaw_scalarmult_fixed_precompute(table, &G);

        const shaw_affine *tables[1] = {table};
        unsigned char scalars[32] = {0x0b}; /* 11 */

        shaw_jacobian msm_result, expected;
        shaw_msm_fixed(&msm_result, scalars, tables, 1);

        unsigned char s11[32] = {0x0b};
        shaw_scalarmult(&expected, s11, &G);

        unsigned char mr[32], ex[32];
        shaw_tobytes(mr, &msm_result);
        shaw_tobytes(ex, &expected);
        check_bytes("shaw msm_fixed: n=1 (11*G)", ex, mr, 32);
    }
}
