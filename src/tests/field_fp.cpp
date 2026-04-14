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

void test_fp()
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
    check_bytes("a * b", tv::fp_field::mul_ab, buf, 32);

    fp_mul(d, b, a);
    fp_tobytes(buf, d);
    check_bytes("b * a == a * b", tv::fp_field::mul_ab, buf, 32);

    fp_sq(c, a);
    fp_tobytes(buf, c);
    check_bytes("a^2", tv::fp_field::sq_a, buf, 32);

    fp_mul(d, a, a);
    fp_tobytes(buf, d);
    check_bytes("sq(a) == mul(a,a)", tv::fp_field::sq_a, buf, 32);

    fp_mul(c, a, one);
    fp_tobytes(buf, c);
    check_bytes("a * 1 == a", test_a_bytes, buf, 32);

    fp_fe inv_a;
    fp_invert(inv_a, a);
    fp_tobytes(buf, inv_a);
    check_bytes("inv(a)", tv::fp_field::inv_a, buf, 32);

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


void test_fp_sqrt()
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


void test_fp_sqrt_sswu()
{
    std::cout << std::endl << "=== F_p sqrt (SSWU gx2) ===" << std::endl;
    unsigned char buf[32];

    /* gx2 for SSWU u=1, known to be a QR */
    const unsigned char *gx2_bytes = tv::sswu_vectors::ran_gx2_u1;
    const unsigned char *y_expected = tv::sswu_vectors::ran_y_u1;

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
    const unsigned char *x2_bytes = tv::sswu_vectors::ran_x2_u1;
    fp_fe x2_fe, x2_sq, x2_cu, gx_computed;
    fp_frombytes(x2_fe, x2_bytes);
    fp_sq(x2_sq, x2_fe);
    fp_mul(x2_cu, x2_sq, x2_fe);

    /* A = -3 mod p */
    fp_fe three_x;
    fp_add(three_x, x2_fe, x2_fe);
    fp_add(three_x, three_x, x2_fe);
    fp_sub(gx_computed, x2_cu, three_x);
    fp_add(gx_computed, gx_computed, RAN_B);
    fp_tobytes(buf, gx_computed);
    check_bytes("gx from x2 matches gx2", gx2_bytes, buf, 32);
}


void test_fp_extended()
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

    /* Curve constant: RAN_B3 == 3 * RAN_B mod p (precomputed for RCB complete addition) */
    {
        fp_fe sum, diff;
        fp_add(sum, RAN_B, RAN_B);
        fp_add(sum, sum, RAN_B);
        fp_sub(diff, sum, RAN_B3);
        fp_tobytes(buf, diff);
        check_bytes("RAN_B3 == 3 * RAN_B mod p", zero_bytes, buf, 32);
    }
}
