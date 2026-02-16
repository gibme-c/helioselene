#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "fp.h"
#include "fp_ops.h"
#include "fp_mul.h"
#include "fp_sq.h"
#include "fp_tobytes.h"
#include "fp_frombytes.h"
#include "fp_invert.h"
#include "fp_pow22523.h"
#include "fp_sqrt.h"
#include "fp_utils.h"

#include "fq.h"
#include "fq_ops.h"
#include "fq_mul.h"
#include "fq_sq.h"
#include "fq_tobytes.h"
#include "fq_frombytes.h"
#include "fq_invert.h"
#include "fq_sqrt.h"
#include "fq_utils.h"

#include "helios.h"
#include "helios_constants.h"
#include "helios_ops.h"
#include "helios_dbl.h"
#include "helios_madd.h"
#include "helios_add.h"
#include "helios_validate.h"
#include "helios_tobytes.h"
#include "helios_frombytes.h"
#include "helios_scalarmult.h"
#include "helios_scalarmult_vartime.h"

#include "selene.h"
#include "selene_constants.h"
#include "selene_ops.h"
#include "selene_dbl.h"
#include "selene_madd.h"
#include "selene_add.h"
#include "selene_validate.h"
#include "selene_tobytes.h"
#include "selene_frombytes.h"
#include "selene_scalarmult.h"
#include "selene_scalarmult_vartime.h"

#include "helioselene_wei25519.h"

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

static bool check_bytes(const char *test_name, const unsigned char *expected,
    const unsigned char *actual, size_t len)
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
    0xca, 0xef, 0xbe, 0xad, 0xde, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00};
static const unsigned char test_b_bytes[32] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x0d, 0xf0, 0xad,
    0xba, 0xce, 0xfa, 0xed, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00};
static const unsigned char one_bytes[32] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00};
static const unsigned char zero_bytes[32] = {0};
static const unsigned char four_bytes[32] = {0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00};

/* F_p known-answer vectors */
static const unsigned char fp_ab_bytes[32] = {0x8b, 0xf8, 0x99, 0xb6, 0x81, 0xc3, 0x9d, 0x32, 0x37, 0x91, 0x83,
    0xab, 0x63, 0xdf, 0xe3, 0x39, 0x5a, 0xbb, 0x62, 0xcf, 0x01, 0xdb, 0x9b, 0x07, 0x40, 0x05, 0x0f, 0x2e, 0x75,
    0x64, 0xbf, 0x5d};
static const unsigned char fp_asq_bytes[32] = {0x34, 0xa5, 0xf2, 0xa2, 0x09, 0x5f, 0x47, 0xa6, 0x80, 0x23, 0x11,
    0x6b, 0x38, 0x72, 0xb0, 0xef, 0x20, 0x65, 0x11, 0xb6, 0xcc, 0x2e, 0x41, 0xd2, 0x18, 0xfa, 0x92, 0x82, 0x13,
    0xcd, 0xb1, 0x41};
static const unsigned char fp_ainv_bytes[32] = {0x3f, 0x3a, 0x94, 0xed, 0xea, 0xf4, 0x00, 0xef, 0x56, 0x09, 0xc0,
    0x94, 0xeb, 0x93, 0x22, 0xcb, 0x71, 0x87, 0x3d, 0x9b, 0x45, 0x9c, 0xde, 0xf4, 0x0a, 0x20, 0x13, 0xc1, 0xfc,
    0x61, 0x66, 0x25};

/* F_q known-answer vectors */
static const unsigned char fq_ab_bytes[32] = {0xd9, 0x30, 0x72, 0x3d, 0x0f, 0xf1, 0xe6, 0xc3, 0xde, 0x25, 0x1e,
    0xf4, 0x36, 0x67, 0x64, 0x7a, 0x5a, 0xbb, 0x62, 0xcf, 0x01, 0xdb, 0x9b, 0x07, 0x40, 0x05, 0x0f, 0x2e, 0x75,
    0x64, 0xbf, 0x5d};
static const unsigned char fq_asq_bytes[32] = {0x82, 0xdd, 0xca, 0x29, 0x97, 0x8c, 0x90, 0x37, 0x28, 0xb8, 0xab,
    0xb3, 0x0b, 0xfa, 0x30, 0x30, 0x21, 0x65, 0x11, 0xb6, 0xcc, 0x2e, 0x41, 0xd2, 0x18, 0xfa, 0x92, 0x82, 0x13,
    0xcd, 0xb1, 0x41};
static const unsigned char fq_ainv_bytes[32] = {0xee, 0xe9, 0xdc, 0xce, 0x6d, 0x37, 0x57, 0xf1, 0xfd, 0x90, 0x58,
    0xf5, 0xff, 0xff, 0x5f, 0xb3, 0x30, 0x3c, 0xb4, 0xb2, 0x81, 0x4a, 0xb8, 0x4f, 0xcf, 0xbe, 0x50, 0xe0, 0x6b,
    0x8e, 0xe1, 0x60};
static const unsigned char fq_sqrt4_bytes[32] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00};

/* Helios compressed point test vectors */
static const unsigned char helios_g_compressed[32] = {
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char helios_2g_compressed[32] = {
    0x26, 0x29, 0x42, 0x40, 0x80, 0x90, 0xb3, 0xc5,
    0x07, 0xb8, 0xac, 0x94, 0xd4, 0x6f, 0xc4, 0x95,
    0xfc, 0x12, 0x9f, 0xb4, 0xd1, 0x65, 0x37, 0x24,
    0x11, 0xd5, 0xe5, 0xea, 0x00, 0x84, 0x02, 0xf2
};

static const unsigned char helios_7g_compressed[32] = {
    0x03, 0xdf, 0x58, 0xab, 0x3f, 0x90, 0x99, 0xc3,
    0x4d, 0x76, 0x64, 0x2b, 0x4c, 0x99, 0xe5, 0x82,
    0xe3, 0x8c, 0xf4, 0x7e, 0x1b, 0xee, 0x44, 0x4c,
    0x48, 0x17, 0xa4, 0x81, 0xba, 0x49, 0x98, 0x26
};

/* Selene compressed point test vectors */
static const unsigned char selene_g_compressed[32] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char selene_2g_compressed[32] = {
    0x9d, 0xc7, 0x27, 0x79, 0x72, 0xd2, 0xb6, 0x6e,
    0x58, 0x6b, 0x65, 0xb7, 0x2c, 0x78, 0x7f, 0xbf,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static const unsigned char selene_7g_compressed[32] = {
    0x99, 0x30, 0x21, 0x4d, 0xf2, 0x35, 0x94, 0x1d,
    0xba, 0x78, 0xb6, 0x1c, 0xeb, 0xf3, 0x81, 0x2c,
    0x69, 0xc0, 0x43, 0x18, 0x28, 0xf9, 0x08, 0x9e,
    0x01, 0x69, 0x5d, 0x8a, 0xfd, 0x58, 0xbe, 0x2f
};

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

    unsigned char invalid_bytes[32] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
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

    unsigned char invalid_bytes[32] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
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

    unsigned char scalar_a[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12,
        0xbe, 0xba, 0xfe, 0xca, 0xef, 0xbe, 0xad, 0xde,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
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

    unsigned char scalar_a[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12,
        0xbe, 0xba, 0xfe, 0xca, 0xef, 0xbe, 0xad, 0xde,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};
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

    unsigned char p_bytes[32] = {
        0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f
    };
    rc = helioselene_wei25519_to_fp(out, p_bytes);
    check_int("x == p rejected", -1, rc);

    unsigned char high_bit[32] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80};
    rc = helioselene_wei25519_to_fp(out, high_bit);
    check_int("bit 255 set rejected", -1, rc);
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

    std::cout << std::endl << "======================" << std::endl;
    std::cout << "Total:  " << tests_run << std::endl;
    std::cout << "Passed: " << tests_passed << std::endl;
    std::cout << "Failed: " << tests_failed << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
