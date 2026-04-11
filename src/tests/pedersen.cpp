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

void test_ran_pedersen()
{
    std::cout << std::endl << "=== Ran Pedersen ===" << std::endl;

    ran_jacobian G;
    fp_copy(G.X, RAN_GX);
    fp_copy(G.Y, RAN_GY);
    fp_1(G.Z);

    /* C = r*H + a*G, where H = 2G, verify == r*2G + a*G */
    ran_jacobian H;
    ran_dbl(&H, &G);

    unsigned char r_scalar[32] = {0x03};
    unsigned char a_scalar[32] = {0x05};

    ran_jacobian commit;
    ran_pedersen_commit(&commit, r_scalar, &H, a_scalar, &G, 1);

    /* Compute expected: 3*2G + 5*G = 6G + 5G = 11G */
    unsigned char eleven_scalar[32] = {0x0b};
    ran_jacobian expected;
    ran_scalarmult_vartime(&expected, eleven_scalar, &G);

    unsigned char commit_bytes[32], expected_bytes[32];
    ran_tobytes(commit_bytes, &commit);
    ran_tobytes(expected_bytes, &expected);
    check_bytes("pedersen(3, 2G, [5], [G]) == 11G", expected_bytes, commit_bytes, 32);

    /* n=0: C = r*H (blinding only) */
    ran_pedersen_commit(&commit, r_scalar, &H, nullptr, nullptr, 0);
    unsigned char three_scalar[32] = {0x03};
    ran_scalarmult_vartime(&expected, three_scalar, &H);
    ran_tobytes(commit_bytes, &commit);
    ran_tobytes(expected_bytes, &expected);
    check_bytes("pedersen n=0: r*H only", expected_bytes, commit_bytes, 32);
}


void test_shaw_pedersen()
{
    std::cout << std::endl << "=== Shaw Pedersen ===" << std::endl;
    (void)zero_bytes; /* suppress unused warnings */

    shaw_jacobian G;
    fq_copy(G.X, SHAW_GX);
    fq_copy(G.Y, SHAW_GY);
    fq_1(G.Z);

    shaw_jacobian H;
    shaw_dbl(&H, &G);

    unsigned char r_scalar[32] = {0x03};
    unsigned char a_scalar[32] = {0x05};

    shaw_jacobian commit;
    shaw_pedersen_commit(&commit, r_scalar, &H, a_scalar, &G, 1);

    unsigned char eleven_scalar[32] = {0x0b};
    shaw_jacobian expected;
    shaw_scalarmult_vartime(&expected, eleven_scalar, &G);

    unsigned char commit_bytes[32], expected_bytes[32];
    shaw_tobytes(commit_bytes, &commit);
    shaw_tobytes(expected_bytes, &expected);
    check_bytes("pedersen(3, 2G, [5], [G]) == 11G", expected_bytes, commit_bytes, 32);
}


void test_pedersen_extended()
{
    std::cout << std::endl << "=== Pedersen extended ===" << std::endl;
    (void)zero_bytes;

    /* Ran: n=3 multiple generators */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_jacobian H, G2, G3;
        ran_dbl(&H, &G); /* H = 2G */
        ran_add(&G2, &H, &G); /* G2 = 3G */
        ran_dbl(&G3, &H); /* G3 = 4G */

        unsigned char r_scalar[32] = {};
        r_scalar[0] = 0x02;
        unsigned char vals[96];
        unsigned char v1[32] = {};
        v1[0] = 0x03;
        unsigned char v2[32] = {};
        v2[0] = 0x05;
        unsigned char v3[32] = {};
        v3[0] = 0x07;
        std::memcpy(vals, v1, 32);
        std::memcpy(vals + 32, v2, 32);
        std::memcpy(vals + 64, v3, 32);

        ran_jacobian gens[3];
        ran_copy(&gens[0], &G);
        ran_copy(&gens[1], &G2);
        ran_copy(&gens[2], &G3);

        ran_jacobian commit;
        ran_pedersen_commit(&commit, r_scalar, &H, vals, gens, 3);

        /* Expected: 2*H + 3*G + 5*G2 + 7*G3 = 2*2G + 3*G + 5*3G + 7*4G = 4G+3G+15G+28G = 50G */
        unsigned char s50[32] = {0x32};
        ran_jacobian expected;
        ran_scalarmult_vartime(&expected, s50, &G);
        unsigned char c_bytes[32], e_bytes[32];
        ran_tobytes(c_bytes, &commit);
        ran_tobytes(e_bytes, &expected);
        check_bytes("ran pedersen n=3", e_bytes, c_bytes, 32);
    }

    /* Shaw: n=0 blinding only */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_jacobian H;
        shaw_dbl(&H, &G);

        unsigned char r_scalar[32] = {0x03};
        shaw_jacobian commit;
        shaw_pedersen_commit(&commit, r_scalar, &H, nullptr, nullptr, 0);

        unsigned char s3[32] = {0x03};
        shaw_jacobian expected;
        shaw_scalarmult_vartime(&expected, s3, &H);
        unsigned char c_bytes[32], e_bytes[32];
        shaw_tobytes(c_bytes, &commit);
        shaw_tobytes(e_bytes, &expected);
        check_bytes("shaw pedersen n=0: r*H", e_bytes, c_bytes, 32);
    }

    /* Ran: zero blinding */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_jacobian H;
        ran_dbl(&H, &G);

        unsigned char s5[32] = {0x05};
        ran_jacobian commit;
        ran_pedersen_commit(&commit, zero_bytes, &H, s5, &G, 1);

        ran_jacobian expected;
        ran_scalarmult_vartime(&expected, s5, &G);
        unsigned char c_bytes[32], e_bytes[32];
        ran_tobytes(c_bytes, &commit);
        ran_tobytes(e_bytes, &expected);
        check_bytes("ran pedersen(0, H, [5], [G]) == 5*G", e_bytes, c_bytes, 32);
    }

    /* Shaw: n=3 multiple generators */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_jacobian H, G2, G3;
        shaw_dbl(&H, &G);
        shaw_add(&G2, &H, &G);
        shaw_dbl(&G3, &H);

        unsigned char r_scalar[32] = {};
        r_scalar[0] = 0x02;
        unsigned char vals[96];
        unsigned char v1[32] = {};
        v1[0] = 0x03;
        unsigned char v2[32] = {};
        v2[0] = 0x05;
        unsigned char v3[32] = {};
        v3[0] = 0x07;
        std::memcpy(vals, v1, 32);
        std::memcpy(vals + 32, v2, 32);
        std::memcpy(vals + 64, v3, 32);

        shaw_jacobian gens[3];
        shaw_copy(&gens[0], &G);
        shaw_copy(&gens[1], &G2);
        shaw_copy(&gens[2], &G3);

        shaw_jacobian commit;
        shaw_pedersen_commit(&commit, r_scalar, &H, vals, gens, 3);

        unsigned char s50[32] = {0x32};
        shaw_jacobian expected;
        shaw_scalarmult_vartime(&expected, s50, &G);
        unsigned char c_bytes[32], e_bytes[32];
        shaw_tobytes(c_bytes, &commit);
        shaw_tobytes(e_bytes, &expected);
        check_bytes("shaw pedersen n=3", e_bytes, c_bytes, 32);
    }
}
