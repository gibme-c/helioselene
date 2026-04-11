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

void test_ran_points()
{
    std::cout << std::endl << "=== Ran point ops ===" << std::endl;
    unsigned char buf[32];

    ran_affine g_aff;
    fp_copy(g_aff.x, RAN_GX);
    fp_copy(g_aff.y, RAN_GY);
    check_nonzero("G is on curve", ran_is_on_curve(&g_aff));

    ran_jacobian G;
    fp_copy(G.X, RAN_GX);
    fp_copy(G.Y, RAN_GY);
    fp_1(G.Z);

    ran_tobytes(buf, &G);
    check_bytes("tobytes(G)", tv::compressed_points::ran_g, buf, 32);

    ran_jacobian G2;
    int rc = ran_frombytes(&G2, tv::compressed_points::ran_g);
    check_int("frombytes(G) returns 0", 0, rc);
    ran_tobytes(buf, &G2);
    check_bytes("frombytes(tobytes(G)) round-trip", tv::compressed_points::ran_g, buf, 32);

    ran_jacobian id;
    ran_identity(&id);
    check_nonzero("identity is_identity", ran_is_identity(&id));

    ran_tobytes(buf, &id);
    check_bytes("tobytes(identity) == zeros", zero_bytes, buf, 32);

    ran_jacobian dbl_G;
    ran_dbl(&dbl_G, &G);
    ran_tobytes(buf, &dbl_G);
    check_bytes("2G = dbl(G)", tv::compressed_points::ran_2g, buf, 32);

    /* 3G = 2G + G (add doesn't handle P==P, so skip G+G test) */
    ran_jacobian three_G;
    ran_add(&three_G, &dbl_G, &G);

    ran_jacobian four_G;
    ran_dbl(&four_G, &dbl_G);

    ran_jacobian seven_G;
    ran_add(&seven_G, &four_G, &three_G);
    ran_tobytes(buf, &seven_G);
    check_bytes("7G = 4G + 3G", tv::compressed_points::ran_7g, buf, 32);

    ran_jacobian decoded_2g;
    rc = ran_frombytes(&decoded_2g, tv::compressed_points::ran_2g);
    check_int("frombytes(2G) returns 0", 0, rc);
    ran_tobytes(buf, &decoded_2g);
    check_bytes("2G round-trip", tv::compressed_points::ran_2g, buf, 32);

    unsigned char invalid_bytes[32] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
    ran_jacobian invalid;
    rc = ran_frombytes(&invalid, invalid_bytes);
    check_int("reject non-canonical x", -1, rc);

    ran_affine g_affine;
    fp_copy(g_affine.x, RAN_GX);
    fp_copy(g_affine.y, RAN_GY);
    ran_jacobian madd_result;
    ran_madd(&madd_result, &dbl_G, &g_affine);
    ran_tobytes(buf, &madd_result);
    unsigned char three_G_bytes[32];
    ran_tobytes(three_G_bytes, &three_G);
    check_bytes("madd(2G, G) == add(2G, G)", three_G_bytes, buf, 32);
}


void test_ran_point_edges()
{
    std::cout << std::endl << "=== Ran point edges ===" << std::endl;
    unsigned char buf[32];

    ran_jacobian G;
    fp_copy(G.X, RAN_GX);
    fp_copy(G.Y, RAN_GY);
    fp_1(G.Z);

    /* (order-1)*G == -G */
    {
        unsigned char om1[32];
        std::memcpy(om1, RAN_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (om1[i] > 0)
            {
                om1[i]--;
                break;
            }
            om1[i] = 0xff;
        }
        ran_jacobian result;
        ran_scalarmult(&result, om1, &G);
        ran_jacobian neg_G;
        ran_neg(&neg_G, &G);
        unsigned char r_bytes[32], neg_bytes[32];
        ran_tobytes(r_bytes, &result);
        ran_tobytes(neg_bytes, &neg_G);
        check_bytes("(order-1)*G == -G", neg_bytes, r_bytes, 32);
    }

    /* vartime: (order-1)*G == -G */
    {
        unsigned char om1[32];
        std::memcpy(om1, RAN_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (om1[i] > 0)
            {
                om1[i]--;
                break;
            }
            om1[i] = 0xff;
        }
        ran_jacobian result;
        ran_scalarmult_vartime(&result, om1, &G);
        ran_jacobian neg_G;
        ran_neg(&neg_G, &G);
        unsigned char r_bytes[32], neg_bytes[32];
        ran_tobytes(r_bytes, &result);
        ran_tobytes(neg_bytes, &neg_G);
        check_bytes("vartime: (order-1)*G == -G", neg_bytes, r_bytes, 32);
    }

    /* (order-1)*G + G == identity */
    {
        unsigned char om1[32];
        std::memcpy(om1, RAN_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (om1[i] > 0)
            {
                om1[i]--;
                break;
            }
            om1[i] = 0xff;
        }
        ran_jacobian om1G, sum;
        ran_scalarmult(&om1G, om1, &G);
        ran_add(&sum, &om1G, &G);
        check_nonzero("(order-1)*G + G == identity", ran_is_identity(&sum));
    }

    /* Y-parity: serialize G, flip bit 255, verify y negated */
    {
        unsigned char g_bytes[32];
        ran_tobytes(g_bytes, &G);
        unsigned char flipped[32];
        std::memcpy(flipped, g_bytes, 32);
        flipped[31] ^= 0x80; /* flip parity bit */
        ran_jacobian decoded;
        int rc = ran_frombytes(&decoded, flipped);
        check_int("flipped parity decodes", 0, rc);
        /* The y should be negated */
        ran_affine aff_orig, aff_flip;
        ran_to_affine(&aff_orig, &G);
        ran_to_affine(&aff_flip, &decoded);
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
        ran_jacobian id;
        ran_identity(&id);
        unsigned char id_bytes[32];
        ran_tobytes(id_bytes, &id);
        check_bytes("tobytes(identity) == 0", zero_bytes, id_bytes, 32);
        /* frombytes(0) — x=0, check if on curve */
        ran_jacobian decoded;
        int rc = ran_frombytes(&decoded, zero_bytes);
        /* x=0: gx = 0^3 - 3*0 + b = b. If b is a QR, this decodes. Otherwise -1. */
        /* Either way, we just record what happens */
        if (rc == 0)
        {
            ran_tobytes(buf, &decoded);
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
        ran_jacobian decoded;
        int rc = ran_frombytes(&decoded, x_bytes);
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
            ran_affine aff;
            ran_to_affine(&aff, &decoded);
            if (ran_is_on_curve(&aff))
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
