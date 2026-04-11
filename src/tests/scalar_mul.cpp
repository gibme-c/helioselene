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

void test_ran_scalarmult()
{
    std::cout << std::endl << "=== Ran scalar mul ===" << std::endl;
    unsigned char buf[32];

    ran_jacobian G;
    fp_copy(G.X, RAN_GX);
    fp_copy(G.Y, RAN_GY);
    fp_1(G.Z);

    ran_jacobian result;
    ran_scalarmult(&result, one_bytes, &G);
    ran_tobytes(buf, &result);
    check_bytes("1*G == G", tv::compressed_points::ran_g, buf, 32);

    ran_scalarmult(&result, zero_bytes, &G);
    check_nonzero("0*G == identity", ran_is_identity(&result));

    unsigned char two_scalar[32] = {0x02};
    ran_scalarmult(&result, two_scalar, &G);
    ran_tobytes(buf, &result);
    check_bytes("2*G == 2G", tv::compressed_points::ran_2g, buf, 32);

    unsigned char seven_scalar[32] = {0x07};
    ran_scalarmult(&result, seven_scalar, &G);
    ran_tobytes(buf, &result);
    check_bytes("7*G", tv::compressed_points::ran_7g, buf, 32);

    ran_scalarmult(&result, RAN_ORDER, &G);
    check_nonzero("order*G == identity", ran_is_identity(&result));

    ran_scalarmult_vartime(&result, one_bytes, &G);
    ran_tobytes(buf, &result);
    check_bytes("vartime: 1*G == G", tv::compressed_points::ran_g, buf, 32);

    ran_scalarmult_vartime(&result, seven_scalar, &G);
    ran_tobytes(buf, &result);
    check_bytes("vartime: 7*G", tv::compressed_points::ran_7g, buf, 32);

    ran_scalarmult_vartime(&result, RAN_ORDER, &G);
    check_nonzero("vartime: order*G == identity", ran_is_identity(&result));

    unsigned char scalar_a[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
                                  0xca, 0xef, 0xbe, 0xad, 0xde, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                  0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    ran_jacobian ct_result, vt_result;
    ran_scalarmult(&ct_result, scalar_a, &G);
    ran_scalarmult_vartime(&vt_result, scalar_a, &G);
    unsigned char ct_bytes[32], vt_bytes[32];
    ran_tobytes(ct_bytes, &ct_result);
    ran_tobytes(vt_bytes, &vt_result);
    check_bytes("CT == vartime for scalar_a", ct_bytes, vt_bytes, 32);

    unsigned char scalar_5[32] = {0x05};
    ran_jacobian aG, bG, sum_pt;
    ran_scalarmult(&aG, two_scalar, &G);
    ran_scalarmult(&bG, scalar_5, &G);
    ran_add(&sum_pt, &aG, &bG);
    ran_tobytes(buf, &sum_pt);
    check_bytes("(2+5)*G == 2*G + 5*G", tv::compressed_points::ran_7g, buf, 32);
}


void test_shaw_scalarmult()
{
    std::cout << std::endl << "=== Shaw scalar mul ===" << std::endl;
    unsigned char buf[32];

    shaw_jacobian G;
    fq_copy(G.X, SHAW_GX);
    fq_copy(G.Y, SHAW_GY);
    fq_1(G.Z);

    shaw_jacobian result;
    shaw_scalarmult(&result, one_bytes, &G);
    shaw_tobytes(buf, &result);
    check_bytes("1*G == G", tv::compressed_points::shaw_g, buf, 32);

    shaw_scalarmult(&result, zero_bytes, &G);
    check_nonzero("0*G == identity", shaw_is_identity(&result));

    unsigned char two_scalar[32] = {0x02};
    shaw_scalarmult(&result, two_scalar, &G);
    shaw_tobytes(buf, &result);
    check_bytes("2*G == 2G", tv::compressed_points::shaw_2g, buf, 32);

    unsigned char seven_scalar[32] = {0x07};
    shaw_scalarmult(&result, seven_scalar, &G);
    shaw_tobytes(buf, &result);
    check_bytes("7*G", tv::compressed_points::shaw_7g, buf, 32);

    shaw_scalarmult(&result, SHAW_ORDER, &G);
    check_nonzero("order*G == identity", shaw_is_identity(&result));

    shaw_scalarmult_vartime(&result, one_bytes, &G);
    shaw_tobytes(buf, &result);
    check_bytes("vartime: 1*G == G", tv::compressed_points::shaw_g, buf, 32);

    shaw_scalarmult_vartime(&result, seven_scalar, &G);
    shaw_tobytes(buf, &result);
    check_bytes("vartime: 7*G", tv::compressed_points::shaw_7g, buf, 32);

    shaw_scalarmult_vartime(&result, SHAW_ORDER, &G);
    check_nonzero("vartime: order*G == identity", shaw_is_identity(&result));

    unsigned char scalar_a[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
                                  0xca, 0xef, 0xbe, 0xad, 0xde, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                  0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    shaw_jacobian ct_result, vt_result;
    shaw_scalarmult(&ct_result, scalar_a, &G);
    shaw_scalarmult_vartime(&vt_result, scalar_a, &G);
    unsigned char ct_bytes[32], vt_bytes[32];
    shaw_tobytes(ct_bytes, &ct_result);
    shaw_tobytes(vt_bytes, &vt_result);
    check_bytes("CT == vartime for scalar_a", ct_bytes, vt_bytes, 32);

    unsigned char scalar_5[32] = {0x05};
    shaw_jacobian aG, bG, sum_pt;
    shaw_scalarmult(&aG, two_scalar, &G);
    shaw_scalarmult(&bG, scalar_5, &G);
    shaw_add(&sum_pt, &aG, &bG);
    shaw_tobytes(buf, &sum_pt);
    check_bytes("(2+5)*G == 2*G + 5*G", tv::compressed_points::shaw_7g, buf, 32);
}


void test_scalarmult_extended()
{
    std::cout << std::endl << "=== Scalar mul extended ===" << std::endl;

    /* Ran: associativity scalarmult(3, scalarmult(7, G)) == scalarmult(21, G) */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        unsigned char s3[32] = {0x03};
        unsigned char s7[32] = {0x07};
        unsigned char s21[32] = {0x15};
        ran_jacobian sevG, result, expected;
        ran_scalarmult(&sevG, s7, &G);
        ran_scalarmult(&result, s3, &sevG);
        ran_scalarmult(&expected, s21, &G);
        unsigned char r_bytes[32], e_bytes[32];
        ran_tobytes(r_bytes, &result);
        ran_tobytes(e_bytes, &expected);
        check_bytes("ran: 3*(7*G) == 21*G", e_bytes, r_bytes, 32);
    }

    /* Shaw: associativity */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        unsigned char s3[32] = {0x03};
        unsigned char s7[32] = {0x07};
        unsigned char s21[32] = {0x15};
        shaw_jacobian sevG, result, expected;
        shaw_scalarmult(&sevG, s7, &G);
        shaw_scalarmult(&result, s3, &sevG);
        shaw_scalarmult(&expected, s21, &G);
        unsigned char r_bytes[32], e_bytes[32];
        shaw_tobytes(r_bytes, &result);
        shaw_tobytes(e_bytes, &expected);
        check_bytes("shaw: 3*(7*G) == 21*G", e_bytes, r_bytes, 32);
    }

    /* Ran: scalarmult(scalar, identity) == identity (via tobytes) */
    {
        ran_jacobian id;
        ran_identity(&id);
        unsigned char s7[32] = {0x07};
        ran_jacobian result;
        ran_scalarmult(&result, s7, &id);
        unsigned char r_bytes[32];
        ran_tobytes(r_bytes, &result);
        check_bytes("ran: 7*identity == identity", zero_bytes, r_bytes, 32);
    }

    /* Shaw: scalarmult(scalar, identity) == identity (via tobytes) */
    {
        shaw_jacobian id;
        shaw_identity(&id);
        unsigned char s7[32] = {0x07};
        shaw_jacobian result;
        shaw_scalarmult(&result, s7, &id);
        unsigned char r_bytes[32];
        shaw_tobytes(r_bytes, &result);
        check_bytes("shaw: 7*identity == identity", zero_bytes, r_bytes, 32);
    }

    /* Ran: scalarmult_vartime(scalar, identity) == identity (via tobytes) */
    {
        ran_jacobian id;
        ran_identity(&id);
        unsigned char s7[32] = {0x07};
        ran_jacobian result;
        ran_scalarmult_vartime(&result, s7, &id);
        unsigned char r_bytes[32];
        ran_tobytes(r_bytes, &result);
        check_bytes("ran: vartime 7*identity == identity", zero_bytes, r_bytes, 32);
    }

    /* Shaw: scalarmult_vartime(scalar, identity) == identity (via tobytes) */
    {
        shaw_jacobian id;
        shaw_identity(&id);
        unsigned char s7[32] = {0x07};
        shaw_jacobian result;
        shaw_scalarmult_vartime(&result, s7, &id);
        unsigned char r_bytes[32];
        shaw_tobytes(r_bytes, &result);
        check_bytes("shaw: vartime 7*identity == identity", zero_bytes, r_bytes, 32);
    }
}


void test_fixed_base_scalarmult()
{
    std::cout << std::endl << "=== Fixed-base scalarmult (w=5) ===" << std::endl;

    /* Ran: fixed_scalarmult(7, G) == scalarmult(7, G) */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_affine table[16];
        ran_scalarmult_fixed_precompute(table, &G);

        unsigned char s7[32] = {0x07};
        ran_jacobian fixed_result, expected;
        ran_scalarmult_fixed(&fixed_result, s7, table);
        ran_scalarmult(&expected, s7, &G);

        unsigned char fr[32], ex[32];
        ran_tobytes(fr, &fixed_result);
        ran_tobytes(ex, &expected);
        check_bytes("ran fixed: 7*G", ex, fr, 32);
    }

    /* Shaw: fixed_scalarmult(7, G) == scalarmult(7, G) */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_affine table[16];
        shaw_scalarmult_fixed_precompute(table, &G);

        unsigned char s7[32] = {0x07};
        shaw_jacobian fixed_result, expected;
        shaw_scalarmult_fixed(&fixed_result, s7, table);
        shaw_scalarmult(&expected, s7, &G);

        unsigned char fr[32], ex[32];
        shaw_tobytes(fr, &fixed_result);
        shaw_tobytes(ex, &expected);
        check_bytes("shaw fixed: 7*G", ex, fr, 32);
    }

    /* Ran: fixed with large scalar (associativity) */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_affine table[16];
        ran_scalarmult_fixed_precompute(table, &G);

        unsigned char s21[32] = {0x15};
        ran_jacobian fixed_result, expected;
        ran_scalarmult_fixed(&fixed_result, s21, table);
        ran_scalarmult(&expected, s21, &G);

        unsigned char fr[32], ex[32];
        ran_tobytes(fr, &fixed_result);
        ran_tobytes(ex, &expected);
        check_bytes("ran fixed: 21*G", ex, fr, 32);
    }

    /* Shaw: fixed with large scalar */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_affine table[16];
        shaw_scalarmult_fixed_precompute(table, &G);

        unsigned char s21[32] = {0x15};
        shaw_jacobian fixed_result, expected;
        shaw_scalarmult_fixed(&fixed_result, s21, table);
        shaw_scalarmult(&expected, s21, &G);

        unsigned char fr[32], ex[32];
        shaw_tobytes(fr, &fixed_result);
        shaw_tobytes(ex, &expected);
        check_bytes("shaw fixed: 21*G", ex, fr, 32);
    }

    /* Ran: fixed with multi-byte scalar */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_affine table[16];
        ran_scalarmult_fixed_precompute(table, &G);

        unsigned char sc[32] = {0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89};
        ran_jacobian fixed_result, expected;
        ran_scalarmult_fixed(&fixed_result, sc, table);
        ran_scalarmult(&expected, sc, &G);

        unsigned char fr[32], ex[32];
        ran_tobytes(fr, &fixed_result);
        ran_tobytes(ex, &expected);
        check_bytes("ran fixed: large scalar", ex, fr, 32);
    }

    /* Shaw: fixed with multi-byte scalar */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_affine table[16];
        shaw_scalarmult_fixed_precompute(table, &G);

        unsigned char sc[32] = {0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89};
        shaw_jacobian fixed_result, expected;
        shaw_scalarmult_fixed(&fixed_result, sc, table);
        shaw_scalarmult(&expected, sc, &G);

        unsigned char fr[32], ex[32];
        shaw_tobytes(fr, &fixed_result);
        shaw_tobytes(ex, &expected);
        check_bytes("shaw fixed: large scalar", ex, fr, 32);
    }

    /* Ran: scalar = 1 (edge case) */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_affine table[16];
        ran_scalarmult_fixed_precompute(table, &G);

        unsigned char s1[32] = {0x01};
        ran_jacobian fixed_result;
        ran_scalarmult_fixed(&fixed_result, s1, table);

        unsigned char fr[32], gx[32];
        ran_tobytes(fr, &fixed_result);
        ran_tobytes(gx, &G);
        check_bytes("ran fixed: 1*G == G", gx, fr, 32);
    }

    /* Shaw: scalar = 1 (edge case) */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_affine table[16];
        shaw_scalarmult_fixed_precompute(table, &G);

        unsigned char s1[32] = {0x01};
        shaw_jacobian fixed_result;
        shaw_scalarmult_fixed(&fixed_result, s1, table);

        unsigned char fr[32], gx[32];
        shaw_tobytes(fr, &fixed_result);
        shaw_tobytes(gx, &G);
        check_bytes("shaw fixed: 1*G == G", gx, fr, 32);
    }
}


void test_precomputed_tables()
{
    std::cout << std::endl << "=== Precomputed generator tables ===" << std::endl;

    /* Ran: precomputed table matches runtime computation */
    {
        ran_affine precomp[16], runtime[16];
        ran_load_g_table(precomp);

        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);
        ran_scalarmult_fixed_precompute(runtime, &G);

        bool all_match = true;
        for (int i = 0; i < 16; i++)
        {
            unsigned char px[32], py[32], rx[32], ry[32];
            fp_tobytes(px, precomp[i].x);
            fp_tobytes(py, precomp[i].y);
            fp_tobytes(rx, runtime[i].x);
            fp_tobytes(ry, runtime[i].y);
            if (std::memcmp(px, rx, 32) != 0 || std::memcmp(py, ry, 32) != 0)
                all_match = false;
        }
        ++tests_run;
        if (all_match)
        {
            ++tests_passed;
            std::cout << "  PASS: ran precomp table matches runtime" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: ran precomp table mismatch" << std::endl;
        }
    }

    /* Shaw: precomputed table matches runtime computation */
    {
        shaw_affine precomp[16], runtime[16];
        shaw_load_g_table(precomp);

        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);
        shaw_scalarmult_fixed_precompute(runtime, &G);

        bool all_match = true;
        for (int i = 0; i < 16; i++)
        {
            unsigned char px[32], py[32], rx[32], ry[32];
            fq_tobytes(px, precomp[i].x);
            fq_tobytes(py, precomp[i].y);
            fq_tobytes(rx, runtime[i].x);
            fq_tobytes(ry, runtime[i].y);
            if (std::memcmp(px, rx, 32) != 0 || std::memcmp(py, ry, 32) != 0)
                all_match = false;
        }
        ++tests_run;
        if (all_match)
        {
            ++tests_passed;
            std::cout << "  PASS: shaw precomp table matches runtime" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: shaw precomp table mismatch" << std::endl;
        }
    }

    /* Ran: fixed scalarmult with precomp table matches regular scalarmult */
    {
        ran_affine table[16];
        ran_load_g_table(table);

        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        unsigned char sc[32] = {0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89};
        ran_jacobian fixed_result, expected;
        ran_scalarmult_fixed(&fixed_result, sc, table);
        ran_scalarmult(&expected, sc, &G);

        unsigned char fr[32], ex[32];
        ran_tobytes(fr, &fixed_result);
        ran_tobytes(ex, &expected);
        check_bytes("ran precomp scalarmult", ex, fr, 32);
    }

    /* Shaw: fixed scalarmult with precomp table matches regular scalarmult */
    {
        shaw_affine table[16];
        shaw_load_g_table(table);

        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        unsigned char sc[32] = {0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89};
        shaw_jacobian fixed_result, expected;
        shaw_scalarmult_fixed(&fixed_result, sc, table);
        shaw_scalarmult(&expected, sc, &G);

        unsigned char fr[32], ex[32];
        shaw_tobytes(fr, &fixed_result);
        shaw_tobytes(ex, &expected);
        check_bytes("shaw precomp scalarmult", ex, fr, 32);
    }
}


void test_point_to_scalar()
{
    std::cout << std::endl << "=== Point-to-scalar ===" << std::endl;

    /* Ran: extract x-coordinate of G as bytes */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        unsigned char xbytes[32];
        ran_point_to_bytes(xbytes, &G);

        check_bytes("ran G.x", tv::compressed_points::ran_gx, xbytes, 32);
    }

    /* Shaw: extract x-coordinate of G as bytes */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        unsigned char xbytes[32];
        shaw_point_to_bytes(xbytes, &G);

        check_bytes("shaw G.x", tv::compressed_points::shaw_gx, xbytes, 32);
    }

    /* Round-trip: 7*G, extract x, verify tobytes(affine.x) matches */
    {
        ran_jacobian G, P;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        unsigned char scalar_7[32] = {0x07};
        ran_scalarmult_vartime(&P, scalar_7, &G);

        unsigned char pt_bytes[32];
        ran_point_to_bytes(pt_bytes, &P);

        /* Verify via independent affine conversion */
        ran_affine a;
        ran_to_affine(&a, &P);
        unsigned char ref_bytes[32];
        fp_tobytes(ref_bytes, a.x);
        check_bytes("ran 7G round-trip x", ref_bytes, pt_bytes, 32);
    }

    /* Identity: should produce 32 zero bytes */
    {
        ran_jacobian id;
        ran_identity(&id);
        unsigned char xbytes[32];
        ran_point_to_bytes(xbytes, &id);
        check_bytes("ran identity -> zero bytes", zero_bytes, xbytes, 32);

        shaw_jacobian sid;
        shaw_identity(&sid);
        unsigned char sxbytes[32];
        shaw_point_to_bytes(sxbytes, &sid);
        check_bytes("shaw identity -> zero bytes", zero_bytes, sxbytes, 32);
    }

    /* Cross-curve: Ran point -> Fp bytes -> Shaw scalar -> Shaw point -> Fq bytes -> Ran scalar */
    {
        ran_jacobian HG, HP;
        fp_copy(HG.X, RAN_GX);
        fp_copy(HG.Y, RAN_GY);
        fp_1(HG.Z);

        unsigned char scalar_5[32] = {0x05};
        ran_scalarmult_vartime(&HP, scalar_5, &HG);

        /* Extract Ran x-coordinate as bytes (element of Fp = Shaw scalar field) */
        unsigned char hp_x[32];
        ran_point_to_bytes(hp_x, &HP);

        /* Use as Shaw scalar for scalarmult */
        shaw_jacobian SG, SP;
        fq_copy(SG.X, SHAW_GX);
        fq_copy(SG.Y, SHAW_GY);
        fq_1(SG.Z);
        shaw_scalarmult_vartime(&SP, hp_x, &SG);

        /* Extract Shaw x-coordinate as bytes (element of Fq = Ran scalar field) */
        unsigned char sp_x[32];
        shaw_point_to_bytes(sp_x, &SP);

        /* Use as Ran scalar */
        ran_jacobian HP2;
        ran_scalarmult_vartime(&HP2, sp_x, &HG);

        /* Verify the chain produced a valid non-identity point */
        unsigned char hp2_bytes[32];
        ran_point_to_bytes(hp2_bytes, &HP2);
        check_nonzero("cross-curve chain produces non-identity", std::memcmp(hp2_bytes, zero_bytes, 32) != 0 ? 1 : 0);
    }
}
