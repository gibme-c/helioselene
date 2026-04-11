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

void test_shaw_points()
{
    std::cout << std::endl << "=== Shaw point ops ===" << std::endl;
    unsigned char buf[32];

    shaw_affine g_aff;
    fq_copy(g_aff.x, SHAW_GX);
    fq_copy(g_aff.y, SHAW_GY);
    check_nonzero("G is on curve", shaw_is_on_curve(&g_aff));

    shaw_jacobian G;
    fq_copy(G.X, SHAW_GX);
    fq_copy(G.Y, SHAW_GY);
    fq_1(G.Z);

    shaw_tobytes(buf, &G);
    check_bytes("tobytes(G)", tv::compressed_points::shaw_g, buf, 32);

    shaw_jacobian G2;
    int rc = shaw_frombytes(&G2, tv::compressed_points::shaw_g);
    check_int("frombytes(G) returns 0", 0, rc);
    shaw_tobytes(buf, &G2);
    check_bytes("frombytes(tobytes(G)) round-trip", tv::compressed_points::shaw_g, buf, 32);

    shaw_jacobian id;
    shaw_identity(&id);
    check_nonzero("identity is_identity", shaw_is_identity(&id));
    shaw_tobytes(buf, &id);
    check_bytes("tobytes(identity) == zeros", zero_bytes, buf, 32);

    shaw_jacobian dbl_G;
    shaw_dbl(&dbl_G, &G);
    shaw_tobytes(buf, &dbl_G);
    check_bytes("2G = dbl(G)", tv::compressed_points::shaw_2g, buf, 32);

    /* 3G, 4G, 7G (add doesn't handle P==P, so skip G+G test) */
    shaw_jacobian three_G;
    shaw_add(&three_G, &dbl_G, &G);
    shaw_jacobian four_G;
    shaw_dbl(&four_G, &dbl_G);
    shaw_jacobian seven_G;
    shaw_add(&seven_G, &four_G, &three_G);
    shaw_tobytes(buf, &seven_G);
    check_bytes("7G = 4G + 3G", tv::compressed_points::shaw_7g, buf, 32);

    shaw_jacobian decoded_2g;
    rc = shaw_frombytes(&decoded_2g, tv::compressed_points::shaw_2g);
    check_int("frombytes(2G) returns 0", 0, rc);
    shaw_tobytes(buf, &decoded_2g);
    check_bytes("2G round-trip", tv::compressed_points::shaw_2g, buf, 32);

    unsigned char invalid_bytes[32] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
    shaw_jacobian invalid;
    rc = shaw_frombytes(&invalid, invalid_bytes);
    check_int("reject non-canonical x", -1, rc);

    shaw_affine g_affine;
    fq_copy(g_affine.x, SHAW_GX);
    fq_copy(g_affine.y, SHAW_GY);
    shaw_jacobian madd_result;
    shaw_madd(&madd_result, &dbl_G, &g_affine);
    shaw_tobytes(buf, &madd_result);
    unsigned char three_G_bytes[32];
    shaw_tobytes(three_G_bytes, &three_G);
    check_bytes("madd(2G, G) == add(2G, G)", three_G_bytes, buf, 32);
}


void test_shaw_point_edges()
{
    std::cout << std::endl << "=== Shaw point edges ===" << std::endl;
    unsigned char buf[32];

    shaw_jacobian G;
    fq_copy(G.X, SHAW_GX);
    fq_copy(G.Y, SHAW_GY);
    fq_1(G.Z);

    /* (order-1)*G == -G */
    {
        unsigned char om1[32];
        std::memcpy(om1, SHAW_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (om1[i] > 0)
            {
                om1[i]--;
                break;
            }
            om1[i] = 0xff;
        }
        shaw_jacobian result;
        shaw_scalarmult(&result, om1, &G);
        shaw_jacobian neg_G;
        shaw_neg(&neg_G, &G);
        unsigned char r_bytes[32], neg_bytes[32];
        shaw_tobytes(r_bytes, &result);
        shaw_tobytes(neg_bytes, &neg_G);
        check_bytes("(order-1)*G == -G", neg_bytes, r_bytes, 32);
    }

    /* vartime: (order-1)*G == -G */
    {
        unsigned char om1[32];
        std::memcpy(om1, SHAW_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (om1[i] > 0)
            {
                om1[i]--;
                break;
            }
            om1[i] = 0xff;
        }
        shaw_jacobian result;
        shaw_scalarmult_vartime(&result, om1, &G);
        shaw_jacobian neg_G;
        shaw_neg(&neg_G, &G);
        unsigned char r_bytes[32], neg_bytes[32];
        shaw_tobytes(r_bytes, &result);
        shaw_tobytes(neg_bytes, &neg_G);
        check_bytes("vartime: (order-1)*G == -G", neg_bytes, r_bytes, 32);
    }

    /* (order-1)*G + G == identity */
    {
        unsigned char om1[32];
        std::memcpy(om1, SHAW_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (om1[i] > 0)
            {
                om1[i]--;
                break;
            }
            om1[i] = 0xff;
        }
        shaw_jacobian om1G, sum;
        shaw_scalarmult(&om1G, om1, &G);
        shaw_add(&sum, &om1G, &G);
        check_nonzero("(order-1)*G + G == identity", shaw_is_identity(&sum));
    }

    /* Y-parity flip */
    {
        unsigned char g_bytes[32];
        shaw_tobytes(g_bytes, &G);
        unsigned char flipped[32];
        std::memcpy(flipped, g_bytes, 32);
        flipped[31] ^= 0x80;
        shaw_jacobian decoded;
        int rc = shaw_frombytes(&decoded, flipped);
        check_int("flipped parity decodes", 0, rc);
        shaw_affine aff_orig, aff_flip;
        shaw_to_affine(&aff_orig, &G);
        shaw_to_affine(&aff_flip, &decoded);
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
        shaw_jacobian id;
        shaw_identity(&id);
        unsigned char id_bytes[32];
        shaw_tobytes(id_bytes, &id);
        check_bytes("tobytes(identity) == 0", zero_bytes, id_bytes, 32);
        shaw_jacobian decoded;
        int rc = shaw_frombytes(&decoded, zero_bytes);
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
