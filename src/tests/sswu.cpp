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

void test_ran_sswu()
{
    std::cout << std::endl << "=== Ran SSWU ===" << std::endl;
    unsigned char buf[32];

    /* Known test vectors */
    ran_jacobian result;
    ran_map_to_curve(&result, one_bytes);
    ran_tobytes(buf, &result);
    check_bytes("sswu(1)", tv::sswu_vectors::ran_u1, buf, 32);

    unsigned char two_bytes[32] = {0x02};
    ran_map_to_curve(&result, two_bytes);
    ran_tobytes(buf, &result);
    check_bytes("sswu(2)", tv::sswu_vectors::ran_u2, buf, 32);

    unsigned char u42_bytes[32] = {0x2a};
    ran_map_to_curve(&result, u42_bytes);
    ran_tobytes(buf, &result);
    check_bytes("sswu(42)", tv::sswu_vectors::ran_u42, buf, 32);

    /* Deterministic: same input → same output */
    ran_jacobian result2;
    ran_map_to_curve(&result2, one_bytes);
    ran_tobytes(buf, &result2);
    check_bytes("sswu(1) deterministic", tv::sswu_vectors::ran_u1, buf, 32);

    /* Output is on curve */
    ran_affine aff;
    ran_to_affine(&aff, &result);
    check_nonzero("sswu(1) on curve", ran_is_on_curve(&aff));

    /* map_to_curve2(u0, u1) == map_to_curve(u0) + map_to_curve(u1) */
    ran_jacobian p0, p1, sum_direct, sum_combined;
    ran_map_to_curve(&p0, one_bytes);
    ran_map_to_curve(&p1, two_bytes);
    ran_add(&sum_direct, &p0, &p1);
    ran_tobytes(buf, &sum_direct);

    ran_map_to_curve2(&sum_combined, one_bytes, two_bytes);
    unsigned char buf2[32];
    ran_tobytes(buf2, &sum_combined);
    check_bytes("map_to_curve2(1,2) == sswu(1)+sswu(2)", buf, buf2, 32);

    /* sswu(0) produces a valid point */
    ran_map_to_curve(&result, zero_bytes);
    ran_to_affine(&aff, &result);
    check_nonzero("sswu(0) on curve", ran_is_on_curve(&aff));
}


void test_shaw_sswu()
{
    std::cout << std::endl << "=== Shaw SSWU ===" << std::endl;
    unsigned char buf[32];

    /* Known test vectors */
    shaw_jacobian result;
    shaw_map_to_curve(&result, one_bytes);
    shaw_tobytes(buf, &result);
    check_bytes("sswu(1)", tv::sswu_vectors::shaw_u1, buf, 32);

    unsigned char two_bytes[32] = {0x02};
    shaw_map_to_curve(&result, two_bytes);
    shaw_tobytes(buf, &result);
    check_bytes("sswu(2)", tv::sswu_vectors::shaw_u2, buf, 32);

    unsigned char u42_bytes[32] = {0x2a};
    shaw_map_to_curve(&result, u42_bytes);
    shaw_tobytes(buf, &result);
    check_bytes("sswu(42)", tv::sswu_vectors::shaw_u42, buf, 32);

    /* Deterministic: same input → same output */
    shaw_jacobian result2;
    shaw_map_to_curve(&result2, one_bytes);
    shaw_tobytes(buf, &result2);
    check_bytes("sswu(1) deterministic", tv::sswu_vectors::shaw_u1, buf, 32);

    /* Output is on curve */
    shaw_affine aff;
    shaw_to_affine(&aff, &result);
    check_nonzero("sswu(1) on curve", shaw_is_on_curve(&aff));

    /* map_to_curve2(u0, u1) == map_to_curve(u0) + map_to_curve(u1) */
    shaw_jacobian p0, p1, sum_direct, sum_combined;
    shaw_map_to_curve(&p0, one_bytes);
    shaw_map_to_curve(&p1, two_bytes);
    shaw_add(&sum_direct, &p0, &p1);
    shaw_tobytes(buf, &sum_direct);

    shaw_map_to_curve2(&sum_combined, one_bytes, two_bytes);
    unsigned char buf2[32];
    shaw_tobytes(buf2, &sum_combined);
    check_bytes("map_to_curve2(1,2) == sswu(1)+sswu(2)", buf, buf2, 32);

    /* sswu(0) produces a valid point */
    shaw_map_to_curve(&result, zero_bytes);
    shaw_to_affine(&aff, &result);
    check_nonzero("sswu(0) on curve", shaw_is_on_curve(&aff));
}
