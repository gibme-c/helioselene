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

void test_ran_batch_affine()
{
    std::cout << std::endl << "=== Ran batch affine ===" << std::endl;

    ran_jacobian G;
    fp_copy(G.X, RAN_GX);
    fp_copy(G.Y, RAN_GY);
    fp_1(G.Z);

    /* n=1: batch matches single to_affine */
    {
        ran_affine batch_out[1], single_out;
        ran_batch_to_affine(batch_out, &G, 1);
        ran_to_affine(&single_out, &G);
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
        ran_jacobian points[4];
        ran_copy(&points[0], &G);
        ran_dbl(&points[1], &G);
        ran_add(&points[2], &points[1], &G);
        ran_dbl(&points[3], &points[1]);

        ran_affine batch_out[4], single_out[4];
        ran_batch_to_affine(batch_out, points, 4);
        for (int i = 0; i < 4; i++)
            ran_to_affine(&single_out[i], &points[i]);

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
        ran_jacobian points[2];
        ran_copy(&points[0], &G);
        ran_identity(&points[1]);
        ran_affine batch_out[2];
        ran_batch_to_affine(batch_out, points, 2);
        unsigned char zx[32];
        fp_tobytes(zx, batch_out[1].x);
        check_bytes("batch identity x == 0", zero_bytes, zx, 32);
    }
}


void test_shaw_batch_affine()
{
    std::cout << std::endl << "=== Shaw batch affine ===" << std::endl;

    shaw_jacobian G;
    fq_copy(G.X, SHAW_GX);
    fq_copy(G.Y, SHAW_GY);
    fq_1(G.Z);

    /* n=4 */
    {
        shaw_jacobian points[4];
        shaw_copy(&points[0], &G);
        shaw_dbl(&points[1], &G);
        shaw_add(&points[2], &points[1], &G);
        shaw_dbl(&points[3], &points[1]);

        shaw_affine batch_out[4], single_out[4];
        shaw_batch_to_affine(batch_out, points, 4);
        for (int i = 0; i < 4; i++)
            shaw_to_affine(&single_out[i], &points[i]);

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


void test_batch_affine_extended()
{
    std::cout << std::endl << "=== Batch affine extended ===" << std::endl;

    /* Shaw n=1 (match Ran coverage) */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_affine batch_out[1], single_out;
        shaw_batch_to_affine(batch_out, &G, 1);
        shaw_to_affine(&single_out, &G);
        unsigned char bx[32], sx[32], by[32], sy[32];
        fq_tobytes(bx, batch_out[0].x);
        fq_tobytes(sx, single_out.x);
        check_bytes("shaw batch n=1 x", sx, bx, 32);
        fq_tobytes(by, batch_out[0].y);
        fq_tobytes(sy, single_out.y);
        check_bytes("shaw batch n=1 y", sy, by, 32);
    }

    /* Ran n=4: verify y-coordinates too */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_jacobian points[4];
        ran_copy(&points[0], &G);
        ran_dbl(&points[1], &G);
        ran_add(&points[2], &points[1], &G);
        ran_dbl(&points[3], &points[1]);

        ran_affine batch_out[4], single_out[4];
        ran_batch_to_affine(batch_out, points, 4);
        for (int i = 0; i < 4; i++)
            ran_to_affine(&single_out[i], &points[i]);

        for (int i = 0; i < 4; i++)
        {
            unsigned char by_arr[32], sy_arr[32];
            fp_tobytes(by_arr, batch_out[i].y);
            fp_tobytes(sy_arr, single_out[i].y);
            std::string name = "ran batch n=4 point " + std::to_string(i) + " y";
            check_bytes(name.c_str(), sy_arr, by_arr, 32);
        }
    }

    /* Shaw n=4: verify y-coordinates */
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_jacobian points[4];
        shaw_copy(&points[0], &G);
        shaw_dbl(&points[1], &G);
        shaw_add(&points[2], &points[1], &G);
        shaw_dbl(&points[3], &points[1]);

        shaw_affine batch_out[4], single_out[4];
        shaw_batch_to_affine(batch_out, points, 4);
        for (int i = 0; i < 4; i++)
            shaw_to_affine(&single_out[i], &points[i]);

        for (int i = 0; i < 4; i++)
        {
            unsigned char by_arr[32], sy_arr[32];
            fq_tobytes(by_arr, batch_out[i].y);
            fq_tobytes(sy_arr, single_out[i].y);
            std::string name = "shaw batch n=4 point " + std::to_string(i) + " y";
            check_bytes(name.c_str(), sy_arr, by_arr, 32);
        }
    }

    /* Ran n=16 stress test */
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_jacobian points[16];
        ran_copy(&points[0], &G); /* 1G */
        ran_dbl(&points[1], &G); /* 2G */
        ran_add(&points[2], &points[1], &G); /* 3G */
        ran_dbl(&points[3], &points[1]); /* 4G */
        ran_add(&points[4], &points[3], &G); /* 5G */
        ran_add(&points[5], &points[4], &G); /* 6G */
        /* Use scalarmult for the rest to avoid add(P,P) */
        for (int i = 6; i < 16; i++)
        {
            unsigned char sc[32] = {};
            sc[0] = (unsigned char)(i + 1);
            ran_scalarmult_vartime(&points[i], sc, &G);
        }

        ran_affine batch_out[16], single_out[16];
        ran_batch_to_affine(batch_out, points, 16);
        for (int i = 0; i < 16; i++)
            ran_to_affine(&single_out[i], &points[i]);

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
            std::cout << "  PASS: ran batch n=16 all x,y match" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: ran batch n=16 mismatch" << std::endl;
        }
    }
}


void test_batch_invert()
{
    std::cout << std::endl << "=== Batch field inversion ===" << std::endl;

    /* Fp: batch invert 4 elements, verify each out[i] * in[i] == 1 */
    {
        fp_fe elems[4], invs[4];
        /* Use small known values: 2, 3, 5, 7 */
        unsigned char v2[32] = {0x02}, v3[32] = {0x03}, v5[32] = {0x05}, v7[32] = {0x07};
        fp_frombytes(elems[0], v2);
        fp_frombytes(elems[1], v3);
        fp_frombytes(elems[2], v5);
        fp_frombytes(elems[3], v7);

        fp_batch_invert(invs, elems, 4);

        bool all_one = true;
        for (int i = 0; i < 4; i++)
        {
            fp_fe prod;
            fp_mul(prod, elems[i], invs[i]);
            unsigned char out[32], one[32] = {};
            one[0] = 1;
            fp_tobytes(out, prod);
            if (std::memcmp(out, one, 32) != 0)
                all_one = false;
        }
        ++tests_run;
        if (all_one)
        {
            ++tests_passed;
            std::cout << "  PASS: fp batch invert 4 elements" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: fp batch invert 4 elements" << std::endl;
        }
    }

    /* Fq: batch invert 4 elements, verify each out[i] * in[i] == 1 */
    {
        fq_fe elems[4], invs[4];
        unsigned char v2[32] = {0x02}, v3[32] = {0x03}, v5[32] = {0x05}, v7[32] = {0x07};
        fq_frombytes(elems[0], v2);
        fq_frombytes(elems[1], v3);
        fq_frombytes(elems[2], v5);
        fq_frombytes(elems[3], v7);

        fq_batch_invert(invs, elems, 4);

        bool all_one = true;
        for (int i = 0; i < 4; i++)
        {
            fq_fe prod;
            fq_mul(prod, elems[i], invs[i]);
            unsigned char out[32], one[32] = {};
            one[0] = 1;
            fq_tobytes(out, prod);
            if (std::memcmp(out, one, 32) != 0)
                all_one = false;
        }
        ++tests_run;
        if (all_one)
        {
            ++tests_passed;
            std::cout << "  PASS: fq batch invert 4 elements" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: fq batch invert 4 elements" << std::endl;
        }
    }

    /* Fp: batch with zero element in position 2 */
    {
        fp_fe elems[4], invs[4];
        unsigned char v2[32] = {0x02}, v3[32] = {0x03}, v0[32] = {}, v7[32] = {0x07};
        fp_frombytes(elems[0], v2);
        fp_frombytes(elems[1], v3);
        fp_frombytes(elems[2], v0); /* zero */
        fp_frombytes(elems[3], v7);

        fp_batch_invert(invs, elems, 4);

        /* Check zero maps to zero */
        unsigned char zero_out[32], zero_exp[32] = {};
        fp_tobytes(zero_out, invs[2]);
        bool zero_ok = (std::memcmp(zero_out, zero_exp, 32) == 0);

        /* Check nonzero elements still correct */
        bool nonzero_ok = true;
        for (int i : {0, 1, 3})
        {
            fp_fe prod;
            fp_mul(prod, elems[i], invs[i]);
            unsigned char out[32], one[32] = {};
            one[0] = 1;
            fp_tobytes(out, prod);
            if (std::memcmp(out, one, 32) != 0)
                nonzero_ok = false;
        }
        ++tests_run;
        if (zero_ok && nonzero_ok)
        {
            ++tests_passed;
            std::cout << "  PASS: fp batch invert with zero element" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: fp batch invert with zero element" << std::endl;
        }
    }

    /* Fq: batch with zero element in position 2 */
    {
        fq_fe elems[4], invs[4];
        unsigned char v2[32] = {0x02}, v3[32] = {0x03}, v0[32] = {}, v7[32] = {0x07};
        fq_frombytes(elems[0], v2);
        fq_frombytes(elems[1], v3);
        fq_frombytes(elems[2], v0); /* zero */
        fq_frombytes(elems[3], v7);

        fq_batch_invert(invs, elems, 4);

        unsigned char zero_out[32], zero_exp[32] = {};
        fq_tobytes(zero_out, invs[2]);
        bool zero_ok = (std::memcmp(zero_out, zero_exp, 32) == 0);

        bool nonzero_ok = true;
        for (int i : {0, 1, 3})
        {
            fq_fe prod;
            fq_mul(prod, elems[i], invs[i]);
            unsigned char out[32], one[32] = {};
            one[0] = 1;
            fq_tobytes(out, prod);
            if (std::memcmp(out, one, 32) != 0)
                nonzero_ok = false;
        }
        ++tests_run;
        if (zero_ok && nonzero_ok)
        {
            ++tests_passed;
            std::cout << "  PASS: fq batch invert with zero element" << std::endl;
        }
        else
        {
            ++tests_failed;
            std::cout << "  FAIL: fq batch invert with zero element" << std::endl;
        }
    }
}
