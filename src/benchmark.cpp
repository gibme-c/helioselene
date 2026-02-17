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
#include "helioselene_benchmark.h"

#include <iostream>
#include <vector>

static const unsigned char test_a_bytes[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
                                               0xca, 0xef, 0xbe, 0xad, 0xde, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const unsigned char test_b_bytes[32] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x0d, 0xf0, 0xad,
                                               0xba, 0xce, 0xfa, 0xed, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char test_scalar[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
                                              0xca, 0xef, 0xbe, 0xad, 0xde, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                              0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};

int main()
{
    auto state = benchmark_setup();

    fp_fe fp_a, fp_b, fp_c;
    fp_frombytes(fp_a, test_a_bytes);
    fp_frombytes(fp_b, test_b_bytes);

    fq_fe fq_a, fq_b, fq_c;
    fq_frombytes(fq_a, test_a_bytes);
    fq_frombytes(fq_b, test_b_bytes);

    /* Set up point data */
    helios_jacobian h_G, h_result;
    fp_copy(h_G.X, HELIOS_GX);
    fp_copy(h_G.Y, HELIOS_GY);
    fp_1(h_G.Z);

    helios_jacobian h_2G;
    helios_dbl(&h_2G, &h_G);

    helios_affine h_G_aff;
    fp_copy(h_G_aff.x, HELIOS_GX);
    fp_copy(h_G_aff.y, HELIOS_GY);

    selene_jacobian s_G, s_result;
    fq_copy(s_G.X, SELENE_GX);
    fq_copy(s_G.Y, SELENE_GY);
    fq_1(s_G.Z);

    selene_jacobian s_2G;
    selene_dbl(&s_2G, &s_G);

    selene_affine s_G_aff;
    fq_copy(s_G_aff.x, SELENE_GX);
    fq_copy(s_G_aff.y, SELENE_GY);

    unsigned char point_bytes[32];

    std::cout << std::endl;
    benchmark_header();
    std::cout << std::endl;

    std::cout << "--- F_p (2^255 - 19) ---" << std::endl;

    benchmark_long(
        [&]()
        {
            fp_add(fp_c, fp_a, fp_b);
            benchmark_do_not_optimize(fp_c);
        },
        "fp_add");

    benchmark_long(
        [&]()
        {
            fp_sub(fp_c, fp_a, fp_b);
            benchmark_do_not_optimize(fp_c);
        },
        "fp_sub");

    benchmark_long(
        [&]()
        {
            fp_mul(fp_c, fp_a, fp_b);
            benchmark_do_not_optimize(fp_c);
        },
        "fp_mul");

    benchmark_long(
        [&]()
        {
            fp_sq(fp_c, fp_a);
            benchmark_do_not_optimize(fp_c);
        },
        "fp_sq");

    benchmark(
        [&]()
        {
            fp_invert(fp_c, fp_a);
            benchmark_do_not_optimize(fp_c);
        },
        "fp_invert");

    benchmark(
        [&]()
        {
            fp_sqrt(fp_c, fp_a);
            benchmark_do_not_optimize(fp_c);
        },
        "fp_sqrt");

    std::cout << std::endl;
    std::cout << "--- F_q (2^255 - gamma) ---" << std::endl;

    benchmark_long(
        [&]()
        {
            fq_add(fq_c, fq_a, fq_b);
            benchmark_do_not_optimize(fq_c);
        },
        "fq_add");

    benchmark_long(
        [&]()
        {
            fq_sub(fq_c, fq_a, fq_b);
            benchmark_do_not_optimize(fq_c);
        },
        "fq_sub");

    benchmark_long(
        [&]()
        {
            fq_mul(fq_c, fq_a, fq_b);
            benchmark_do_not_optimize(fq_c);
        },
        "fq_mul");

    benchmark_long(
        [&]()
        {
            fq_sq(fq_c, fq_a);
            benchmark_do_not_optimize(fq_c);
        },
        "fq_sq");

    benchmark(
        [&]()
        {
            fq_invert(fq_c, fq_a);
            benchmark_do_not_optimize(fq_c);
        },
        "fq_invert");

    benchmark(
        [&]()
        {
            fq_sqrt(fq_c, fq_a);
            benchmark_do_not_optimize(fq_c);
        },
        "fq_sqrt");

    std::cout << std::endl;
    std::cout << "--- Helios (over F_p) ---" << std::endl;

    benchmark_long(
        [&]()
        {
            helios_dbl(&h_result, &h_G);
            benchmark_do_not_optimize(h_result);
        },
        "helios_dbl");

    benchmark_long(
        [&]()
        {
            helios_madd(&h_result, &h_2G, &h_G_aff);
            benchmark_do_not_optimize(h_result);
        },
        "helios_madd");

    benchmark_long(
        [&]()
        {
            helios_add(&h_result, &h_2G, &h_G);
            benchmark_do_not_optimize(h_result);
        },
        "helios_add");

    benchmark(
        [&]()
        {
            helios_tobytes(point_bytes, &h_G);
            benchmark_do_not_optimize(point_bytes);
        },
        "helios_tobytes");

    benchmark(
        [&]()
        {
            helios_frombytes(&h_result, point_bytes);
            benchmark_do_not_optimize(h_result);
        },
        "helios_frombytes");

    benchmark(
        [&]()
        {
            helios_scalarmult(&h_result, test_scalar, &h_G);
            benchmark_do_not_optimize(h_result);
        },
        "helios_scalarmult");

    benchmark(
        [&]()
        {
            helios_scalarmult_vartime(&h_result, test_scalar, &h_G);
            benchmark_do_not_optimize(h_result);
        },
        "helios_scalarmult_vt");

    std::cout << std::endl;
    std::cout << "--- Selene (over F_q) ---" << std::endl;

    benchmark_long(
        [&]()
        {
            selene_dbl(&s_result, &s_G);
            benchmark_do_not_optimize(s_result);
        },
        "selene_dbl");

    benchmark_long(
        [&]()
        {
            selene_madd(&s_result, &s_2G, &s_G_aff);
            benchmark_do_not_optimize(s_result);
        },
        "selene_madd");

    benchmark_long(
        [&]()
        {
            selene_add(&s_result, &s_2G, &s_G);
            benchmark_do_not_optimize(s_result);
        },
        "selene_add");

    benchmark(
        [&]()
        {
            selene_tobytes(point_bytes, &s_G);
            benchmark_do_not_optimize(point_bytes);
        },
        "selene_tobytes");

    benchmark(
        [&]()
        {
            selene_frombytes(&s_result, point_bytes);
            benchmark_do_not_optimize(s_result);
        },
        "selene_frombytes");

    benchmark(
        [&]()
        {
            selene_scalarmult(&s_result, test_scalar, &s_G);
            benchmark_do_not_optimize(s_result);
        },
        "selene_scalarmult");

    benchmark(
        [&]()
        {
            selene_scalarmult_vartime(&s_result, test_scalar, &s_G);
            benchmark_do_not_optimize(s_result);
        },
        "selene_scalarmult_vt");

    std::cout << std::endl;
    std::cout << "--- Helios MSM ---" << std::endl;

    /* Prepare MSM data */
    auto make_helios_msm_data = [&](size_t n, std::vector<unsigned char> &scalars, std::vector<helios_jacobian> &points)
    {
        scalars.resize(n * 32, 0);
        points.resize(n);
        for (size_t i = 0; i < n; i++)
        {
            scalars[i * 32] = static_cast<unsigned char>((i + 1) & 0xff);
            scalars[i * 32 + 1] = static_cast<unsigned char>(((i + 1) >> 8) & 0xff);
            helios_copy(&points[i], &h_G);
        }
    };

    {
        std::vector<unsigned char> sc;
        std::vector<helios_jacobian> pts;

        make_helios_msm_data(1, sc, pts);
        benchmark(
            [&]()
            {
                helios_msm_vartime(&h_result, sc.data(), pts.data(), 1);
                benchmark_do_not_optimize(h_result);
            },
            "helios_msm n=1");

        make_helios_msm_data(8, sc, pts);
        benchmark(
            [&]()
            {
                helios_msm_vartime(&h_result, sc.data(), pts.data(), 8);
                benchmark_do_not_optimize(h_result);
            },
            "helios_msm n=8");

        make_helios_msm_data(32, sc, pts);
        benchmark(
            [&]()
            {
                helios_msm_vartime(&h_result, sc.data(), pts.data(), 32);
                benchmark_do_not_optimize(h_result);
            },
            "helios_msm n=32");

        make_helios_msm_data(64, sc, pts);
        benchmark(
            [&]()
            {
                helios_msm_vartime(&h_result, sc.data(), pts.data(), 64);
                benchmark_do_not_optimize(h_result);
            },
            "helios_msm n=64");

        make_helios_msm_data(256, sc, pts);
        benchmark(
            [&]()
            {
                helios_msm_vartime(&h_result, sc.data(), pts.data(), 256);
                benchmark_do_not_optimize(h_result);
            },
            "helios_msm n=256");
    }

    std::cout << std::endl;
    std::cout << "--- Selene MSM ---" << std::endl;

    auto make_selene_msm_data = [&](size_t n, std::vector<unsigned char> &scalars, std::vector<selene_jacobian> &points)
    {
        scalars.resize(n * 32, 0);
        points.resize(n);
        for (size_t i = 0; i < n; i++)
        {
            scalars[i * 32] = static_cast<unsigned char>((i + 1) & 0xff);
            scalars[i * 32 + 1] = static_cast<unsigned char>(((i + 1) >> 8) & 0xff);
            selene_copy(&points[i], &s_G);
        }
    };

    {
        std::vector<unsigned char> sc;
        std::vector<selene_jacobian> pts;

        make_selene_msm_data(1, sc, pts);
        benchmark(
            [&]()
            {
                selene_msm_vartime(&s_result, sc.data(), pts.data(), 1);
                benchmark_do_not_optimize(s_result);
            },
            "selene_msm n=1");

        make_selene_msm_data(8, sc, pts);
        benchmark(
            [&]()
            {
                selene_msm_vartime(&s_result, sc.data(), pts.data(), 8);
                benchmark_do_not_optimize(s_result);
            },
            "selene_msm n=8");

        make_selene_msm_data(32, sc, pts);
        benchmark(
            [&]()
            {
                selene_msm_vartime(&s_result, sc.data(), pts.data(), 32);
                benchmark_do_not_optimize(s_result);
            },
            "selene_msm n=32");

        make_selene_msm_data(64, sc, pts);
        benchmark(
            [&]()
            {
                selene_msm_vartime(&s_result, sc.data(), pts.data(), 64);
                benchmark_do_not_optimize(s_result);
            },
            "selene_msm n=64");

        make_selene_msm_data(256, sc, pts);
        benchmark(
            [&]()
            {
                selene_msm_vartime(&s_result, sc.data(), pts.data(), 256);
                benchmark_do_not_optimize(s_result);
            },
            "selene_msm n=256");
    }

    std::cout << std::endl;
    std::cout << "--- SSWU map-to-curve ---" << std::endl;

    unsigned char sswu_input[32] = {0x2a};

    benchmark(
        [&]()
        {
            helios_map_to_curve(&h_result, sswu_input);
            benchmark_do_not_optimize(h_result);
        },
        "helios_map_to_curve");

    benchmark(
        [&]()
        {
            selene_map_to_curve(&s_result, sswu_input);
            benchmark_do_not_optimize(s_result);
        },
        "selene_map_to_curve");

    unsigned char sswu_u0[32] = {0x01};
    unsigned char sswu_u1[32] = {0x02};

    benchmark(
        [&]()
        {
            helios_map_to_curve2(&h_result, sswu_u0, sswu_u1);
            benchmark_do_not_optimize(h_result);
        },
        "helios_map_to_curve2");

    benchmark(
        [&]()
        {
            selene_map_to_curve2(&s_result, sswu_u0, sswu_u1);
            benchmark_do_not_optimize(s_result);
        },
        "selene_map_to_curve2");

    std::cout << "--- Batch affine ---" << std::endl;

    /* Prepare batch data */
    auto make_helios_batch_data = [&](size_t n, std::vector<helios_jacobian> &pts)
    {
        pts.resize(n);
        helios_copy(&pts[0], &h_G);
        for (size_t i = 1; i < n; i++)
            helios_dbl(&pts[i], &pts[i - 1]);
    };

    {
        std::vector<helios_jacobian> pts;
        std::vector<helios_affine> out;

        make_helios_batch_data(16, pts);
        out.resize(16);
        benchmark(
            [&]()
            {
                helios_batch_to_affine(out.data(), pts.data(), 16);
                benchmark_do_not_optimize(out[0]);
            },
            "helios_batch_affine n=16");

        make_helios_batch_data(64, pts);
        out.resize(64);
        benchmark(
            [&]()
            {
                helios_batch_to_affine(out.data(), pts.data(), 64);
                benchmark_do_not_optimize(out[0]);
            },
            "helios_batch_affine n=64");

        make_helios_batch_data(256, pts);
        out.resize(256);
        benchmark(
            [&]()
            {
                helios_batch_to_affine(out.data(), pts.data(), 256);
                benchmark_do_not_optimize(out[0]);
            },
            "helios_batch_affine n=256");
    }

    std::cout << std::endl;
    std::cout << "--- Pedersen commit ---" << std::endl;

    {
        /* Pedersen with n generators */
        auto bench_helios_pedersen = [&](size_t n, const char *name)
        {
            unsigned char r[32] = {0x03};
            std::vector<unsigned char> vals(n * 32, 0);
            std::vector<helios_jacobian> gens(n);
            for (size_t i = 0; i < n; i++)
            {
                vals[i * 32] = static_cast<unsigned char>((i + 1) & 0xff);
                helios_copy(&gens[i], &h_G);
            }
            benchmark(
                [&]()
                {
                    helios_pedersen_commit(&h_result, r, &h_2G, vals.data(), gens.data(), n);
                    benchmark_do_not_optimize(h_result);
                },
                name);
        };

        bench_helios_pedersen(16, "helios_pedersen n=16");
        bench_helios_pedersen(64, "helios_pedersen n=64");
    }

    std::cout << std::endl;
    std::cout << "--- Polynomial ops ---" << std::endl;

    {
        /* Build two polynomials of degree d for benchmarking multiply */
        auto bench_fp_poly_mul = [&](size_t d, const char *name)
        {
            fp_poly a, b, r;
            a.coeffs.resize(d + 1);
            b.coeffs.resize(d + 1);
            for (size_t i = 0; i <= d; i++)
            {
                fp_frombytes(a.coeffs[i].v, test_a_bytes);
                fp_frombytes(b.coeffs[i].v, test_b_bytes);
            }
            benchmark(
                [&]()
                {
                    fp_poly_mul(&r, &a, &b);
                    benchmark_do_not_optimize(r.coeffs[0]);
                },
                name);
        };

        bench_fp_poly_mul(32, "fp_poly_mul deg=32");
        bench_fp_poly_mul(128, "fp_poly_mul deg=128");
    }

    std::cout << std::endl;
    std::cout << "--- Divisor ---" << std::endl;

    {
        /* Build n affine points for divisor benchmark */
        auto bench_helios_divisor = [&](size_t n, const char *name)
        {
            std::vector<helios_jacobian> jac_pts(n);
            helios_copy(&jac_pts[0], &h_G);
            for (size_t i = 1; i < n; i++)
                helios_dbl(&jac_pts[i], &jac_pts[i - 1]);

            std::vector<helios_affine> aff_pts(n);
            for (size_t i = 0; i < n; i++)
                helios_to_affine(&aff_pts[i], &jac_pts[i]);

            helios_divisor d;
            benchmark(
                [&]()
                {
                    helios_compute_divisor(&d, aff_pts.data(), n);
                    benchmark_do_not_optimize(d.a.coeffs[0]);
                },
                name);
        };

        bench_helios_divisor(4, "helios_divisor n=4");
        bench_helios_divisor(16, "helios_divisor n=16");
    }

    std::cout << std::endl;

    benchmark_teardown(state);

    return 0;
}
