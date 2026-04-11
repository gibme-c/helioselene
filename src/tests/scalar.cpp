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

void test_ran_scalar()
{
    std::cout << std::endl << "=== Ran scalar ===" << std::endl;

    fq_fe a, b;
    fq_frombytes(a, test_a_bytes);
    fq_frombytes(b, test_b_bytes);

    /* a + (-a) == 0 */
    {
        fq_fe neg_a, sum;
        ran_scalar_neg(neg_a, a);
        ran_scalar_add(sum, a, neg_a);
        unsigned char out[32];
        ran_scalar_to_bytes(out, sum);
        check_bytes("ran scalar a + (-a) == 0", zero_bytes, out, 32);
    }

    /* a * 1 == a */
    {
        fq_fe one, prod;
        ran_scalar_one(one);
        ran_scalar_mul(prod, a, one);
        unsigned char out[32], expected[32];
        ran_scalar_to_bytes(out, prod);
        ran_scalar_to_bytes(expected, a);
        check_bytes("ran scalar a * 1 == a", expected, out, 32);
    }

    /* a * a^(-1) == 1 */
    {
        fq_fe inv, prod;
        ran_scalar_invert(inv, a);
        ran_scalar_mul(prod, a, inv);
        unsigned char out[32];
        ran_scalar_to_bytes(out, prod);
        check_bytes("ran scalar a * a^-1 == 1", one_bytes, out, 32);
    }

    /* Distributivity: a * (b + c) == a*b + a*c where c = 1 */
    {
        fq_fe one, b_plus_one, lhs, ab, a_one, rhs;
        ran_scalar_one(one);
        ran_scalar_add(b_plus_one, b, one);
        ran_scalar_mul(lhs, a, b_plus_one);
        ran_scalar_mul(ab, a, b);
        ran_scalar_mul(a_one, a, one);
        ran_scalar_add(rhs, ab, a_one);
        unsigned char lhs_bytes[32], rhs_bytes[32];
        ran_scalar_to_bytes(lhs_bytes, lhs);
        ran_scalar_to_bytes(rhs_bytes, rhs);
        check_bytes("ran scalar distributivity", lhs_bytes, rhs_bytes, 32);
    }

    /* Serialization round-trip */
    {
        unsigned char buf[32];
        ran_scalar_to_bytes(buf, a);
        fq_fe a2;
        ran_scalar_from_bytes(a2, buf);
        unsigned char buf2[32];
        ran_scalar_to_bytes(buf2, a2);
        check_bytes("ran scalar round-trip", buf, buf2, 32);
    }

    /* is_zero */
    {
        fq_fe z;
        ran_scalar_zero(z);
        check_int("ran scalar is_zero(0)", 1, ran_scalar_is_zero(z));
        check_int("ran scalar !is_zero(a)", 0, ran_scalar_is_zero(a));
    }

    /* Wide reduction: reduce known 64-byte value */
    {
        /* All-zero 64 bytes should give zero */
        unsigned char wide_zero[64] = {};
        fq_fe result;
        ran_scalar_reduce_wide(result, wide_zero);
        unsigned char out[32];
        ran_scalar_to_bytes(out, result);
        check_bytes("ran scalar reduce_wide(0) == 0", zero_bytes, out, 32);

        /* lo = 1, hi = 0 -> result = 1 */
        unsigned char wide_one[64] = {0x01};
        ran_scalar_reduce_wide(result, wide_one);
        ran_scalar_to_bytes(out, result);
        check_bytes("ran scalar reduce_wide(lo=1,hi=0) == 1", one_bytes, out, 32);
    }

    /* muladd: a*b + 1 == a*b + 1 (computed two ways) */
    {
        fq_fe one, ab, ab_plus_one, muladd_result;
        ran_scalar_one(one);
        ran_scalar_mul(ab, a, b);
        ran_scalar_add(ab_plus_one, ab, one);
        ran_scalar_muladd(muladd_result, a, b, one);
        unsigned char out1[32], out2[32];
        ran_scalar_to_bytes(out1, ab_plus_one);
        ran_scalar_to_bytes(out2, muladd_result);
        check_bytes("ran scalar muladd(a,b,1) == a*b+1", out1, out2, 32);
    }

    /* sq: a^2 == a*a */
    {
        fq_fe sq_result, mul_result;
        ran_scalar_sq(sq_result, a);
        ran_scalar_mul(mul_result, a, a);
        unsigned char out1[32], out2[32];
        ran_scalar_to_bytes(out1, sq_result);
        ran_scalar_to_bytes(out2, mul_result);
        check_bytes("ran scalar sq(a) == a*a", out1, out2, 32);
    }
}


void test_shaw_scalar()
{
    std::cout << std::endl << "=== Shaw scalar ===" << std::endl;

    fp_fe a, b;
    fp_frombytes(a, test_a_bytes);
    fp_frombytes(b, test_b_bytes);

    /* a + (-a) == 0 */
    {
        fp_fe neg_a, sum;
        shaw_scalar_neg(neg_a, a);
        shaw_scalar_add(sum, a, neg_a);
        unsigned char out[32];
        shaw_scalar_to_bytes(out, sum);
        check_bytes("shaw scalar a + (-a) == 0", zero_bytes, out, 32);
    }

    /* a * 1 == a */
    {
        fp_fe one, prod;
        shaw_scalar_one(one);
        shaw_scalar_mul(prod, a, one);
        unsigned char out[32], expected[32];
        shaw_scalar_to_bytes(out, prod);
        shaw_scalar_to_bytes(expected, a);
        check_bytes("shaw scalar a * 1 == a", expected, out, 32);
    }

    /* a * a^(-1) == 1 */
    {
        fp_fe inv, prod;
        shaw_scalar_invert(inv, a);
        shaw_scalar_mul(prod, a, inv);
        unsigned char out[32];
        shaw_scalar_to_bytes(out, prod);
        check_bytes("shaw scalar a * a^-1 == 1", one_bytes, out, 32);
    }

    /* Distributivity */
    {
        fp_fe one, b_plus_one, lhs, ab, a_one, rhs;
        shaw_scalar_one(one);
        shaw_scalar_add(b_plus_one, b, one);
        shaw_scalar_mul(lhs, a, b_plus_one);
        shaw_scalar_mul(ab, a, b);
        shaw_scalar_mul(a_one, a, one);
        shaw_scalar_add(rhs, ab, a_one);
        unsigned char lhs_bytes[32], rhs_bytes[32];
        shaw_scalar_to_bytes(lhs_bytes, lhs);
        shaw_scalar_to_bytes(rhs_bytes, rhs);
        check_bytes("shaw scalar distributivity", lhs_bytes, rhs_bytes, 32);
    }

    /* Serialization round-trip */
    {
        unsigned char buf[32];
        shaw_scalar_to_bytes(buf, a);
        fp_fe a2;
        shaw_scalar_from_bytes(a2, buf);
        unsigned char buf2[32];
        shaw_scalar_to_bytes(buf2, a2);
        check_bytes("shaw scalar round-trip", buf, buf2, 32);
    }

    /* is_zero */
    {
        fp_fe z;
        shaw_scalar_zero(z);
        check_int("shaw scalar is_zero(0)", 1, shaw_scalar_is_zero(z));
        check_int("shaw scalar !is_zero(a)", 0, shaw_scalar_is_zero(a));
    }

    /* Wide reduction */
    {
        unsigned char wide_zero[64] = {};
        fp_fe result;
        shaw_scalar_reduce_wide(result, wide_zero);
        unsigned char out[32];
        shaw_scalar_to_bytes(out, result);
        check_bytes("shaw scalar reduce_wide(0) == 0", zero_bytes, out, 32);

        unsigned char wide_one[64] = {0x01};
        shaw_scalar_reduce_wide(result, wide_one);
        shaw_scalar_to_bytes(out, result);
        check_bytes("shaw scalar reduce_wide(lo=1,hi=0) == 1", one_bytes, out, 32);
    }

    /* muladd: a*b + 1 == a*b + 1 (computed two ways) */
    {
        fp_fe one, ab, ab_plus_one, muladd_result;
        shaw_scalar_one(one);
        shaw_scalar_mul(ab, a, b);
        shaw_scalar_add(ab_plus_one, ab, one);
        shaw_scalar_muladd(muladd_result, a, b, one);
        unsigned char out1[32], out2[32];
        shaw_scalar_to_bytes(out1, ab_plus_one);
        shaw_scalar_to_bytes(out2, muladd_result);
        check_bytes("shaw scalar muladd(a,b,1) == a*b+1", out1, out2, 32);
    }

    /* sq: a^2 == a*a */
    {
        fp_fe sq_result, mul_result;
        shaw_scalar_sq(sq_result, a);
        shaw_scalar_mul(mul_result, a, a);
        unsigned char out1[32], out2[32];
        shaw_scalar_to_bytes(out1, sq_result);
        shaw_scalar_to_bytes(out2, mul_result);
        check_bytes("shaw scalar sq(a) == a*a", out1, out2, 32);
    }
}
