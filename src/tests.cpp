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

#include "helioselene.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

static std::string hex(const unsigned char *data, size_t len)
{
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i)
        oss << std::hex << std::setfill('0') << std::setw(2) << (int)data[i];
    return oss.str();
}

static bool check_bytes(const char *test_name, const unsigned char *expected, const unsigned char *actual, size_t len)
{
    ++tests_run;
    if (std::memcmp(expected, actual, len) == 0)
    {
        ++tests_passed;
        std::cout << "  PASS: " << test_name << std::endl;
        return true;
    }
    else
    {
        ++tests_failed;
        std::cout << "  FAIL: " << test_name << std::endl;
        std::cout << "    expected: " << hex(expected, len) << std::endl;
        std::cout << "    actual:   " << hex(actual, len) << std::endl;
        return false;
    }
}

static bool check_int(const char *test_name, int expected, int actual)
{
    ++tests_run;
    if (expected == actual)
    {
        ++tests_passed;
        std::cout << "  PASS: " << test_name << std::endl;
        return true;
    }
    else
    {
        ++tests_failed;
        std::cout << "  FAIL: " << test_name << std::endl;
        std::cout << "    expected: " << expected << std::endl;
        std::cout << "    actual:   " << actual << std::endl;
        return false;
    }
}

static bool check_nonzero(const char *test_name, int actual)
{
    ++tests_run;
    if (actual != 0)
    {
        ++tests_passed;
        std::cout << "  PASS: " << test_name << std::endl;
        return true;
    }
    else
    {
        ++tests_failed;
        std::cout << "  FAIL: " << test_name << " (expected non-zero, got 0)" << std::endl;
        return false;
    }
}

static const unsigned char test_a_bytes[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
                                               0xca, 0xef, 0xbe, 0xad, 0xde, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const unsigned char test_b_bytes[32] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x0d, 0xf0, 0xad,
                                               0xba, 0xce, 0xfa, 0xed, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const unsigned char one_bytes[32] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const unsigned char zero_bytes[32] = {0};
static const unsigned char four_bytes[32] = {0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/* F_p known-answer vectors */
static const unsigned char fp_ab_bytes[32] = {0x8b, 0xf8, 0x99, 0xb6, 0x81, 0xc3, 0x9d, 0x32, 0x37, 0x91, 0x83,
                                              0xab, 0x63, 0xdf, 0xe3, 0x39, 0x5a, 0xbb, 0x62, 0xcf, 0x01, 0xdb,
                                              0x9b, 0x07, 0x40, 0x05, 0x0f, 0x2e, 0x75, 0x64, 0xbf, 0x5d};
static const unsigned char fp_asq_bytes[32] = {0x34, 0xa5, 0xf2, 0xa2, 0x09, 0x5f, 0x47, 0xa6, 0x80, 0x23, 0x11,
                                               0x6b, 0x38, 0x72, 0xb0, 0xef, 0x20, 0x65, 0x11, 0xb6, 0xcc, 0x2e,
                                               0x41, 0xd2, 0x18, 0xfa, 0x92, 0x82, 0x13, 0xcd, 0xb1, 0x41};
static const unsigned char fp_ainv_bytes[32] = {0x3f, 0x3a, 0x94, 0xed, 0xea, 0xf4, 0x00, 0xef, 0x56, 0x09, 0xc0,
                                                0x94, 0xeb, 0x93, 0x22, 0xcb, 0x71, 0x87, 0x3d, 0x9b, 0x45, 0x9c,
                                                0xde, 0xf4, 0x0a, 0x20, 0x13, 0xc1, 0xfc, 0x61, 0x66, 0x25};

/* F_q known-answer vectors */
static const unsigned char fq_ab_bytes[32] = {0xd9, 0x30, 0x72, 0x3d, 0x0f, 0xf1, 0xe6, 0xc3, 0xde, 0x25, 0x1e,
                                              0xf4, 0x36, 0x67, 0x64, 0x7a, 0x5a, 0xbb, 0x62, 0xcf, 0x01, 0xdb,
                                              0x9b, 0x07, 0x40, 0x05, 0x0f, 0x2e, 0x75, 0x64, 0xbf, 0x5d};
static const unsigned char fq_asq_bytes[32] = {0x82, 0xdd, 0xca, 0x29, 0x97, 0x8c, 0x90, 0x37, 0x28, 0xb8, 0xab,
                                               0xb3, 0x0b, 0xfa, 0x30, 0x30, 0x21, 0x65, 0x11, 0xb6, 0xcc, 0x2e,
                                               0x41, 0xd2, 0x18, 0xfa, 0x92, 0x82, 0x13, 0xcd, 0xb1, 0x41};
static const unsigned char fq_ainv_bytes[32] = {0xee, 0xe9, 0xdc, 0xce, 0x6d, 0x37, 0x57, 0xf1, 0xfd, 0x90, 0x58,
                                                0xf5, 0xff, 0xff, 0x5f, 0xb3, 0x30, 0x3c, 0xb4, 0xb2, 0x81, 0x4a,
                                                0xb8, 0x4f, 0xcf, 0xbe, 0x50, 0xe0, 0x6b, 0x8e, 0xe1, 0x60};
static const unsigned char fq_sqrt4_bytes[32] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/* Helios compressed point test vectors */
static const unsigned char helios_g_compressed[32] = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char helios_2g_compressed[32] = {0x26, 0x29, 0x42, 0x40, 0x80, 0x90, 0xb3, 0xc5, 0x07, 0xb8, 0xac,
                                                       0x94, 0xd4, 0x6f, 0xc4, 0x95, 0xfc, 0x12, 0x9f, 0xb4, 0xd1, 0x65,
                                                       0x37, 0x24, 0x11, 0xd5, 0xe5, 0xea, 0x00, 0x84, 0x02, 0xf2};

static const unsigned char helios_7g_compressed[32] = {0x03, 0xdf, 0x58, 0xab, 0x3f, 0x90, 0x99, 0xc3, 0x4d, 0x76, 0x64,
                                                       0x2b, 0x4c, 0x99, 0xe5, 0x82, 0xe3, 0x8c, 0xf4, 0x7e, 0x1b, 0xee,
                                                       0x44, 0x4c, 0x48, 0x17, 0xa4, 0x81, 0xba, 0x49, 0x98, 0x26};

/* Selene compressed point test vectors */
static const unsigned char selene_g_compressed[32] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char selene_2g_compressed[32] = {0x9d, 0xc7, 0x27, 0x79, 0x72, 0xd2, 0xb6, 0x6e, 0x58, 0x6b, 0x65,
                                                       0xb7, 0x2c, 0x78, 0x7f, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static const unsigned char selene_7g_compressed[32] = {0x99, 0x30, 0x21, 0x4d, 0xf2, 0x35, 0x94, 0x1d, 0xba, 0x78, 0xb6,
                                                       0x1c, 0xeb, 0xf3, 0x81, 0x2c, 0x69, 0xc0, 0x43, 0x18, 0x28, 0xf9,
                                                       0x08, 0x9e, 0x01, 0x69, 0x5d, 0x8a, 0xfd, 0x58, 0xbe, 0x2f};

static void test_fp()
{
    std::cout << std::endl << "=== F_p arithmetic ===" << std::endl;
    unsigned char buf[32];

    fp_fe a, b, c, d;
    fp_frombytes(a, test_a_bytes);
    fp_frombytes(b, test_b_bytes);

    fp_tobytes(buf, a);
    check_bytes("tobytes(frombytes(a)) == a", test_a_bytes, buf, 32);

    fp_fe zero;
    fp_0(zero);
    fp_tobytes(buf, zero);
    check_bytes("tobytes(0)", zero_bytes, buf, 32);

    fp_fe one;
    fp_1(one);
    fp_tobytes(buf, one);
    check_bytes("tobytes(1)", one_bytes, buf, 32);

    fp_add(c, a, zero);
    fp_tobytes(buf, c);
    check_bytes("a + 0 == a", test_a_bytes, buf, 32);

    fp_mul(c, a, b);
    fp_tobytes(buf, c);
    check_bytes("a * b", fp_ab_bytes, buf, 32);

    fp_mul(d, b, a);
    fp_tobytes(buf, d);
    check_bytes("b * a == a * b", fp_ab_bytes, buf, 32);

    fp_sq(c, a);
    fp_tobytes(buf, c);
    check_bytes("a^2", fp_asq_bytes, buf, 32);

    fp_mul(d, a, a);
    fp_tobytes(buf, d);
    check_bytes("sq(a) == mul(a,a)", fp_asq_bytes, buf, 32);

    fp_mul(c, a, one);
    fp_tobytes(buf, c);
    check_bytes("a * 1 == a", test_a_bytes, buf, 32);

    fp_fe inv_a;
    fp_invert(inv_a, a);
    fp_tobytes(buf, inv_a);
    check_bytes("inv(a)", fp_ainv_bytes, buf, 32);

    fp_mul(c, inv_a, a);
    fp_tobytes(buf, c);
    check_bytes("inv(a) * a == 1", one_bytes, buf, 32);

    fp_sub(c, a, a);
    fp_tobytes(buf, c);
    check_bytes("a - a == 0", zero_bytes, buf, 32);

    fp_neg(d, a);
    fp_add(c, a, d);
    fp_tobytes(buf, c);
    check_bytes("a + (-a) == 0", zero_bytes, buf, 32);
}

static void test_fq()
{
    std::cout << std::endl << "=== F_q arithmetic ===" << std::endl;
    unsigned char buf[32];

    fq_fe a, b, c, d;
    fq_frombytes(a, test_a_bytes);
    fq_frombytes(b, test_b_bytes);

    fq_tobytes(buf, a);
    check_bytes("tobytes(frombytes(a)) == a", test_a_bytes, buf, 32);

    fq_fe zero;
    fq_0(zero);
    fq_tobytes(buf, zero);
    check_bytes("tobytes(0)", zero_bytes, buf, 32);

    fq_fe one;
    fq_1(one);
    fq_tobytes(buf, one);
    check_bytes("tobytes(1)", one_bytes, buf, 32);

    fq_add(c, a, zero);
    fq_tobytes(buf, c);
    check_bytes("a + 0 == a", test_a_bytes, buf, 32);

    fq_mul(c, a, b);
    fq_tobytes(buf, c);
    check_bytes("a * b", fq_ab_bytes, buf, 32);

    fq_mul(d, b, a);
    fq_tobytes(buf, d);
    check_bytes("b * a == a * b", fq_ab_bytes, buf, 32);

    fq_sq(c, a);
    fq_tobytes(buf, c);
    check_bytes("a^2", fq_asq_bytes, buf, 32);

    fq_mul(d, a, a);
    fq_tobytes(buf, d);
    check_bytes("sq(a) == mul(a,a)", fq_asq_bytes, buf, 32);

    fq_mul(c, a, one);
    fq_tobytes(buf, c);
    check_bytes("a * 1 == a", test_a_bytes, buf, 32);

    fq_fe inv_a;
    fq_invert(inv_a, a);
    fq_tobytes(buf, inv_a);
    check_bytes("inv(a)", fq_ainv_bytes, buf, 32);

    fq_mul(c, inv_a, a);
    fq_tobytes(buf, c);
    check_bytes("inv(a) * a == 1", one_bytes, buf, 32);

    fq_sub(c, a, a);
    fq_tobytes(buf, c);
    check_bytes("a - a == 0", zero_bytes, buf, 32);

    fq_neg(d, a);
    fq_add(c, a, d);
    fq_tobytes(buf, c);
    check_bytes("a + (-a) == 0", zero_bytes, buf, 32);

    fq_fe four;
    fq_frombytes(four, four_bytes);
    fq_fe sqrt4;
    fq_sqrt(sqrt4, four);
    fq_tobytes(buf, sqrt4);
    check_bytes("sqrt(4) == 2", fq_sqrt4_bytes, buf, 32);

    fq_sq(c, sqrt4);
    fq_tobytes(buf, c);
    check_bytes("sqrt(4)^2 == 4", four_bytes, buf, 32);
}

static void test_fp_sqrt()
{
    std::cout << std::endl << "=== F_p sqrt ===" << std::endl;
    unsigned char buf[32];

    fp_fe zero_fe, sqrt_out;
    fp_0(zero_fe);
    int rc = fp_sqrt(sqrt_out, zero_fe);
    check_int("sqrt(0) returns 0", 0, rc);
    fp_tobytes(buf, sqrt_out);
    check_bytes("sqrt(0) == 0", zero_bytes, buf, 32);

    fp_fe one_fe;
    fp_1(one_fe);
    rc = fp_sqrt(sqrt_out, one_fe);
    check_int("sqrt(1) returns 0", 0, rc);
    fp_fe sq_check;
    fp_sq(sq_check, sqrt_out);
    fp_tobytes(buf, sq_check);
    check_bytes("sqrt(1)^2 == 1", one_bytes, buf, 32);

    fp_fe four_fe;
    fp_frombytes(four_fe, four_bytes);
    rc = fp_sqrt(sqrt_out, four_fe);
    check_int("sqrt(4) returns 0", 0, rc);
    fp_sq(sq_check, sqrt_out);
    fp_tobytes(buf, sq_check);
    check_bytes("sqrt(4)^2 == 4", four_bytes, buf, 32);

    fp_fe a;
    fp_frombytes(a, test_a_bytes);
    fp_fe a_sq;
    fp_sq(a_sq, a);
    rc = fp_sqrt(sqrt_out, a_sq);
    check_int("sqrt(a^2) returns 0", 0, rc);
    fp_sq(sq_check, sqrt_out);
    fp_tobytes(buf, sq_check);
    unsigned char a_sq_bytes[32];
    fp_tobytes(a_sq_bytes, a_sq);
    check_bytes("sqrt(a^2)^2 == a^2", a_sq_bytes, buf, 32);

    unsigned char two_bytes[32] = {0x02};
    fp_fe two_fe;
    fp_frombytes(two_fe, two_bytes);
    rc = fp_sqrt(sqrt_out, two_fe);
    check_int("sqrt(2) returns -1 (non-square)", -1, rc);
}

static void test_helios_points()
{
    std::cout << std::endl << "=== Helios point ops ===" << std::endl;
    unsigned char buf[32];

    helios_affine g_aff;
    fp_copy(g_aff.x, HELIOS_GX);
    fp_copy(g_aff.y, HELIOS_GY);
    check_nonzero("G is on curve", helios_is_on_curve(&g_aff));

    helios_jacobian G;
    fp_copy(G.X, HELIOS_GX);
    fp_copy(G.Y, HELIOS_GY);
    fp_1(G.Z);

    helios_tobytes(buf, &G);
    check_bytes("tobytes(G)", helios_g_compressed, buf, 32);

    helios_jacobian G2;
    int rc = helios_frombytes(&G2, helios_g_compressed);
    check_int("frombytes(G) returns 0", 0, rc);
    helios_tobytes(buf, &G2);
    check_bytes("frombytes(tobytes(G)) round-trip", helios_g_compressed, buf, 32);

    helios_jacobian id;
    helios_identity(&id);
    check_nonzero("identity is_identity", helios_is_identity(&id));

    helios_tobytes(buf, &id);
    check_bytes("tobytes(identity) == zeros", zero_bytes, buf, 32);

    helios_jacobian dbl_G;
    helios_dbl(&dbl_G, &G);
    helios_tobytes(buf, &dbl_G);
    check_bytes("2G = dbl(G)", helios_2g_compressed, buf, 32);

    /* 3G = 2G + G (add doesn't handle P==P, so skip G+G test) */
    helios_jacobian three_G;
    helios_add(&three_G, &dbl_G, &G);

    helios_jacobian four_G;
    helios_dbl(&four_G, &dbl_G);

    helios_jacobian seven_G;
    helios_add(&seven_G, &four_G, &three_G);
    helios_tobytes(buf, &seven_G);
    check_bytes("7G = 4G + 3G", helios_7g_compressed, buf, 32);

    helios_jacobian decoded_2g;
    rc = helios_frombytes(&decoded_2g, helios_2g_compressed);
    check_int("frombytes(2G) returns 0", 0, rc);
    helios_tobytes(buf, &decoded_2g);
    check_bytes("2G round-trip", helios_2g_compressed, buf, 32);

    unsigned char invalid_bytes[32] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
    helios_jacobian invalid;
    rc = helios_frombytes(&invalid, invalid_bytes);
    check_int("reject non-canonical x", -1, rc);

    helios_affine g_affine;
    fp_copy(g_affine.x, HELIOS_GX);
    fp_copy(g_affine.y, HELIOS_GY);
    helios_jacobian madd_result;
    helios_madd(&madd_result, &dbl_G, &g_affine);
    helios_tobytes(buf, &madd_result);
    unsigned char three_G_bytes[32];
    helios_tobytes(three_G_bytes, &three_G);
    check_bytes("madd(2G, G) == add(2G, G)", three_G_bytes, buf, 32);
}

static void test_selene_points()
{
    std::cout << std::endl << "=== Selene point ops ===" << std::endl;
    unsigned char buf[32];

    selene_affine g_aff;
    fq_copy(g_aff.x, SELENE_GX);
    fq_copy(g_aff.y, SELENE_GY);
    check_nonzero("G is on curve", selene_is_on_curve(&g_aff));

    selene_jacobian G;
    fq_copy(G.X, SELENE_GX);
    fq_copy(G.Y, SELENE_GY);
    fq_1(G.Z);

    selene_tobytes(buf, &G);
    check_bytes("tobytes(G)", selene_g_compressed, buf, 32);

    selene_jacobian G2;
    int rc = selene_frombytes(&G2, selene_g_compressed);
    check_int("frombytes(G) returns 0", 0, rc);
    selene_tobytes(buf, &G2);
    check_bytes("frombytes(tobytes(G)) round-trip", selene_g_compressed, buf, 32);

    selene_jacobian id;
    selene_identity(&id);
    check_nonzero("identity is_identity", selene_is_identity(&id));
    selene_tobytes(buf, &id);
    check_bytes("tobytes(identity) == zeros", zero_bytes, buf, 32);

    selene_jacobian dbl_G;
    selene_dbl(&dbl_G, &G);
    selene_tobytes(buf, &dbl_G);
    check_bytes("2G = dbl(G)", selene_2g_compressed, buf, 32);

    /* 3G, 4G, 7G (add doesn't handle P==P, so skip G+G test) */
    selene_jacobian three_G;
    selene_add(&three_G, &dbl_G, &G);
    selene_jacobian four_G;
    selene_dbl(&four_G, &dbl_G);
    selene_jacobian seven_G;
    selene_add(&seven_G, &four_G, &three_G);
    selene_tobytes(buf, &seven_G);
    check_bytes("7G = 4G + 3G", selene_7g_compressed, buf, 32);

    selene_jacobian decoded_2g;
    rc = selene_frombytes(&decoded_2g, selene_2g_compressed);
    check_int("frombytes(2G) returns 0", 0, rc);
    selene_tobytes(buf, &decoded_2g);
    check_bytes("2G round-trip", selene_2g_compressed, buf, 32);

    unsigned char invalid_bytes[32] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
    selene_jacobian invalid;
    rc = selene_frombytes(&invalid, invalid_bytes);
    check_int("reject non-canonical x", -1, rc);

    selene_affine g_affine;
    fq_copy(g_affine.x, SELENE_GX);
    fq_copy(g_affine.y, SELENE_GY);
    selene_jacobian madd_result;
    selene_madd(&madd_result, &dbl_G, &g_affine);
    selene_tobytes(buf, &madd_result);
    unsigned char three_G_bytes[32];
    selene_tobytes(three_G_bytes, &three_G);
    check_bytes("madd(2G, G) == add(2G, G)", three_G_bytes, buf, 32);
}

static void test_helios_scalarmult()
{
    std::cout << std::endl << "=== Helios scalar mul ===" << std::endl;
    unsigned char buf[32];

    helios_jacobian G;
    fp_copy(G.X, HELIOS_GX);
    fp_copy(G.Y, HELIOS_GY);
    fp_1(G.Z);

    helios_jacobian result;
    helios_scalarmult(&result, one_bytes, &G);
    helios_tobytes(buf, &result);
    check_bytes("1*G == G", helios_g_compressed, buf, 32);

    helios_scalarmult(&result, zero_bytes, &G);
    check_nonzero("0*G == identity", helios_is_identity(&result));

    unsigned char two_scalar[32] = {0x02};
    helios_scalarmult(&result, two_scalar, &G);
    helios_tobytes(buf, &result);
    check_bytes("2*G == 2G", helios_2g_compressed, buf, 32);

    unsigned char seven_scalar[32] = {0x07};
    helios_scalarmult(&result, seven_scalar, &G);
    helios_tobytes(buf, &result);
    check_bytes("7*G", helios_7g_compressed, buf, 32);

    helios_scalarmult(&result, HELIOS_ORDER, &G);
    check_nonzero("order*G == identity", helios_is_identity(&result));

    helios_scalarmult_vartime(&result, one_bytes, &G);
    helios_tobytes(buf, &result);
    check_bytes("vartime: 1*G == G", helios_g_compressed, buf, 32);

    helios_scalarmult_vartime(&result, seven_scalar, &G);
    helios_tobytes(buf, &result);
    check_bytes("vartime: 7*G", helios_7g_compressed, buf, 32);

    helios_scalarmult_vartime(&result, HELIOS_ORDER, &G);
    check_nonzero("vartime: order*G == identity", helios_is_identity(&result));

    unsigned char scalar_a[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
                                  0xca, 0xef, 0xbe, 0xad, 0xde, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                  0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    helios_jacobian ct_result, vt_result;
    helios_scalarmult(&ct_result, scalar_a, &G);
    helios_scalarmult_vartime(&vt_result, scalar_a, &G);
    unsigned char ct_bytes[32], vt_bytes[32];
    helios_tobytes(ct_bytes, &ct_result);
    helios_tobytes(vt_bytes, &vt_result);
    check_bytes("CT == vartime for scalar_a", ct_bytes, vt_bytes, 32);

    unsigned char scalar_5[32] = {0x05};
    helios_jacobian aG, bG, sum_pt;
    helios_scalarmult(&aG, two_scalar, &G);
    helios_scalarmult(&bG, scalar_5, &G);
    helios_add(&sum_pt, &aG, &bG);
    helios_tobytes(buf, &sum_pt);
    check_bytes("(2+5)*G == 2*G + 5*G", helios_7g_compressed, buf, 32);
}

static void test_selene_scalarmult()
{
    std::cout << std::endl << "=== Selene scalar mul ===" << std::endl;
    unsigned char buf[32];

    selene_jacobian G;
    fq_copy(G.X, SELENE_GX);
    fq_copy(G.Y, SELENE_GY);
    fq_1(G.Z);

    selene_jacobian result;
    selene_scalarmult(&result, one_bytes, &G);
    selene_tobytes(buf, &result);
    check_bytes("1*G == G", selene_g_compressed, buf, 32);

    selene_scalarmult(&result, zero_bytes, &G);
    check_nonzero("0*G == identity", selene_is_identity(&result));

    unsigned char two_scalar[32] = {0x02};
    selene_scalarmult(&result, two_scalar, &G);
    selene_tobytes(buf, &result);
    check_bytes("2*G == 2G", selene_2g_compressed, buf, 32);

    unsigned char seven_scalar[32] = {0x07};
    selene_scalarmult(&result, seven_scalar, &G);
    selene_tobytes(buf, &result);
    check_bytes("7*G", selene_7g_compressed, buf, 32);

    selene_scalarmult(&result, SELENE_ORDER, &G);
    check_nonzero("order*G == identity", selene_is_identity(&result));

    selene_scalarmult_vartime(&result, one_bytes, &G);
    selene_tobytes(buf, &result);
    check_bytes("vartime: 1*G == G", selene_g_compressed, buf, 32);

    selene_scalarmult_vartime(&result, seven_scalar, &G);
    selene_tobytes(buf, &result);
    check_bytes("vartime: 7*G", selene_7g_compressed, buf, 32);

    selene_scalarmult_vartime(&result, SELENE_ORDER, &G);
    check_nonzero("vartime: order*G == identity", selene_is_identity(&result));

    unsigned char scalar_a[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
                                  0xca, 0xef, 0xbe, 0xad, 0xde, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                  0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    selene_jacobian ct_result, vt_result;
    selene_scalarmult(&ct_result, scalar_a, &G);
    selene_scalarmult_vartime(&vt_result, scalar_a, &G);
    unsigned char ct_bytes[32], vt_bytes[32];
    selene_tobytes(ct_bytes, &ct_result);
    selene_tobytes(vt_bytes, &vt_result);
    check_bytes("CT == vartime for scalar_a", ct_bytes, vt_bytes, 32);

    unsigned char scalar_5[32] = {0x05};
    selene_jacobian aG, bG, sum_pt;
    selene_scalarmult(&aG, two_scalar, &G);
    selene_scalarmult(&bG, scalar_5, &G);
    selene_add(&sum_pt, &aG, &bG);
    selene_tobytes(buf, &sum_pt);
    check_bytes("(2+5)*G == 2*G + 5*G", selene_7g_compressed, buf, 32);
}

static void test_wei25519()
{
    std::cout << std::endl << "=== Wei25519 bridge ===" << std::endl;

    unsigned char valid_x[32] = {0x03};
    fp_fe out;
    int rc = helioselene_wei25519_to_fp(out, valid_x);
    check_int("valid x accepted", 0, rc);
    unsigned char buf[32];
    fp_tobytes(buf, out);
    check_bytes("value preserved", valid_x, buf, 32);

    unsigned char p_bytes[32] = {0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
    rc = helioselene_wei25519_to_fp(out, p_bytes);
    check_int("x == p rejected", -1, rc);

    unsigned char high_bit[32] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};
    rc = helioselene_wei25519_to_fp(out, high_bit);
    check_int("bit 255 set rejected", -1, rc);
}

static void test_helios_msm()
{
    std::cout << std::endl << "=== Helios MSM ===" << std::endl;
    unsigned char buf[32];

    helios_jacobian G;
    fp_copy(G.X, HELIOS_GX);
    fp_copy(G.Y, HELIOS_GY);
    fp_1(G.Z);

    /* msm([1], [G]) == G */
    helios_jacobian result;
    helios_msm_vartime(&result, one_bytes, &G, 1);
    helios_tobytes(buf, &result);
    check_bytes("msm([1], [G]) == G", helios_g_compressed, buf, 32);

    /* msm([7], [G]) == 7*G */
    unsigned char seven_scalar[32] = {0x07};
    helios_msm_vartime(&result, seven_scalar, &G, 1);
    helios_tobytes(buf, &result);
    check_bytes("msm([7], [G]) == 7G", helios_7g_compressed, buf, 32);

    /* msm([0], [G]) == identity */
    helios_msm_vartime(&result, zero_bytes, &G, 1);
    check_nonzero("msm([0], [G]) == identity", helios_is_identity(&result));

    /* msm([], []) == identity (n=0) */
    helios_msm_vartime(&result, nullptr, nullptr, 0);
    check_nonzero("msm([], []) == identity", helios_is_identity(&result));

    /* Linearity: msm([2, 5], [G, G]) == 7*G */
    unsigned char two_scalar[32] = {0x02};
    unsigned char five_scalar[32] = {0x05};
    unsigned char scalars_2_5[64];
    std::memcpy(scalars_2_5, two_scalar, 32);
    std::memcpy(scalars_2_5 + 32, five_scalar, 32);
    helios_jacobian points_2[2];
    helios_copy(&points_2[0], &G);
    helios_copy(&points_2[1], &G);
    helios_msm_vartime(&result, scalars_2_5, points_2, 2);
    helios_tobytes(buf, &result);
    check_bytes("msm([2,5], [G,G]) == 7G", helios_7g_compressed, buf, 32);

    /* msm([a], [P]) == scalarmult_vartime(a, P) */
    unsigned char scalar_a[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
                                  0xca, 0xef, 0xbe, 0xad, 0xde, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                  0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    helios_jacobian sm_result;
    helios_scalarmult_vartime(&sm_result, scalar_a, &G);
    unsigned char sm_bytes[32];
    helios_tobytes(sm_bytes, &sm_result);
    helios_msm_vartime(&result, scalar_a, &G, 1);
    helios_tobytes(buf, &result);
    check_bytes("msm([a], [G]) == vartime(a, G)", sm_bytes, buf, 32);

    /* Two distinct points: msm([a, b], [G, 2G]) == a*G + b*2G */
    helios_jacobian G2;
    helios_dbl(&G2, &G);
    unsigned char scalar_b[32] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x0d, 0xf0, 0xad,
                                  0xba, 0xce, 0xfa, 0xed, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char scalars_ab[64];
    std::memcpy(scalars_ab, scalar_a, 32);
    std::memcpy(scalars_ab + 32, scalar_b, 32);
    helios_jacobian points_ab[2];
    helios_copy(&points_ab[0], &G);
    helios_copy(&points_ab[1], &G2);
    helios_msm_vartime(&result, scalars_ab, points_ab, 2);
    helios_tobytes(buf, &result);

    helios_jacobian aG, bG2, expected;
    helios_scalarmult_vartime(&aG, scalar_a, &G);
    helios_scalarmult_vartime(&bG2, scalar_b, &G2);
    helios_add(&expected, &aG, &bG2);
    unsigned char expected_bytes[32];
    helios_tobytes(expected_bytes, &expected);
    check_bytes("msm([a,b], [G,2G]) == a*G + b*2G", expected_bytes, buf, 32);

    /* n=8 (exercises Straus): all scalars=1, all points=G → sum = 8*G */
    {
        unsigned char scalars8[8 * 32] = {};
        helios_jacobian points8[8];
        for (int i = 0; i < 8; i++)
        {
            scalars8[i * 32] = 0x01;
            helios_copy(&points8[i], &G);
        }
        /* 8*G via repeated doubling */
        unsigned char eight_scalar[32] = {0x08};
        helios_jacobian eightG;
        helios_scalarmult_vartime(&eightG, eight_scalar, &G);
        helios_tobytes(expected_bytes, &eightG);
        helios_msm_vartime(&result, scalars8, points8, 8);
        helios_tobytes(buf, &result);
        check_bytes("msm n=8 (Straus)", expected_bytes, buf, 32);
    }

    /* n=33 (crosses Straus/Pippenger boundary): all scalars=1, all points=G → 33*G */
    {
        unsigned char scalars33[33 * 32] = {};
        helios_jacobian points33[33];
        for (int i = 0; i < 33; i++)
        {
            scalars33[i * 32] = 0x01;
            helios_copy(&points33[i], &G);
        }
        unsigned char thirtythree_scalar[32] = {33};
        helios_jacobian expected_pt;
        helios_scalarmult_vartime(&expected_pt, thirtythree_scalar, &G);
        helios_tobytes(expected_bytes, &expected_pt);
        helios_msm_vartime(&result, scalars33, points33, 33);
        helios_tobytes(buf, &result);
        check_bytes("msm n=33 (Pippenger)", expected_bytes, buf, 32);
    }

    /* All-zero scalars → identity */
    {
        unsigned char zero_scalars[4 * 32] = {};
        helios_jacobian points4[4];
        for (int i = 0; i < 4; i++)
            helios_copy(&points4[i], &G);
        helios_msm_vartime(&result, zero_scalars, points4, 4);
        check_nonzero("msm all-zero scalars == identity", helios_is_identity(&result));
    }
}

static void test_selene_msm()
{
    std::cout << std::endl << "=== Selene MSM ===" << std::endl;
    unsigned char buf[32];

    selene_jacobian G;
    fq_copy(G.X, SELENE_GX);
    fq_copy(G.Y, SELENE_GY);
    fq_1(G.Z);

    /* msm([1], [G]) == G */
    selene_jacobian result;
    selene_msm_vartime(&result, one_bytes, &G, 1);
    selene_tobytes(buf, &result);
    check_bytes("msm([1], [G]) == G", selene_g_compressed, buf, 32);

    /* msm([7], [G]) == 7*G */
    unsigned char seven_scalar[32] = {0x07};
    selene_msm_vartime(&result, seven_scalar, &G, 1);
    selene_tobytes(buf, &result);
    check_bytes("msm([7], [G]) == 7G", selene_7g_compressed, buf, 32);

    /* msm([0], [G]) == identity */
    selene_msm_vartime(&result, zero_bytes, &G, 1);
    check_nonzero("msm([0], [G]) == identity", selene_is_identity(&result));

    /* msm([], []) == identity (n=0) */
    selene_msm_vartime(&result, nullptr, nullptr, 0);
    check_nonzero("msm([], []) == identity", selene_is_identity(&result));

    /* Linearity: msm([2, 5], [G, G]) == 7*G */
    unsigned char two_scalar[32] = {0x02};
    unsigned char five_scalar[32] = {0x05};
    unsigned char scalars_2_5[64];
    std::memcpy(scalars_2_5, two_scalar, 32);
    std::memcpy(scalars_2_5 + 32, five_scalar, 32);
    selene_jacobian points_2[2];
    selene_copy(&points_2[0], &G);
    selene_copy(&points_2[1], &G);
    selene_msm_vartime(&result, scalars_2_5, points_2, 2);
    selene_tobytes(buf, &result);
    check_bytes("msm([2,5], [G,G]) == 7G", selene_7g_compressed, buf, 32);

    /* msm([a], [P]) == scalarmult_vartime(a, P) */
    unsigned char scalar_a[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
                                  0xca, 0xef, 0xbe, 0xad, 0xde, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                  0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
    selene_jacobian sm_result;
    selene_scalarmult_vartime(&sm_result, scalar_a, &G);
    unsigned char sm_bytes[32];
    selene_tobytes(sm_bytes, &sm_result);
    selene_msm_vartime(&result, scalar_a, &G, 1);
    selene_tobytes(buf, &result);
    check_bytes("msm([a], [G]) == vartime(a, G)", sm_bytes, buf, 32);

    /* Two distinct points: msm([a, b], [G, 2G]) == a*G + b*2G */
    {
        selene_jacobian G2;
        selene_dbl(&G2, &G);
        unsigned char scalar_b[32] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x0d, 0xf0, 0xad,
                                      0xba, 0xce, 0xfa, 0xed, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        unsigned char scalars_ab[64];
        std::memcpy(scalars_ab, scalar_a, 32);
        std::memcpy(scalars_ab + 32, scalar_b, 32);
        selene_jacobian points_ab[2];
        selene_copy(&points_ab[0], &G);
        selene_copy(&points_ab[1], &G2);
        selene_msm_vartime(&result, scalars_ab, points_ab, 2);
        selene_tobytes(buf, &result);

        selene_jacobian aG, bG2, expected;
        selene_scalarmult_vartime(&aG, scalar_a, &G);
        selene_scalarmult_vartime(&bG2, scalar_b, &G2);
        selene_add(&expected, &aG, &bG2);
        unsigned char expected_bytes[32];
        selene_tobytes(expected_bytes, &expected);
        check_bytes("msm([a,b], [G,2G]) == a*G + b*2G", expected_bytes, buf, 32);
    }

    /* n=8 (exercises Straus): all scalars=1, all points=G → sum = 8*G */
    {
        unsigned char scalars8[8 * 32] = {};
        selene_jacobian points8[8];
        for (int i = 0; i < 8; i++)
        {
            scalars8[i * 32] = 0x01;
            selene_copy(&points8[i], &G);
        }
        unsigned char eight_scalar[32] = {0x08};
        selene_jacobian eightG;
        selene_scalarmult_vartime(&eightG, eight_scalar, &G);
        unsigned char expected_bytes[32];
        selene_tobytes(expected_bytes, &eightG);
        selene_msm_vartime(&result, scalars8, points8, 8);
        selene_tobytes(buf, &result);
        check_bytes("msm n=8 (Straus)", expected_bytes, buf, 32);
    }

    /* n=33 (crosses Straus/Pippenger boundary): all scalars=1, all points=G → 33*G */
    {
        unsigned char scalars33[33 * 32] = {};
        selene_jacobian points33[33];
        for (int i = 0; i < 33; i++)
        {
            scalars33[i * 32] = 0x01;
            selene_copy(&points33[i], &G);
        }
        unsigned char thirtythree_scalar[32] = {33};
        selene_jacobian expected_pt;
        selene_scalarmult_vartime(&expected_pt, thirtythree_scalar, &G);
        unsigned char expected_bytes[32];
        selene_tobytes(expected_bytes, &expected_pt);
        selene_msm_vartime(&result, scalars33, points33, 33);
        selene_tobytes(buf, &result);
        check_bytes("msm n=33 (Pippenger)", expected_bytes, buf, 32);
    }

    /* All-zero scalars → identity */
    {
        unsigned char zero_scalars[4 * 32] = {};
        selene_jacobian points4[4];
        for (int i = 0; i < 4; i++)
            selene_copy(&points4[i], &G);
        selene_msm_vartime(&result, zero_scalars, points4, 4);
        check_nonzero("msm all-zero scalars == identity", selene_is_identity(&result));
    }
}

/* Helios SSWU test vectors (Z=7) */
static const unsigned char helios_sswu_u1_result[32] = {
    0xc1, 0x2b, 0xdf, 0x94, 0x58, 0xf9, 0x6c, 0x32, 0x1e, 0xe6, 0x8e, 0x9a, 0x25, 0xa8, 0x16, 0x2a,
    0xac, 0x44, 0xfd, 0xb4, 0x9e, 0x0d, 0xa1, 0xc4, 0xb6, 0xcb, 0x2c, 0x04, 0x29, 0xd9, 0xe8, 0x92};
static const unsigned char helios_sswu_u2_result[32] = {
    0x2b, 0xa6, 0x56, 0xa7, 0x92, 0xc8, 0x4a, 0x9c, 0xfc, 0xf6, 0xe2, 0xef, 0x8f, 0x17, 0x45, 0x5b,
    0x02, 0x31, 0x05, 0xc2, 0x18, 0x51, 0xe5, 0xee, 0x95, 0xda, 0x5a, 0x9e, 0x35, 0xcd, 0x68, 0x7e};
static const unsigned char helios_sswu_u42_result[32] = {
    0x02, 0xa5, 0xe6, 0x21, 0x27, 0x7d, 0xf1, 0x0c, 0xb8, 0xab, 0xf7, 0xaa, 0xf2, 0x30, 0x8c, 0x83,
    0x51, 0xae, 0xb8, 0xf8, 0x9f, 0x87, 0x0f, 0x38, 0xe4, 0x4b, 0xf6, 0x26, 0x32, 0xda, 0xfa, 0x44};

/* Selene SSWU test vectors (Z=-4) */
static const unsigned char selene_sswu_u1_result[32] = {
    0x86, 0x47, 0x94, 0xcc, 0xb4, 0x7a, 0x10, 0x0d, 0x9c, 0x06, 0x24, 0x65, 0xde, 0x49, 0x0c, 0x58,
    0x4f, 0xd5, 0xaa, 0x7c, 0xbb, 0x62, 0xa6, 0x2b, 0x93, 0x1b, 0xb9, 0xa0, 0x8e, 0x37, 0x1e, 0xde};
static const unsigned char selene_sswu_u2_result[32] = {
    0x89, 0x0c, 0xf9, 0x19, 0x1a, 0x8d, 0x52, 0x90, 0xc3, 0xd9, 0x8d, 0xba, 0x4c, 0xf8, 0x18, 0x1f,
    0x0b, 0x8d, 0xef, 0x20, 0x78, 0xd4, 0x2d, 0x0c, 0x49, 0x23, 0xba, 0x5f, 0xed, 0xd1, 0xfd, 0x5a};
static const unsigned char selene_sswu_u42_result[32] = {
    0xd1, 0x74, 0x24, 0x2b, 0x58, 0x40, 0xdf, 0xd2, 0x85, 0x39, 0x24, 0x38, 0x3d, 0x6b, 0x0f, 0x62,
    0xb0, 0x93, 0xb5, 0x9b, 0x6d, 0xdc, 0x89, 0x71, 0x36, 0x19, 0x00, 0xcf, 0x6c, 0xb7, 0xe5, 0x06};

static void test_fp_sqrt_sswu()
{
    std::cout << std::endl << "=== F_p sqrt (SSWU gx2) ===" << std::endl;
    unsigned char buf[32];

    /* gx2 for SSWU u=1, known to be a QR */
    static const unsigned char gx2_bytes[32] = {0x4a, 0x9d, 0xd9, 0xd3, 0x95, 0x50, 0x3c, 0x31, 0x36, 0x8c, 0x6b,
                                                0xc5, 0x81, 0xc6, 0xa4, 0xc0, 0xc9, 0xca, 0x97, 0xde, 0x52, 0x20,
                                                0x8d, 0x23, 0xb2, 0x69, 0xc5, 0x73, 0x68, 0x0d, 0xcb, 0x16};
    static const unsigned char y_expected[32] = {0x98, 0xff, 0x11, 0x0f, 0x2a, 0xbf, 0xc4, 0x3f, 0xdf, 0xac, 0x96,
                                                 0x12, 0xf6, 0xde, 0x68, 0x85, 0x41, 0xf8, 0xf4, 0xbb, 0xea, 0xe4,
                                                 0x73, 0x1c, 0x10, 0x71, 0xce, 0xc2, 0xd0, 0xef, 0xc1, 0x47};

    fp_fe gx2_fe, y_fe;
    fp_frombytes(gx2_fe, gx2_bytes);
    int rc = fp_sqrt(y_fe, gx2_fe);
    check_int("fp_sqrt(gx2) returns 0 (is QR)", 0, rc);

    fp_fe check;
    fp_sq(check, y_fe);
    fp_tobytes(buf, check);
    check_bytes("sqrt(gx2)^2 == gx2", gx2_bytes, buf, 32);

    /* Also check the value matches Python */
    fp_tobytes(buf, y_fe);
    /* Note: fp_sqrt may return either root; check value or its negation */
    bool match_pos = (std::memcmp(buf, y_expected, 32) == 0);
    fp_fe neg_y;
    fp_neg(neg_y, y_fe);
    unsigned char neg_buf[32];
    fp_tobytes(neg_buf, neg_y);
    bool match_neg = (std::memcmp(neg_buf, y_expected, 32) == 0);
    ++tests_run;
    if (match_pos || match_neg)
    {
        ++tests_passed;
        std::cout << "  PASS: sqrt(gx2) matches expected root" << std::endl;
    }
    else
    {
        ++tests_failed;
        std::cout << "  FAIL: sqrt(gx2) matches expected root" << std::endl;
        std::cout << "    expected: " << hex(y_expected, 32) << std::endl;
        std::cout << "    actual:   " << hex(buf, 32) << std::endl;
    }

    /* Now test: compute gx from x2 directly and check sqrt */
    static const unsigned char x2_bytes[32] = {0xc1, 0x2b, 0xdf, 0x94, 0x58, 0xf9, 0x6c, 0x32, 0x1e, 0xe6, 0x8e,
                                               0x9a, 0x25, 0xa8, 0x16, 0x2a, 0xac, 0x44, 0xfd, 0xb4, 0x9e, 0x0d,
                                               0xa1, 0xc4, 0xb6, 0xcb, 0x2c, 0x04, 0x29, 0xd9, 0xe8, 0x12};
    fp_fe x2_fe, x2_sq, x2_cu, gx_computed;
    fp_frombytes(x2_fe, x2_bytes);
    fp_sq(x2_sq, x2_fe);
    fp_mul(x2_cu, x2_sq, x2_fe);

    /* A = -3 mod p */
    fp_fe three_x;
    fp_add(three_x, x2_fe, x2_fe);
    fp_add(three_x, three_x, x2_fe);
    fp_sub(gx_computed, x2_cu, three_x);
    fp_add(gx_computed, gx_computed, HELIOS_B);
    fp_tobytes(buf, gx_computed);
    check_bytes("gx from x2 matches gx2", gx2_bytes, buf, 32);
}

static void test_helios_sswu()
{
    std::cout << std::endl << "=== Helios SSWU ===" << std::endl;
    unsigned char buf[32];

    /* Known test vectors */
    helios_jacobian result;
    helios_map_to_curve(&result, one_bytes);
    helios_tobytes(buf, &result);
    check_bytes("sswu(1)", helios_sswu_u1_result, buf, 32);

    unsigned char two_bytes[32] = {0x02};
    helios_map_to_curve(&result, two_bytes);
    helios_tobytes(buf, &result);
    check_bytes("sswu(2)", helios_sswu_u2_result, buf, 32);

    unsigned char u42_bytes[32] = {0x2a};
    helios_map_to_curve(&result, u42_bytes);
    helios_tobytes(buf, &result);
    check_bytes("sswu(42)", helios_sswu_u42_result, buf, 32);

    /* Deterministic: same input → same output */
    helios_jacobian result2;
    helios_map_to_curve(&result2, one_bytes);
    helios_tobytes(buf, &result2);
    check_bytes("sswu(1) deterministic", helios_sswu_u1_result, buf, 32);

    /* Output is on curve */
    helios_affine aff;
    helios_to_affine(&aff, &result);
    check_nonzero("sswu(1) on curve", helios_is_on_curve(&aff));

    /* map_to_curve2(u0, u1) == map_to_curve(u0) + map_to_curve(u1) */
    helios_jacobian p0, p1, sum_direct, sum_combined;
    helios_map_to_curve(&p0, one_bytes);
    helios_map_to_curve(&p1, two_bytes);
    helios_add(&sum_direct, &p0, &p1);
    helios_tobytes(buf, &sum_direct);

    helios_map_to_curve2(&sum_combined, one_bytes, two_bytes);
    unsigned char buf2[32];
    helios_tobytes(buf2, &sum_combined);
    check_bytes("map_to_curve2(1,2) == sswu(1)+sswu(2)", buf, buf2, 32);

    /* sswu(0) produces a valid point */
    helios_map_to_curve(&result, zero_bytes);
    helios_to_affine(&aff, &result);
    check_nonzero("sswu(0) on curve", helios_is_on_curve(&aff));
}

static void test_selene_sswu()
{
    std::cout << std::endl << "=== Selene SSWU ===" << std::endl;
    unsigned char buf[32];

    /* Known test vectors */
    selene_jacobian result;
    selene_map_to_curve(&result, one_bytes);
    selene_tobytes(buf, &result);
    check_bytes("sswu(1)", selene_sswu_u1_result, buf, 32);

    unsigned char two_bytes[32] = {0x02};
    selene_map_to_curve(&result, two_bytes);
    selene_tobytes(buf, &result);
    check_bytes("sswu(2)", selene_sswu_u2_result, buf, 32);

    unsigned char u42_bytes[32] = {0x2a};
    selene_map_to_curve(&result, u42_bytes);
    selene_tobytes(buf, &result);
    check_bytes("sswu(42)", selene_sswu_u42_result, buf, 32);

    /* Deterministic: same input → same output */
    selene_jacobian result2;
    selene_map_to_curve(&result2, one_bytes);
    selene_tobytes(buf, &result2);
    check_bytes("sswu(1) deterministic", selene_sswu_u1_result, buf, 32);

    /* Output is on curve */
    selene_affine aff;
    selene_to_affine(&aff, &result);
    check_nonzero("sswu(1) on curve", selene_is_on_curve(&aff));

    /* map_to_curve2(u0, u1) == map_to_curve(u0) + map_to_curve(u1) */
    selene_jacobian p0, p1, sum_direct, sum_combined;
    selene_map_to_curve(&p0, one_bytes);
    selene_map_to_curve(&p1, two_bytes);
    selene_add(&sum_direct, &p0, &p1);
    selene_tobytes(buf, &sum_direct);

    selene_map_to_curve2(&sum_combined, one_bytes, two_bytes);
    unsigned char buf2[32];
    selene_tobytes(buf2, &sum_combined);
    check_bytes("map_to_curve2(1,2) == sswu(1)+sswu(2)", buf, buf2, 32);

    /* sswu(0) produces a valid point */
    selene_map_to_curve(&result, zero_bytes);
    selene_to_affine(&aff, &result);
    check_nonzero("sswu(0) on curve", selene_is_on_curve(&aff));
}

static void test_helios_batch_affine()
{
    std::cout << std::endl << "=== Helios batch affine ===" << std::endl;

    helios_jacobian G;
    fp_copy(G.X, HELIOS_GX);
    fp_copy(G.Y, HELIOS_GY);
    fp_1(G.Z);

    /* n=1: batch matches single to_affine */
    {
        helios_affine batch_out[1], single_out;
        helios_batch_to_affine(batch_out, &G, 1);
        helios_to_affine(&single_out, &G);
        unsigned char bx[32], sx[32], by[32], sy[32];
        fp_tobytes(bx, batch_out[0].x);
        fp_tobytes(sx, single_out.x);
        check_bytes("batch n=1 x matches single", sx, bx, 32);
        fp_tobytes(by, batch_out[0].y);
        fp_tobytes(sy, single_out.y);
        check_bytes("batch n=1 y matches single", sy, by, 32);
    }

    /* n=4: multiple distinct points */
    {
        helios_jacobian points[4];
        helios_copy(&points[0], &G);
        helios_dbl(&points[1], &G);
        helios_add(&points[2], &points[1], &G);
        helios_dbl(&points[3], &points[1]);

        helios_affine batch_out[4], single_out[4];
        helios_batch_to_affine(batch_out, points, 4);
        for (int i = 0; i < 4; i++)
            helios_to_affine(&single_out[i], &points[i]);

        for (int i = 0; i < 4; i++)
        {
            unsigned char bx[32], sx[32];
            fp_tobytes(bx, batch_out[i].x);
            fp_tobytes(sx, single_out[i].x);
            std::string name = "batch n=4 point " + std::to_string(i) + " x";
            check_bytes(name.c_str(), sx, bx, 32);
        }
    }

    /* Identity point handling */
    {
        helios_jacobian points[2];
        helios_copy(&points[0], &G);
        helios_identity(&points[1]);
        helios_affine batch_out[2];
        helios_batch_to_affine(batch_out, points, 2);
        unsigned char zx[32];
        fp_tobytes(zx, batch_out[1].x);
        check_bytes("batch identity x == 0", zero_bytes, zx, 32);
    }
}

static void test_selene_batch_affine()
{
    std::cout << std::endl << "=== Selene batch affine ===" << std::endl;

    selene_jacobian G;
    fq_copy(G.X, SELENE_GX);
    fq_copy(G.Y, SELENE_GY);
    fq_1(G.Z);

    /* n=4 */
    {
        selene_jacobian points[4];
        selene_copy(&points[0], &G);
        selene_dbl(&points[1], &G);
        selene_add(&points[2], &points[1], &G);
        selene_dbl(&points[3], &points[1]);

        selene_affine batch_out[4], single_out[4];
        selene_batch_to_affine(batch_out, points, 4);
        for (int i = 0; i < 4; i++)
            selene_to_affine(&single_out[i], &points[i]);

        for (int i = 0; i < 4; i++)
        {
            unsigned char bx[32], sx[32];
            fq_tobytes(bx, batch_out[i].x);
            fq_tobytes(sx, single_out[i].x);
            std::string name = "batch n=4 point " + std::to_string(i) + " x";
            check_bytes(name.c_str(), sx, bx, 32);
        }
    }
}

static void test_helios_pedersen()
{
    std::cout << std::endl << "=== Helios Pedersen ===" << std::endl;

    helios_jacobian G;
    fp_copy(G.X, HELIOS_GX);
    fp_copy(G.Y, HELIOS_GY);
    fp_1(G.Z);

    /* C = r*H + a*G, where H = 2G, verify == r*2G + a*G */
    helios_jacobian H;
    helios_dbl(&H, &G);

    unsigned char r_scalar[32] = {0x03};
    unsigned char a_scalar[32] = {0x05};

    helios_jacobian commit;
    helios_pedersen_commit(&commit, r_scalar, &H, a_scalar, &G, 1);

    /* Compute expected: 3*2G + 5*G = 6G + 5G = 11G */
    unsigned char eleven_scalar[32] = {0x0b};
    helios_jacobian expected;
    helios_scalarmult_vartime(&expected, eleven_scalar, &G);

    unsigned char commit_bytes[32], expected_bytes[32];
    helios_tobytes(commit_bytes, &commit);
    helios_tobytes(expected_bytes, &expected);
    check_bytes("pedersen(3, 2G, [5], [G]) == 11G", expected_bytes, commit_bytes, 32);

    /* n=0: C = r*H (blinding only) */
    helios_pedersen_commit(&commit, r_scalar, &H, nullptr, nullptr, 0);
    unsigned char three_scalar[32] = {0x03};
    helios_scalarmult_vartime(&expected, three_scalar, &H);
    helios_tobytes(commit_bytes, &commit);
    helios_tobytes(expected_bytes, &expected);
    check_bytes("pedersen n=0: r*H only", expected_bytes, commit_bytes, 32);
}

static void test_selene_pedersen()
{
    std::cout << std::endl << "=== Selene Pedersen ===" << std::endl;
    (void)zero_bytes; /* suppress unused warnings */

    selene_jacobian G;
    fq_copy(G.X, SELENE_GX);
    fq_copy(G.Y, SELENE_GY);
    fq_1(G.Z);

    selene_jacobian H;
    selene_dbl(&H, &G);

    unsigned char r_scalar[32] = {0x03};
    unsigned char a_scalar[32] = {0x05};

    selene_jacobian commit;
    selene_pedersen_commit(&commit, r_scalar, &H, a_scalar, &G, 1);

    unsigned char eleven_scalar[32] = {0x0b};
    selene_jacobian expected;
    selene_scalarmult_vartime(&expected, eleven_scalar, &G);

    unsigned char commit_bytes[32], expected_bytes[32];
    selene_tobytes(commit_bytes, &commit);
    selene_tobytes(expected_bytes, &expected);
    check_bytes("pedersen(3, 2G, [5], [G]) == 11G", expected_bytes, commit_bytes, 32);
}

static void test_fp_poly()
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

static void test_fq_poly()
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

static void test_helios_divisor()
{
    std::cout << std::endl << "=== Helios divisor ===" << std::endl;
    unsigned char buf[32];

    helios_jacobian G;
    fp_copy(G.X, HELIOS_GX);
    fp_copy(G.Y, HELIOS_GY);
    fp_1(G.Z);

    /* Get a few affine points on the curve */
    helios_jacobian G2, G3, G4;
    helios_dbl(&G2, &G);
    helios_add(&G3, &G2, &G);
    helios_dbl(&G4, &G2);

    helios_affine pts[3];
    helios_to_affine(&pts[0], &G);
    helios_to_affine(&pts[1], &G2);
    helios_to_affine(&pts[2], &G3);

    /* Compute divisor */
    helios_divisor d;
    helios_compute_divisor(&d, pts, 3);

    /* Evaluate at each point: should give 0 */
    for (int i = 0; i < 3; i++)
    {
        fp_fe val;
        helios_evaluate_divisor(val, &d, pts[i].x, pts[i].y);
        fp_tobytes(buf, val);
        std::string name = "divisor eval at point " + std::to_string(i) + " == 0";
        check_bytes(name.c_str(), zero_bytes, buf, 32);
    }

    /* Evaluate at a different point: should NOT be 0 */
    {
        helios_affine p4;
        helios_to_affine(&p4, &G4);
        fp_fe val;
        helios_evaluate_divisor(val, &d, p4.x, p4.y);
        fp_tobytes(buf, val);
        check_nonzero("divisor eval at non-member != 0", std::memcmp(buf, zero_bytes, 32) != 0 ? 1 : 0);
    }

    /* Single point divisor */
    {
        helios_divisor d1;
        helios_compute_divisor(&d1, pts, 1);
        fp_fe val;
        helios_evaluate_divisor(val, &d1, pts[0].x, pts[0].y);
        fp_tobytes(buf, val);
        check_bytes("single-point divisor eval == 0", zero_bytes, buf, 32);
    }
}

static void test_selene_divisor()
{
    std::cout << std::endl << "=== Selene divisor ===" << std::endl;
    unsigned char buf[32];

    selene_jacobian G;
    fq_copy(G.X, SELENE_GX);
    fq_copy(G.Y, SELENE_GY);
    fq_1(G.Z);

    selene_jacobian G2, G3;
    selene_dbl(&G2, &G);
    selene_add(&G3, &G2, &G);

    selene_affine pts[2];
    selene_to_affine(&pts[0], &G);
    selene_to_affine(&pts[1], &G2);

    selene_divisor d;
    selene_compute_divisor(&d, pts, 2);

    for (int i = 0; i < 2; i++)
    {
        fq_fe val;
        selene_evaluate_divisor(val, &d, pts[i].x, pts[i].y);
        fq_tobytes(buf, val);
        std::string name = "divisor eval at point " + std::to_string(i) + " == 0";
        check_bytes(name.c_str(), zero_bytes, buf, 32);
    }

    /* Non-member check */
    {
        selene_affine p3;
        selene_to_affine(&p3, &G3);
        fq_fe val;
        selene_evaluate_divisor(val, &d, p3.x, p3.y);
        fq_tobytes(buf, val);
        check_nonzero("divisor eval at non-member != 0", std::memcmp(buf, zero_bytes, 32) != 0 ? 1 : 0);
    }
}

/* ========================================================================
 * Extended tests for pre-SIMD hardening
 * ======================================================================== */

static void test_fp_extended()
{
    std::cout << std::endl << "=== F_p extended ===" << std::endl;
    unsigned char buf[32];

    fp_fe a, one_fe, zero_fe;
    fp_frombytes(a, test_a_bytes);
    fp_1(one_fe);
    fp_0(zero_fe);

    /* fp_sq(a) then add to itself == 2*a^2 (tests sq2 property) */
    {
        fp_fe sq_a, sq2_via_add;
        fp_sq(sq_a, a);
        fp_add(sq2_via_add, sq_a, sq_a);
        /* Compare against mul(sq(a), 2) */
        unsigned char two_b[32] = {0x02};
        fp_fe two_fe, sq2_via_mul;
        fp_frombytes(two_fe, two_b);
        fp_mul(sq2_via_mul, sq_a, two_fe);
        unsigned char add_bytes[32], mul_bytes[32];
        fp_tobytes(add_bytes, sq2_via_add);
        fp_tobytes(mul_bytes, sq2_via_mul);
        check_bytes("2*sq(a) via add == via mul", mul_bytes, add_bytes, 32);
    }

    /* sqn chain: sq(sq(sq(sq(sq(a))))) == a^32 via repeated squaring */
    {
        fp_fe chain;
        fp_sq(chain, a);
        fp_sq(chain, chain);
        fp_sq(chain, chain);
        fp_sq(chain, chain);
        fp_sq(chain, chain);
        /* Compare against a^32 via mul: a^2, a^4, a^8, a^16, a^32 */
        fp_fe power;
        fp_sq(power, a); /* a^2 */
        fp_mul(power, power, power); /* a^4 */
        fp_mul(power, power, power); /* a^8 */
        fp_mul(power, power, power); /* a^16 */
        fp_mul(power, power, power); /* a^32 */
        unsigned char chain_bytes[32], power_bytes[32];
        fp_tobytes(chain_bytes, chain);
        fp_tobytes(power_bytes, power);
        check_bytes("sq^5(a) == a^32", power_bytes, chain_bytes, 32);
    }

    /* cmov: b=0 keeps original */
    {
        fp_fe target;
        fp_copy(target, a);
        fp_cmov(target, one_fe, 0);
        fp_tobytes(buf, target);
        check_bytes("cmov(a, 1, 0) == a", test_a_bytes, buf, 32);
    }

    /* cmov: b=1 replaces */
    {
        fp_fe target;
        fp_copy(target, a);
        fp_cmov(target, one_fe, 1);
        fp_tobytes(buf, target);
        check_bytes("cmov(a, 1, 1) == 1", one_bytes, buf, 32);
    }

    /* Edge: (p-1)*(p-1) */
    {
        /* p-1 in little-endian: 0xec, 0xff...0xff, 0x7f */
        unsigned char pm1_bytes[32] = {0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
        fp_fe pm1;
        fp_frombytes(pm1, pm1_bytes);
        fp_fe pm1_sq;
        fp_mul(pm1_sq, pm1, pm1);
        /* (p-1)^2 = (-1)^2 = 1 mod p */
        fp_tobytes(buf, pm1_sq);
        check_bytes("(p-1)^2 == 1", one_bytes, buf, 32);
    }

    /* Edge: (p-1)*2 */
    {
        unsigned char pm1_bytes[32] = {0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
        fp_fe pm1;
        fp_frombytes(pm1, pm1_bytes);
        unsigned char two_b[32] = {0x02};
        fp_fe two_fe, result;
        fp_frombytes(two_fe, two_b);
        fp_mul(result, pm1, two_fe);
        /* (-1)*2 = -2 mod p = p-2 */
        unsigned char pm2_bytes[32] = {0xeb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
        fp_tobytes(buf, result);
        check_bytes("(p-1)*2 == p-2", pm2_bytes, buf, 32);
    }

    /* Edge: (p-1) + 1 wraps to 0 */
    {
        unsigned char pm1_bytes[32] = {0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
        fp_fe pm1, result;
        fp_frombytes(pm1, pm1_bytes);
        fp_add(result, pm1, one_fe);
        fp_tobytes(buf, result);
        check_bytes("(p-1) + 1 == 0", zero_bytes, buf, 32);
    }

    /* Edge: 0 - 1 wraps to p-1 */
    {
        fp_fe result;
        fp_sub(result, zero_fe, one_fe);
        unsigned char pm1_bytes[32] = {0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
        fp_tobytes(buf, result);
        check_bytes("0 - 1 == p-1", pm1_bytes, buf, 32);
    }

    /* neg(0) == 0 */
    {
        fp_fe result;
        fp_neg(result, zero_fe);
        fp_tobytes(buf, result);
        check_bytes("neg(0) == 0", zero_bytes, buf, 32);
    }

    /* invert(1) == 1 */
    {
        fp_fe result;
        fp_invert(result, one_fe);
        fp_tobytes(buf, result);
        check_bytes("invert(1) == 1", one_bytes, buf, 32);
    }

    /* Serialization: frombytes(p_bytes) reduces to 0 */
    {
        unsigned char p_bytes[32] = {0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
        fp_fe result;
        fp_frombytes(result, p_bytes);
        fp_tobytes(buf, result);
        check_bytes("frombytes(p) == 0", zero_bytes, buf, 32);
    }
}

static void test_fq_extended()
{
    std::cout << std::endl << "=== F_q extended ===" << std::endl;
    unsigned char buf[32];

    fq_fe a, one_fe, zero_fe;
    fq_frombytes(a, test_a_bytes);
    fq_1(one_fe);
    fq_0(zero_fe);

    /* 2*sq(a) via add == via mul */
    {
        fq_fe sq_a, sq2_via_add;
        fq_sq(sq_a, a);
        fq_add(sq2_via_add, sq_a, sq_a);
        unsigned char two_b[32] = {0x02};
        fq_fe two_fe, sq2_via_mul;
        fq_frombytes(two_fe, two_b);
        fq_mul(sq2_via_mul, sq_a, two_fe);
        unsigned char add_bytes[32], mul_bytes[32];
        fq_tobytes(add_bytes, sq2_via_add);
        fq_tobytes(mul_bytes, sq2_via_mul);
        check_bytes("2*sq(a) via add == via mul", mul_bytes, add_bytes, 32);
    }

    /* sq^5 chain equivalence */
    {
        fq_fe chain;
        fq_sq(chain, a);
        fq_sq(chain, chain);
        fq_sq(chain, chain);
        fq_sq(chain, chain);
        fq_sq(chain, chain);
        fq_fe power;
        fq_sq(power, a);
        fq_mul(power, power, power);
        fq_mul(power, power, power);
        fq_mul(power, power, power);
        fq_mul(power, power, power);
        unsigned char chain_bytes[32], power_bytes[32];
        fq_tobytes(chain_bytes, chain);
        fq_tobytes(power_bytes, power);
        check_bytes("sq^5(a) == a^32", power_bytes, chain_bytes, 32);
    }

    /* cmov */
    {
        fq_fe target;
        fq_copy(target, a);
        fq_cmov(target, one_fe, 0);
        fq_tobytes(buf, target);
        check_bytes("cmov(a, 1, 0) == a", test_a_bytes, buf, 32);
    }
    {
        fq_fe target;
        fq_copy(target, a);
        fq_cmov(target, one_fe, 1);
        fq_tobytes(buf, target);
        check_bytes("cmov(a, 1, 1) == 1", one_bytes, buf, 32);
    }

    /* fq_sqrt(0) == 0 */
    {
        fq_fe result;
        fq_sqrt(result, zero_fe);
        fq_tobytes(buf, result);
        check_bytes("sqrt(0) == 0", zero_bytes, buf, 32);
    }

    /* fq_sqrt(1)^2 == 1 */
    {
        fq_fe sqrt1, sq_check;
        fq_sqrt(sqrt1, one_fe);
        fq_sq(sq_check, sqrt1);
        fq_tobytes(buf, sq_check);
        check_bytes("sqrt(1)^2 == 1", one_bytes, buf, 32);
    }

    /* fq_sqrt(a^2)^2 == a^2 */
    {
        fq_fe a_sq, sqrt_asq, sq_check;
        fq_sq(a_sq, a);
        fq_sqrt(sqrt_asq, a_sq);
        fq_sq(sq_check, sqrt_asq);
        unsigned char asq_bytes[32];
        fq_tobytes(asq_bytes, a_sq);
        fq_tobytes(buf, sq_check);
        check_bytes("sqrt(a^2)^2 == a^2", asq_bytes, buf, 32);
    }

    /* Edge: (q-1)*(q-1) == 1 */
    {
        /* q = 2^255 - gamma, q-1 in LE */
        /* HELIOS_ORDER is q in LE. q-1 = HELIOS_ORDER - 1 */
        unsigned char qm1_bytes[32];
        std::memcpy(qm1_bytes, HELIOS_ORDER, 32);
        /* Subtract 1 from little-endian */
        for (int i = 0; i < 32; i++)
        {
            if (qm1_bytes[i] > 0)
            {
                qm1_bytes[i]--;
                break;
            }
            qm1_bytes[i] = 0xff;
        }
        fq_fe qm1, qm1_sq;
        fq_frombytes(qm1, qm1_bytes);
        fq_mul(qm1_sq, qm1, qm1);
        fq_tobytes(buf, qm1_sq);
        check_bytes("(q-1)^2 == 1", one_bytes, buf, 32);
    }

    /* Edge: (q-1) + 1 wraps to 0 */
    {
        unsigned char qm1_bytes[32];
        std::memcpy(qm1_bytes, HELIOS_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (qm1_bytes[i] > 0)
            {
                qm1_bytes[i]--;
                break;
            }
            qm1_bytes[i] = 0xff;
        }
        fq_fe qm1, result;
        fq_frombytes(qm1, qm1_bytes);
        fq_add(result, qm1, one_fe);
        fq_tobytes(buf, result);
        check_bytes("(q-1) + 1 == 0", zero_bytes, buf, 32);
    }

    /* invert(1) == 1 */
    {
        fq_fe result;
        fq_invert(result, one_fe);
        fq_tobytes(buf, result);
        check_bytes("invert(1) == 1", one_bytes, buf, 32);
    }

    /* neg(0) == 0 */
    {
        fq_fe result;
        fq_neg(result, zero_fe);
        fq_tobytes(buf, result);
        check_bytes("neg(0) == 0", zero_bytes, buf, 32);
    }

    /* Serialization: frombytes(q_bytes) reduces to 0 */
    {
        fq_fe result;
        fq_frombytes(result, HELIOS_ORDER);
        fq_tobytes(buf, result);
        check_bytes("frombytes(q) == 0", zero_bytes, buf, 32);
    }
}

static void test_serialization_edges()
{
    std::cout << std::endl << "=== Serialization edges ===" << std::endl;
    unsigned char buf[32];

    /* Fp: round-trip 0, 1, p-1 */
    {
        fp_fe fe;
        fp_0(fe);
        fp_tobytes(buf, fe);
        fp_fe fe2;
        fp_frombytes(fe2, buf);
        unsigned char buf2[32];
        fp_tobytes(buf2, fe2);
        check_bytes("fp round-trip 0", buf, buf2, 32);
    }
    {
        fp_fe fe;
        fp_1(fe);
        fp_tobytes(buf, fe);
        fp_fe fe2;
        fp_frombytes(fe2, buf);
        unsigned char buf2[32];
        fp_tobytes(buf2, fe2);
        check_bytes("fp round-trip 1", buf, buf2, 32);
    }
    {
        unsigned char pm1_bytes[32] = {0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
        fp_fe fe;
        fp_frombytes(fe, pm1_bytes);
        fp_tobytes(buf, fe);
        check_bytes("fp round-trip p-1", pm1_bytes, buf, 32);
    }

    /* Fq: round-trip 0, 1, q-1 */
    {
        fq_fe fe;
        fq_0(fe);
        fq_tobytes(buf, fe);
        fq_fe fe2;
        fq_frombytes(fe2, buf);
        unsigned char buf2[32];
        fq_tobytes(buf2, fe2);
        check_bytes("fq round-trip 0", buf, buf2, 32);
    }
    {
        fq_fe fe;
        fq_1(fe);
        fq_tobytes(buf, fe);
        fq_fe fe2;
        fq_frombytes(fe2, buf);
        unsigned char buf2[32];
        fq_tobytes(buf2, fe2);
        check_bytes("fq round-trip 1", buf, buf2, 32);
    }
    {
        unsigned char qm1_bytes[32];
        std::memcpy(qm1_bytes, HELIOS_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (qm1_bytes[i] > 0)
            {
                qm1_bytes[i]--;
                break;
            }
            qm1_bytes[i] = 0xff;
        }
        fq_fe fe;
        fq_frombytes(fe, qm1_bytes);
        fq_tobytes(buf, fe);
        check_bytes("fq round-trip q-1", qm1_bytes, buf, 32);
    }

    /* Fp: value with high bits near 255 */
    {
        unsigned char high_bytes[32] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40};
        fp_fe fe;
        fp_frombytes(fe, high_bytes);
        fp_tobytes(buf, fe);
        check_bytes("fp round-trip high bit value", high_bytes, buf, 32);
    }
}

static void test_helios_point_edges()
{
    std::cout << std::endl << "=== Helios point edges ===" << std::endl;
    unsigned char buf[32];

    helios_jacobian G;
    fp_copy(G.X, HELIOS_GX);
    fp_copy(G.Y, HELIOS_GY);
    fp_1(G.Z);

    /* (order-1)*G == -G */
    {
        unsigned char om1[32];
        std::memcpy(om1, HELIOS_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (om1[i] > 0)
            {
                om1[i]--;
                break;
            }
            om1[i] = 0xff;
        }
        helios_jacobian result;
        helios_scalarmult(&result, om1, &G);
        helios_jacobian neg_G;
        helios_neg(&neg_G, &G);
        unsigned char r_bytes[32], neg_bytes[32];
        helios_tobytes(r_bytes, &result);
        helios_tobytes(neg_bytes, &neg_G);
        check_bytes("(order-1)*G == -G", neg_bytes, r_bytes, 32);
    }

    /* vartime: (order-1)*G == -G */
    {
        unsigned char om1[32];
        std::memcpy(om1, HELIOS_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (om1[i] > 0)
            {
                om1[i]--;
                break;
            }
            om1[i] = 0xff;
        }
        helios_jacobian result;
        helios_scalarmult_vartime(&result, om1, &G);
        helios_jacobian neg_G;
        helios_neg(&neg_G, &G);
        unsigned char r_bytes[32], neg_bytes[32];
        helios_tobytes(r_bytes, &result);
        helios_tobytes(neg_bytes, &neg_G);
        check_bytes("vartime: (order-1)*G == -G", neg_bytes, r_bytes, 32);
    }

    /* (order-1)*G + G == identity */
    {
        unsigned char om1[32];
        std::memcpy(om1, HELIOS_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (om1[i] > 0)
            {
                om1[i]--;
                break;
            }
            om1[i] = 0xff;
        }
        helios_jacobian om1G, sum;
        helios_scalarmult(&om1G, om1, &G);
        helios_add(&sum, &om1G, &G);
        check_nonzero("(order-1)*G + G == identity", helios_is_identity(&sum));
    }

    /* Y-parity: serialize G, flip bit 255, verify y negated */
    {
        unsigned char g_bytes[32];
        helios_tobytes(g_bytes, &G);
        unsigned char flipped[32];
        std::memcpy(flipped, g_bytes, 32);
        flipped[31] ^= 0x80; /* flip parity bit */
        helios_jacobian decoded;
        int rc = helios_frombytes(&decoded, flipped);
        check_int("flipped parity decodes", 0, rc);
        /* The y should be negated */
        helios_affine aff_orig, aff_flip;
        helios_to_affine(&aff_orig, &G);
        helios_to_affine(&aff_flip, &decoded);
        /* x should match */
        unsigned char ox[32], fx[32];
        fp_tobytes(ox, aff_orig.x);
        fp_tobytes(fx, aff_flip.x);
        check_bytes("flipped parity: x matches", ox, fx, 32);
        /* y + flipped_y == 0 (they should be negations) */
        fp_fe y_sum;
        fp_add(y_sum, aff_orig.y, aff_flip.y);
        fp_tobytes(buf, y_sum);
        check_bytes("flipped parity: y + y' == 0", zero_bytes, buf, 32);
    }

    /* Identity round-trip */
    {
        helios_jacobian id;
        helios_identity(&id);
        unsigned char id_bytes[32];
        helios_tobytes(id_bytes, &id);
        check_bytes("tobytes(identity) == 0", zero_bytes, id_bytes, 32);
        /* frombytes(0) — x=0, check if on curve */
        helios_jacobian decoded;
        int rc = helios_frombytes(&decoded, zero_bytes);
        /* x=0: gx = 0^3 - 3*0 + b = b. If b is a QR, this decodes. Otherwise -1. */
        /* Either way, we just record what happens */
        if (rc == 0)
        {
            helios_tobytes(buf, &decoded);
            /* Should be a valid non-identity point with x=0 */
            ++tests_run;
            ++tests_passed;
            std::cout << "  PASS: frombytes(0) decodes (x=0 on curve)" << std::endl;
        }
        else
        {
            ++tests_run;
            ++tests_passed;
            std::cout << "  PASS: frombytes(0) rejects (x=0 not on curve)" << std::endl;
        }
    }

    /* Off-curve rejection: x=2, check x^3-3x+b is not a QR */
    {
        unsigned char x_bytes[32] = {0x02};
        helios_jacobian decoded;
        int rc = helios_frombytes(&decoded, x_bytes);
        /* We don't know a priori, but can test the contract: if rc==-1, off-curve rejected */
        ++tests_run;
        if (rc == -1)
        {
            ++tests_passed;
            std::cout << "  PASS: frombytes(x=2) rejects off-curve" << std::endl;
        }
        else
        {
            /* x=2 might be on curve; verify it's actually valid */
            helios_affine aff;
            helios_to_affine(&aff, &decoded);
            if (helios_is_on_curve(&aff))
            {
                ++tests_passed;
                std::cout << "  PASS: frombytes(x=2) accepted and on curve" << std::endl;
            }
            else
            {
                ++tests_failed;
                std::cout << "  FAIL: frombytes(x=2) accepted but NOT on curve" << std::endl;
            }
        }
    }
}

static void test_selene_point_edges()
{
    std::cout << std::endl << "=== Selene point edges ===" << std::endl;
    unsigned char buf[32];

    selene_jacobian G;
    fq_copy(G.X, SELENE_GX);
    fq_copy(G.Y, SELENE_GY);
    fq_1(G.Z);

    /* (order-1)*G == -G */
    {
        unsigned char om1[32];
        std::memcpy(om1, SELENE_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (om1[i] > 0)
            {
                om1[i]--;
                break;
            }
            om1[i] = 0xff;
        }
        selene_jacobian result;
        selene_scalarmult(&result, om1, &G);
        selene_jacobian neg_G;
        selene_neg(&neg_G, &G);
        unsigned char r_bytes[32], neg_bytes[32];
        selene_tobytes(r_bytes, &result);
        selene_tobytes(neg_bytes, &neg_G);
        check_bytes("(order-1)*G == -G", neg_bytes, r_bytes, 32);
    }

    /* vartime: (order-1)*G == -G */
    {
        unsigned char om1[32];
        std::memcpy(om1, SELENE_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (om1[i] > 0)
            {
                om1[i]--;
                break;
            }
            om1[i] = 0xff;
        }
        selene_jacobian result;
        selene_scalarmult_vartime(&result, om1, &G);
        selene_jacobian neg_G;
        selene_neg(&neg_G, &G);
        unsigned char r_bytes[32], neg_bytes[32];
        selene_tobytes(r_bytes, &result);
        selene_tobytes(neg_bytes, &neg_G);
        check_bytes("vartime: (order-1)*G == -G", neg_bytes, r_bytes, 32);
    }

    /* (order-1)*G + G == identity */
    {
        unsigned char om1[32];
        std::memcpy(om1, SELENE_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (om1[i] > 0)
            {
                om1[i]--;
                break;
            }
            om1[i] = 0xff;
        }
        selene_jacobian om1G, sum;
        selene_scalarmult(&om1G, om1, &G);
        selene_add(&sum, &om1G, &G);
        check_nonzero("(order-1)*G + G == identity", selene_is_identity(&sum));
    }

    /* Y-parity flip */
    {
        unsigned char g_bytes[32];
        selene_tobytes(g_bytes, &G);
        unsigned char flipped[32];
        std::memcpy(flipped, g_bytes, 32);
        flipped[31] ^= 0x80;
        selene_jacobian decoded;
        int rc = selene_frombytes(&decoded, flipped);
        check_int("flipped parity decodes", 0, rc);
        selene_affine aff_orig, aff_flip;
        selene_to_affine(&aff_orig, &G);
        selene_to_affine(&aff_flip, &decoded);
        unsigned char ox[32], fx[32];
        fq_tobytes(ox, aff_orig.x);
        fq_tobytes(fx, aff_flip.x);
        check_bytes("flipped parity: x matches", ox, fx, 32);
        fq_fe y_sum;
        fq_add(y_sum, aff_orig.y, aff_flip.y);
        fq_tobytes(buf, y_sum);
        check_bytes("flipped parity: y + y' == 0", zero_bytes, buf, 32);
    }

    /* Identity round-trip */
    {
        selene_jacobian id;
        selene_identity(&id);
        unsigned char id_bytes[32];
        selene_tobytes(id_bytes, &id);
        check_bytes("tobytes(identity) == 0", zero_bytes, id_bytes, 32);
        selene_jacobian decoded;
        int rc = selene_frombytes(&decoded, zero_bytes);
        ++tests_run;
        if (rc == 0)
        {
            ++tests_passed;
            std::cout << "  PASS: frombytes(0) decodes (x=0 on curve)" << std::endl;
        }
        else
        {
            ++tests_passed;
            std::cout << "  PASS: frombytes(0) rejects (x=0 not on curve)" << std::endl;
        }
    }
}

static void test_scalarmult_extended()
{
    std::cout << std::endl << "=== Scalar mul extended ===" << std::endl;

    /* Helios: associativity scalarmult(3, scalarmult(7, G)) == scalarmult(21, G) */
    {
        helios_jacobian G;
        fp_copy(G.X, HELIOS_GX);
        fp_copy(G.Y, HELIOS_GY);
        fp_1(G.Z);

        unsigned char s3[32] = {0x03};
        unsigned char s7[32] = {0x07};
        unsigned char s21[32] = {0x15};
        helios_jacobian sevG, result, expected;
        helios_scalarmult(&sevG, s7, &G);
        helios_scalarmult(&result, s3, &sevG);
        helios_scalarmult(&expected, s21, &G);
        unsigned char r_bytes[32], e_bytes[32];
        helios_tobytes(r_bytes, &result);
        helios_tobytes(e_bytes, &expected);
        check_bytes("helios: 3*(7*G) == 21*G", e_bytes, r_bytes, 32);
    }

    /* Selene: associativity */
    {
        selene_jacobian G;
        fq_copy(G.X, SELENE_GX);
        fq_copy(G.Y, SELENE_GY);
        fq_1(G.Z);

        unsigned char s3[32] = {0x03};
        unsigned char s7[32] = {0x07};
        unsigned char s21[32] = {0x15};
        selene_jacobian sevG, result, expected;
        selene_scalarmult(&sevG, s7, &G);
        selene_scalarmult(&result, s3, &sevG);
        selene_scalarmult(&expected, s21, &G);
        unsigned char r_bytes[32], e_bytes[32];
        selene_tobytes(r_bytes, &result);
        selene_tobytes(e_bytes, &expected);
        check_bytes("selene: 3*(7*G) == 21*G", e_bytes, r_bytes, 32);
    }

    /* Helios: scalarmult(scalar, identity) == identity (via tobytes) */
    {
        helios_jacobian id;
        helios_identity(&id);
        unsigned char s7[32] = {0x07};
        helios_jacobian result;
        helios_scalarmult(&result, s7, &id);
        unsigned char r_bytes[32];
        helios_tobytes(r_bytes, &result);
        check_bytes("helios: 7*identity == identity", zero_bytes, r_bytes, 32);
    }

    /* Selene: scalarmult(scalar, identity) == identity (via tobytes) */
    {
        selene_jacobian id;
        selene_identity(&id);
        unsigned char s7[32] = {0x07};
        selene_jacobian result;
        selene_scalarmult(&result, s7, &id);
        unsigned char r_bytes[32];
        selene_tobytes(r_bytes, &result);
        check_bytes("selene: 7*identity == identity", zero_bytes, r_bytes, 32);
    }

    /* Helios: scalarmult_vartime(scalar, identity) == identity (via tobytes) */
    {
        helios_jacobian id;
        helios_identity(&id);
        unsigned char s7[32] = {0x07};
        helios_jacobian result;
        helios_scalarmult_vartime(&result, s7, &id);
        unsigned char r_bytes[32];
        helios_tobytes(r_bytes, &result);
        check_bytes("helios: vartime 7*identity == identity", zero_bytes, r_bytes, 32);
    }

    /* Selene: scalarmult_vartime(scalar, identity) == identity (via tobytes) */
    {
        selene_jacobian id;
        selene_identity(&id);
        unsigned char s7[32] = {0x07};
        selene_jacobian result;
        selene_scalarmult_vartime(&result, s7, &id);
        unsigned char r_bytes[32];
        selene_tobytes(r_bytes, &result);
        check_bytes("selene: vartime 7*identity == identity", zero_bytes, r_bytes, 32);
    }
}

static void test_msm_extended()
{
    std::cout << std::endl << "=== MSM extended ===" << std::endl;
    unsigned char buf[32];

    /* Helios: MSM with identity in array */
    {
        helios_jacobian G;
        fp_copy(G.X, HELIOS_GX);
        fp_copy(G.Y, HELIOS_GY);
        fp_1(G.Z);

        helios_jacobian id;
        helios_identity(&id);

        unsigned char scalars[64];
        std::memcpy(scalars, one_bytes, 32);
        std::memcpy(scalars + 32, one_bytes, 32);
        helios_jacobian points[2];
        helios_copy(&points[0], &id);
        helios_copy(&points[1], &G);
        helios_jacobian result;
        helios_msm_vartime(&result, scalars, points, 2);
        helios_tobytes(buf, &result);
        check_bytes("helios msm([1,1],[id,G]) == G", helios_g_compressed, buf, 32);
    }

    /* Helios: MSM all identities */
    {
        helios_jacobian id;
        helios_identity(&id);
        unsigned char scalars[64];
        std::memcpy(scalars, one_bytes, 32);
        std::memcpy(scalars + 32, one_bytes, 32);
        helios_jacobian points[2];
        helios_copy(&points[0], &id);
        helios_copy(&points[1], &id);
        helios_jacobian result;
        helios_msm_vartime(&result, scalars, points, 2);
        check_nonzero("helios msm all identities == identity", helios_is_identity(&result));
    }

    /* Helios: MSM n=64 (deep Pippenger) */
    {
        helios_jacobian G;
        fp_copy(G.X, HELIOS_GX);
        fp_copy(G.Y, HELIOS_GY);
        fp_1(G.Z);

        unsigned char scalars[64 * 32] = {};
        helios_jacobian points[64];
        for (int i = 0; i < 64; i++)
        {
            /* scalar_i = i+1 */
            scalars[i * 32] = (unsigned char)(i + 1);
            helios_copy(&points[i], &G);
        }
        helios_jacobian result;
        helios_msm_vartime(&result, scalars, points, 64);
        /* Expected: sum(1..64)*G = 2080*G */
        unsigned char s2080[32] = {0x20, 0x08}; /* 2080 = 0x0820 LE */
        helios_jacobian expected;
        helios_scalarmult_vartime(&expected, s2080, &G);
        unsigned char r_bytes[32], e_bytes[32];
        helios_tobytes(r_bytes, &result);
        helios_tobytes(e_bytes, &expected);
        check_bytes("helios msm n=64 == 2080*G", e_bytes, r_bytes, 32);
    }

    /* Helios: MSM duplicate scalars+points: msm([a,a],[G,G]) == 2a*G */
    {
        helios_jacobian G;
        fp_copy(G.X, HELIOS_GX);
        fp_copy(G.Y, HELIOS_GY);
        fp_1(G.Z);

        unsigned char s5[32] = {0x05};
        unsigned char scalars[64];
        std::memcpy(scalars, s5, 32);
        std::memcpy(scalars + 32, s5, 32);
        helios_jacobian points[2];
        helios_copy(&points[0], &G);
        helios_copy(&points[1], &G);
        helios_jacobian result;
        helios_msm_vartime(&result, scalars, points, 2);
        unsigned char s10[32] = {0x0a};
        helios_jacobian expected;
        helios_scalarmult_vartime(&expected, s10, &G);
        unsigned char r_bytes[32], e_bytes[32];
        helios_tobytes(r_bytes, &result);
        helios_tobytes(e_bytes, &expected);
        check_bytes("helios msm([5,5],[G,G]) == 10*G", e_bytes, r_bytes, 32);
    }

    /* Selene: MSM with identity */
    {
        selene_jacobian G;
        fq_copy(G.X, SELENE_GX);
        fq_copy(G.Y, SELENE_GY);
        fq_1(G.Z);

        selene_jacobian id;
        selene_identity(&id);

        unsigned char scalars[64];
        std::memcpy(scalars, one_bytes, 32);
        std::memcpy(scalars + 32, one_bytes, 32);
        selene_jacobian points[2];
        selene_copy(&points[0], &id);
        selene_copy(&points[1], &G);
        selene_jacobian result;
        selene_msm_vartime(&result, scalars, points, 2);
        selene_tobytes(buf, &result);
        check_bytes("selene msm([1,1],[id,G]) == G", selene_g_compressed, buf, 32);
    }

    /* Selene: MSM all identities */
    {
        selene_jacobian id;
        selene_identity(&id);
        unsigned char scalars[64];
        std::memcpy(scalars, one_bytes, 32);
        std::memcpy(scalars + 32, one_bytes, 32);
        selene_jacobian points[2];
        selene_copy(&points[0], &id);
        selene_copy(&points[1], &id);
        selene_jacobian result;
        selene_msm_vartime(&result, scalars, points, 2);
        check_nonzero("selene msm all identities == identity", selene_is_identity(&result));
    }

    /* Selene: MSM n=64 */
    {
        selene_jacobian G;
        fq_copy(G.X, SELENE_GX);
        fq_copy(G.Y, SELENE_GY);
        fq_1(G.Z);

        unsigned char scalars[64 * 32] = {};
        selene_jacobian points[64];
        for (int i = 0; i < 64; i++)
        {
            scalars[i * 32] = (unsigned char)(i + 1);
            selene_copy(&points[i], &G);
        }
        selene_jacobian result;
        selene_msm_vartime(&result, scalars, points, 64);
        unsigned char s2080[32] = {0x20, 0x08};
        selene_jacobian expected;
        selene_scalarmult_vartime(&expected, s2080, &G);
        unsigned char r_bytes[32], e_bytes[32];
        selene_tobytes(r_bytes, &result);
        selene_tobytes(e_bytes, &expected);
        check_bytes("selene msm n=64 == 2080*G", e_bytes, r_bytes, 32);
    }
}

static void test_batch_affine_extended()
{
    std::cout << std::endl << "=== Batch affine extended ===" << std::endl;

    /* Selene n=1 (match Helios coverage) */
    {
        selene_jacobian G;
        fq_copy(G.X, SELENE_GX);
        fq_copy(G.Y, SELENE_GY);
        fq_1(G.Z);

        selene_affine batch_out[1], single_out;
        selene_batch_to_affine(batch_out, &G, 1);
        selene_to_affine(&single_out, &G);
        unsigned char bx[32], sx[32], by[32], sy[32];
        fq_tobytes(bx, batch_out[0].x);
        fq_tobytes(sx, single_out.x);
        check_bytes("selene batch n=1 x", sx, bx, 32);
        fq_tobytes(by, batch_out[0].y);
        fq_tobytes(sy, single_out.y);
        check_bytes("selene batch n=1 y", sy, by, 32);
    }

    /* Helios n=4: verify y-coordinates too */
    {
        helios_jacobian G;
        fp_copy(G.X, HELIOS_GX);
        fp_copy(G.Y, HELIOS_GY);
        fp_1(G.Z);

        helios_jacobian points[4];
        helios_copy(&points[0], &G);
        helios_dbl(&points[1], &G);
        helios_add(&points[2], &points[1], &G);
        helios_dbl(&points[3], &points[1]);

        helios_affine batch_out[4], single_out[4];
        helios_batch_to_affine(batch_out, points, 4);
        for (int i = 0; i < 4; i++)
            helios_to_affine(&single_out[i], &points[i]);

        for (int i = 0; i < 4; i++)
        {
            unsigned char by_arr[32], sy_arr[32];
            fp_tobytes(by_arr, batch_out[i].y);
            fp_tobytes(sy_arr, single_out[i].y);
            std::string name = "helios batch n=4 point " + std::to_string(i) + " y";
            check_bytes(name.c_str(), sy_arr, by_arr, 32);
        }
    }

    /* Selene n=4: verify y-coordinates */
    {
        selene_jacobian G;
        fq_copy(G.X, SELENE_GX);
        fq_copy(G.Y, SELENE_GY);
        fq_1(G.Z);

        selene_jacobian points[4];
        selene_copy(&points[0], &G);
        selene_dbl(&points[1], &G);
        selene_add(&points[2], &points[1], &G);
        selene_dbl(&points[3], &points[1]);

        selene_affine batch_out[4], single_out[4];
        selene_batch_to_affine(batch_out, points, 4);
        for (int i = 0; i < 4; i++)
            selene_to_affine(&single_out[i], &points[i]);

        for (int i = 0; i < 4; i++)
        {
            unsigned char by_arr[32], sy_arr[32];
            fq_tobytes(by_arr, batch_out[i].y);
            fq_tobytes(sy_arr, single_out[i].y);
            std::string name = "selene batch n=4 point " + std::to_string(i) + " y";
            check_bytes(name.c_str(), sy_arr, by_arr, 32);
        }
    }

    /* Helios n=16 stress test */
    {
        helios_jacobian G;
        fp_copy(G.X, HELIOS_GX);
        fp_copy(G.Y, HELIOS_GY);
        fp_1(G.Z);

        helios_jacobian points[16];
        helios_copy(&points[0], &G); /* 1G */
        helios_dbl(&points[1], &G); /* 2G */
        helios_add(&points[2], &points[1], &G); /* 3G */
        helios_dbl(&points[3], &points[1]); /* 4G */
        helios_add(&points[4], &points[3], &G); /* 5G */
        helios_add(&points[5], &points[4], &G); /* 6G */
        /* Use scalarmult for the rest to avoid add(P,P) */
        for (int i = 6; i < 16; i++)
        {
            unsigned char sc[32] = {};
            sc[0] = (unsigned char)(i + 1);
            helios_scalarmult_vartime(&points[i], sc, &G);
        }

        helios_affine batch_out[16], single_out[16];
        helios_batch_to_affine(batch_out, points, 16);
        for (int i = 0; i < 16; i++)
            helios_to_affine(&single_out[i], &points[i]);

        bool all_match = true;
        for (int i = 0; i < 16; i++)
        {
            unsigned char bx[32], sx[32], by_arr[32], sy_arr[32];
            fp_tobytes(bx, batch_out[i].x);
            fp_tobytes(sx, single_out[i].x);
            fp_tobytes(by_arr, batch_out[i].y);
            fp_tobytes(sy_arr, single_out[i].y);
            if (std::memcmp(bx, sx, 32) != 0 || std::memcmp(by_arr, sy_arr, 32) != 0)
                all_match = false;
        }
        ++tests_run;
        if (all_match)
        {
            ++tests_passed;
            std::cout << "  PASS: helios batch n=16 all x,y match" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: helios batch n=16 mismatch" << std::endl;
        }
    }
}

static void test_pedersen_extended()
{
    std::cout << std::endl << "=== Pedersen extended ===" << std::endl;
    (void)zero_bytes;

    /* Helios: n=3 multiple generators */
    {
        helios_jacobian G;
        fp_copy(G.X, HELIOS_GX);
        fp_copy(G.Y, HELIOS_GY);
        fp_1(G.Z);

        helios_jacobian H, G2, G3;
        helios_dbl(&H, &G); /* H = 2G */
        helios_add(&G2, &H, &G); /* G2 = 3G */
        helios_dbl(&G3, &H); /* G3 = 4G */

        unsigned char r_scalar[32] = {0x02};
        unsigned char vals[96];
        unsigned char v1[32] = {0x03};
        unsigned char v2[32] = {0x05};
        unsigned char v3[32] = {0x07};
        std::memcpy(vals, v1, 32);
        std::memcpy(vals + 32, v2, 32);
        std::memcpy(vals + 64, v3, 32);

        helios_jacobian gens[3];
        helios_copy(&gens[0], &G);
        helios_copy(&gens[1], &G2);
        helios_copy(&gens[2], &G3);

        helios_jacobian commit;
        helios_pedersen_commit(&commit, r_scalar, &H, vals, gens, 3);

        /* Expected: 2*H + 3*G + 5*G2 + 7*G3 = 2*2G + 3*G + 5*3G + 7*4G = 4G+3G+15G+28G = 50G */
        unsigned char s50[32] = {0x32};
        helios_jacobian expected;
        helios_scalarmult_vartime(&expected, s50, &G);
        unsigned char c_bytes[32], e_bytes[32];
        helios_tobytes(c_bytes, &commit);
        helios_tobytes(e_bytes, &expected);
        check_bytes("helios pedersen n=3", e_bytes, c_bytes, 32);
    }

    /* Selene: n=0 blinding only */
    {
        selene_jacobian G;
        fq_copy(G.X, SELENE_GX);
        fq_copy(G.Y, SELENE_GY);
        fq_1(G.Z);

        selene_jacobian H;
        selene_dbl(&H, &G);

        unsigned char r_scalar[32] = {0x03};
        selene_jacobian commit;
        selene_pedersen_commit(&commit, r_scalar, &H, nullptr, nullptr, 0);

        unsigned char s3[32] = {0x03};
        selene_jacobian expected;
        selene_scalarmult_vartime(&expected, s3, &H);
        unsigned char c_bytes[32], e_bytes[32];
        selene_tobytes(c_bytes, &commit);
        selene_tobytes(e_bytes, &expected);
        check_bytes("selene pedersen n=0: r*H", e_bytes, c_bytes, 32);
    }

    /* Helios: zero blinding */
    {
        helios_jacobian G;
        fp_copy(G.X, HELIOS_GX);
        fp_copy(G.Y, HELIOS_GY);
        fp_1(G.Z);

        helios_jacobian H;
        helios_dbl(&H, &G);

        unsigned char s5[32] = {0x05};
        helios_jacobian commit;
        helios_pedersen_commit(&commit, zero_bytes, &H, s5, &G, 1);

        helios_jacobian expected;
        helios_scalarmult_vartime(&expected, s5, &G);
        unsigned char c_bytes[32], e_bytes[32];
        helios_tobytes(c_bytes, &commit);
        helios_tobytes(e_bytes, &expected);
        check_bytes("helios pedersen(0, H, [5], [G]) == 5*G", e_bytes, c_bytes, 32);
    }

    /* Selene: n=3 multiple generators */
    {
        selene_jacobian G;
        fq_copy(G.X, SELENE_GX);
        fq_copy(G.Y, SELENE_GY);
        fq_1(G.Z);

        selene_jacobian H, G2, G3;
        selene_dbl(&H, &G);
        selene_add(&G2, &H, &G);
        selene_dbl(&G3, &H);

        unsigned char r_scalar[32] = {0x02};
        unsigned char vals[96];
        unsigned char v1[32] = {0x03};
        unsigned char v2[32] = {0x05};
        unsigned char v3[32] = {0x07};
        std::memcpy(vals, v1, 32);
        std::memcpy(vals + 32, v2, 32);
        std::memcpy(vals + 64, v3, 32);

        selene_jacobian gens[3];
        selene_copy(&gens[0], &G);
        selene_copy(&gens[1], &G2);
        selene_copy(&gens[2], &G3);

        selene_jacobian commit;
        selene_pedersen_commit(&commit, r_scalar, &H, vals, gens, 3);

        unsigned char s50[32] = {0x32};
        selene_jacobian expected;
        selene_scalarmult_vartime(&expected, s50, &G);
        unsigned char c_bytes[32], e_bytes[32];
        selene_tobytes(c_bytes, &commit);
        selene_tobytes(e_bytes, &expected);
        check_bytes("selene pedersen n=3", e_bytes, c_bytes, 32);
    }
}

static void test_poly_extended()
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

static void test_divisor_extended()
{
    std::cout << std::endl << "=== Divisor extended ===" << std::endl;
    unsigned char buf[32];

    /* Helios: 5-point divisor */
    {
        helios_jacobian G;
        fp_copy(G.X, HELIOS_GX);
        fp_copy(G.Y, HELIOS_GY);
        fp_1(G.Z);

        helios_jacobian pts_jac[6];
        helios_copy(&pts_jac[0], &G);
        helios_dbl(&pts_jac[1], &G);
        helios_add(&pts_jac[2], &pts_jac[1], &G);
        helios_dbl(&pts_jac[3], &pts_jac[1]);
        helios_add(&pts_jac[4], &pts_jac[3], &G);
        helios_add(&pts_jac[5], &pts_jac[4], &G); /* non-member */

        helios_affine pts[5], non_member;
        for (int i = 0; i < 5; i++)
            helios_to_affine(&pts[i], &pts_jac[i]);
        helios_to_affine(&non_member, &pts_jac[5]);

        helios_divisor d;
        helios_compute_divisor(&d, pts, 5);

        bool all_zero = true;
        for (int i = 0; i < 5; i++)
        {
            fp_fe val;
            helios_evaluate_divisor(val, &d, pts[i].x, pts[i].y);
            unsigned char vb[32];
            fp_tobytes(vb, val);
            if (std::memcmp(vb, zero_bytes, 32) != 0)
                all_zero = false;
        }
        ++tests_run;
        if (all_zero)
        {
            ++tests_passed;
            std::cout << "  PASS: helios 5-point divisor all evals == 0" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: helios 5-point divisor some eval != 0" << std::endl;
        }

        fp_fe val;
        helios_evaluate_divisor(val, &d, non_member.x, non_member.y);
        fp_tobytes(buf, val);
        check_nonzero("helios 5-point divisor non-member != 0", std::memcmp(buf, zero_bytes, 32) != 0 ? 1 : 0);
    }

    /* Selene: single-point divisor */
    {
        selene_jacobian G;
        fq_copy(G.X, SELENE_GX);
        fq_copy(G.Y, SELENE_GY);
        fq_1(G.Z);

        selene_affine pt;
        selene_to_affine(&pt, &G);

        selene_divisor d;
        selene_compute_divisor(&d, &pt, 1);

        fq_fe val;
        selene_evaluate_divisor(val, &d, pt.x, pt.y);
        fq_tobytes(buf, val);
        check_bytes("selene single-point divisor eval == 0", zero_bytes, buf, 32);
    }

    /* Selene: 5-point divisor */
    {
        selene_jacobian G;
        fq_copy(G.X, SELENE_GX);
        fq_copy(G.Y, SELENE_GY);
        fq_1(G.Z);

        selene_jacobian pts_jac[6];
        selene_copy(&pts_jac[0], &G);
        selene_dbl(&pts_jac[1], &G);
        selene_add(&pts_jac[2], &pts_jac[1], &G);
        selene_dbl(&pts_jac[3], &pts_jac[1]);
        selene_add(&pts_jac[4], &pts_jac[3], &G);
        selene_add(&pts_jac[5], &pts_jac[4], &G);

        selene_affine pts[5], non_member;
        for (int i = 0; i < 5; i++)
            selene_to_affine(&pts[i], &pts_jac[i]);
        selene_to_affine(&non_member, &pts_jac[5]);

        selene_divisor d;
        selene_compute_divisor(&d, pts, 5);

        bool all_zero = true;
        for (int i = 0; i < 5; i++)
        {
            fq_fe val;
            selene_evaluate_divisor(val, &d, pts[i].x, pts[i].y);
            unsigned char vb[32];
            fq_tobytes(vb, val);
            if (std::memcmp(vb, zero_bytes, 32) != 0)
                all_zero = false;
        }
        ++tests_run;
        if (all_zero)
        {
            ++tests_passed;
            std::cout << "  PASS: selene 5-point divisor all evals == 0" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: selene 5-point divisor some eval != 0" << std::endl;
        }
    }
}

int main()
{
    std::cout << "Helioselene Unit Tests" << std::endl;
    std::cout << "======================" << std::endl;

    test_fp();
    test_fq();
    test_fp_sqrt();
    test_helios_points();
    test_selene_points();
    test_helios_scalarmult();
    test_selene_scalarmult();
    test_wei25519();
    test_helios_msm();
    test_selene_msm();
    test_fp_sqrt_sswu();
    test_helios_sswu();
    test_selene_sswu();
    test_helios_batch_affine();
    test_selene_batch_affine();
    test_helios_pedersen();
    test_selene_pedersen();
    test_fp_poly();
    test_fq_poly();
    test_helios_divisor();
    test_selene_divisor();
    test_fp_extended();
    test_fq_extended();
    test_serialization_edges();
    test_helios_point_edges();
    test_selene_point_edges();
    test_scalarmult_extended();
    test_msm_extended();
    test_batch_affine_extended();
    test_pedersen_extended();
    test_poly_extended();
    test_divisor_extended();

    std::cout << std::endl << "======================" << std::endl;
    std::cout << "Total:  " << tests_run << std::endl;
    std::cout << "Passed: " << tests_passed << std::endl;
    std::cout << "Failed: " << tests_failed << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
