#include <cstdio>
#include <cstring>

#include "fp.h"
#include "fp_ops.h"
#include "fp_mul.h"
#include "fp_sq.h"
#include "fp_tobytes.h"
#include "fp_frombytes.h"
#include "fp_invert.h"
#include "fp_pow22523.h"

#include "fq.h"
#include "fq_ops.h"
#include "fq_mul.h"
#include "fq_sq.h"
#include "fq_tobytes.h"
#include "fq_frombytes.h"
#include "fq_invert.h"
#include "fq_sqrt.h"

static int tests_passed = 0;
static int tests_failed = 0;

static void check_bytes(const char *name, const unsigned char *got, const unsigned char *expected, int len)
{
    if (std::memcmp(got, expected, len) == 0)
    {
        tests_passed++;
    }
    else
    {
        tests_failed++;
        std::printf("FAIL: %s\n  got:      ", name);
        for (int i = 0; i < len; i++)
            std::printf("%02x", got[i]);
        std::printf("\n  expected: ");
        for (int i = 0; i < len; i++)
            std::printf("%02x", expected[i]);
        std::printf("\n");
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

static void test_fp()
{
    std::printf("--- F_p tests ---\n");
    unsigned char buf[32];

    fp_fe a, b, c, d;
    fp_frombytes(a, test_a_bytes);
    fp_frombytes(b, test_b_bytes);

    /* Serialization round-trip */
    fp_tobytes(buf, a);
    check_bytes("fp: tobytes(frombytes(a)) == a", buf, test_a_bytes, 32);

    /* Zero */
    fp_fe zero;
    fp_0(zero);
    fp_tobytes(buf, zero);
    check_bytes("fp: tobytes(0)", buf, zero_bytes, 32);

    /* One */
    fp_fe one;
    fp_1(one);
    fp_tobytes(buf, one);
    check_bytes("fp: tobytes(1)", buf, one_bytes, 32);

    /* Addition identity: a + 0 = a */
    fp_add(c, a, zero);
    fp_tobytes(buf, c);
    check_bytes("fp: a + 0 == a", buf, test_a_bytes, 32);

    /* Multiplication: a * b */
    fp_mul(c, a, b);
    fp_tobytes(buf, c);
    check_bytes("fp: a * b", buf, fp_ab_bytes, 32);

    /* Commutativity: a * b = b * a */
    fp_mul(d, b, a);
    fp_tobytes(buf, d);
    check_bytes("fp: b * a == a * b", buf, fp_ab_bytes, 32);

    /* Squaring: a^2 */
    fp_sq(c, a);
    fp_tobytes(buf, c);
    check_bytes("fp: a^2", buf, fp_asq_bytes, 32);

    /* sq(a) == mul(a,a) */
    fp_mul(d, a, a);
    fp_tobytes(buf, d);
    check_bytes("fp: sq(a) == mul(a,a)", buf, fp_asq_bytes, 32);

    /* Mul by one: a * 1 = a */
    fp_mul(c, a, one);
    fp_tobytes(buf, c);
    check_bytes("fp: a * 1 == a", buf, test_a_bytes, 32);

    /* Inversion: inv(a) * a = 1 */
    fp_fe inv_a;
    fp_invert(inv_a, a);
    fp_tobytes(buf, inv_a);
    check_bytes("fp: inv(a)", buf, fp_ainv_bytes, 32);

    fp_mul(c, inv_a, a);
    fp_tobytes(buf, c);
    check_bytes("fp: inv(a) * a == 1", buf, one_bytes, 32);

    /* Subtraction: a - a = 0 */
    fp_sub(c, a, a);
    fp_tobytes(buf, c);
    check_bytes("fp: a - a == 0", buf, zero_bytes, 32);

    /* Negation: a + (-a) = 0 */
    fp_neg(d, a);
    fp_add(c, a, d);
    fp_tobytes(buf, c);
    check_bytes("fp: a + (-a) == 0", buf, zero_bytes, 32);
}

static void test_fq()
{
    std::printf("--- F_q tests ---\n");
    unsigned char buf[32];

    fq_fe a, b, c, d;
    fq_frombytes(a, test_a_bytes);
    fq_frombytes(b, test_b_bytes);

    /* Serialization round-trip */
    fq_tobytes(buf, a);
    check_bytes("fq: tobytes(frombytes(a)) == a", buf, test_a_bytes, 32);

    /* Zero */
    fq_fe zero;
    fq_0(zero);
    fq_tobytes(buf, zero);
    check_bytes("fq: tobytes(0)", buf, zero_bytes, 32);

    /* One */
    fq_fe one;
    fq_1(one);
    fq_tobytes(buf, one);
    check_bytes("fq: tobytes(1)", buf, one_bytes, 32);

    /* Addition identity: a + 0 = a */
    fq_add(c, a, zero);
    fq_tobytes(buf, c);
    check_bytes("fq: a + 0 == a", buf, test_a_bytes, 32);

    /* Multiplication: a * b */
    fq_mul(c, a, b);
    fq_tobytes(buf, c);
    check_bytes("fq: a * b", buf, fq_ab_bytes, 32);

    /* Commutativity: a * b = b * a */
    fq_mul(d, b, a);
    fq_tobytes(buf, d);
    check_bytes("fq: b * a == a * b", buf, fq_ab_bytes, 32);

    /* Squaring: a^2 */
    fq_sq(c, a);
    fq_tobytes(buf, c);
    check_bytes("fq: a^2", buf, fq_asq_bytes, 32);

    /* sq(a) == mul(a,a) */
    fq_mul(d, a, a);
    fq_tobytes(buf, d);
    check_bytes("fq: sq(a) == mul(a,a)", buf, fq_asq_bytes, 32);

    /* Mul by one: a * 1 = a */
    fq_mul(c, a, one);
    fq_tobytes(buf, c);
    check_bytes("fq: a * 1 == a", buf, test_a_bytes, 32);

    /* Inversion: inv(a) * a = 1 */
    fq_fe inv_a;
    fq_invert(inv_a, a);
    fq_tobytes(buf, inv_a);
    check_bytes("fq: inv(a)", buf, fq_ainv_bytes, 32);

    fq_mul(c, inv_a, a);
    fq_tobytes(buf, c);
    check_bytes("fq: inv(a) * a == 1", buf, one_bytes, 32);

    /* Subtraction: a - a = 0 */
    fq_sub(c, a, a);
    fq_tobytes(buf, c);
    check_bytes("fq: a - a == 0", buf, zero_bytes, 32);

    /* Negation: a + (-a) = 0 */
    fq_neg(d, a);
    fq_add(c, a, d);
    fq_tobytes(buf, c);
    check_bytes("fq: a + (-a) == 0", buf, zero_bytes, 32);

    /* Square root: sqrt(4) = 2 */
    fq_fe four;
    fq_frombytes(four, four_bytes);
    fq_fe sqrt4;
    fq_sqrt(sqrt4, four);
    fq_tobytes(buf, sqrt4);
    check_bytes("fq: sqrt(4) == 2", buf, fq_sqrt4_bytes, 32);

    /* Verify sqrt: sqrt(4)^2 = 4 */
    fq_sq(c, sqrt4);
    fq_tobytes(buf, c);
    check_bytes("fq: sqrt(4)^2 == 4", buf, four_bytes, 32);
}

int main()
{
    std::printf("helioselene field arithmetic tests\n\n");

    test_fp();
    std::printf("\n");
    test_fq();

    std::printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
