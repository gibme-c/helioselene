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

#include <cstring>
#include <iostream>
#include <string>
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

int main(int argc, char *argv[])
{
    const char *dispatch_label = "baseline (x64/portable)";
    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--autotune") == 0)
        {
            helioselene_autotune();
            dispatch_label = "autotune";
        }
        else if (std::strcmp(argv[i], "--init") == 0)
        {
            helioselene_init();
            dispatch_label = "init (CPUID heuristic)";
        }
        else if (std::strcmp(argv[i], "--all") == 0)
        {
            /* handled below */
        }
        else
        {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            std::cerr << "Usage: helioselene-benchmark [--init|--autotune] [--all]" << std::endl;
            return 1;
        }
    }

    bool bench_all = false;
    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "--all") == 0)
            bench_all = true;
    }

    auto state = benchmark_setup();

    std::cout << "Dispatch: " << dispatch_label << std::endl;
#if HELIOSELENE_SIMD
    std::cout << "CPU features:";
    if (helioselene_has_avx2())
        std::cout << " AVX2";
    if (helioselene_has_avx512f())
        std::cout << " AVX512F";
    if (helioselene_has_avx512ifma())
        std::cout << " AVX512IFMA";
    if (!helioselene_cpu_features())
        std::cout << " (none)";
    std::cout << std::endl;
#endif

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

    if (bench_all)
    {
        std::cout << std::endl;
        std::cout << "--- Helios MSM ---" << std::endl;

        /* Prepare MSM data */
        auto make_helios_msm_data =
            [&](size_t n, std::vector<unsigned char> &scalars, std::vector<helios_jacobian> &points)
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

            const size_t helios_msm_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
            for (size_t sz : helios_msm_sizes)
            {
                make_helios_msm_data(sz, sc, pts);
                std::string label = "helios_msm n=" + std::to_string(sz);
                benchmark(
                    [&]()
                    {
                        helios_msm_vartime(&h_result, sc.data(), pts.data(), sz);
                        benchmark_do_not_optimize(h_result);
                    },
                    label.c_str());
            }
        }

        std::cout << std::endl;
        std::cout << "--- Selene MSM ---" << std::endl;

        auto make_selene_msm_data =
            [&](size_t n, std::vector<unsigned char> &scalars, std::vector<selene_jacobian> &points)
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

            const size_t selene_msm_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
            for (size_t sz : selene_msm_sizes)
            {
                make_selene_msm_data(sz, sc, pts);
                std::string label = "selene_msm n=" + std::to_string(sz);
                benchmark(
                    [&]()
                    {
                        selene_msm_vartime(&s_result, sc.data(), pts.data(), sz);
                        benchmark_do_not_optimize(s_result);
                    },
                    label.c_str());
            }
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

        std::cout << std::endl;
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
            const size_t batch_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
            for (size_t sz : batch_sizes)
            {
                make_helios_batch_data(sz, pts);
                out.resize(sz);
                std::string label = "helios_batch_affine n=" + std::to_string(sz);
                benchmark(
                    [&]()
                    {
                        helios_batch_to_affine(out.data(), pts.data(), sz);
                        benchmark_do_not_optimize(out[0]);
                    },
                    label.c_str());
            }
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

            const size_t pedersen_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
            for (size_t sz : pedersen_sizes)
            {
                std::string label = "helios_pedersen n=" + std::to_string(sz);
                bench_helios_pedersen(sz, label.c_str());
            }
        }

        std::cout << std::endl;
        std::cout << "--- Selene Pedersen commit ---" << std::endl;

        {
            auto bench_selene_pedersen = [&](size_t n, const char *name)
            {
                unsigned char r[32] = {0x03};
                std::vector<unsigned char> vals(n * 32, 0);
                std::vector<selene_jacobian> gens(n);
                for (size_t i = 0; i < n; i++)
                {
                    vals[i * 32] = static_cast<unsigned char>((i + 1) & 0xff);
                    selene_copy(&gens[i], &s_G);
                }
                benchmark(
                    [&]()
                    {
                        selene_pedersen_commit(&s_result, r, &s_2G, vals.data(), gens.data(), n);
                        benchmark_do_not_optimize(s_result);
                    },
                    name);
            };

            const size_t pedersen_sizes[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
            for (size_t sz : pedersen_sizes)
            {
                std::string label = "selene_pedersen n=" + std::to_string(sz);
                bench_selene_pedersen(sz, label.c_str());
            }
        }

        std::cout << std::endl;
        std::cout << "--- Polynomial ops (Karatsuba only, no ECFFT) ---" << std::endl;

        {
            /* Fewer iterations for large-degree polys to keep runtime reasonable */
            auto poly_iters = [](size_t d) -> size_t
            {
                if (d >= 256)
                    return 500;
                if (d >= 128)
                    return 2000;
                return BENCHMARK_PERFORMANCE_ITERATIONS;
            };

            auto poly_warmup = [](size_t d) -> size_t
            {
                if (d >= 256)
                    return 10;
                if (d >= 128)
                    return 50;
                return BENCHMARK_WARMUP_ITERATIONS;
            };

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
                    name,
                    poly_iters(d),
                    poly_warmup(d));
            };

            auto bench_fq_poly_mul = [&](size_t d, const char *name)
            {
                fq_poly a, b, r;
                a.coeffs.resize(d + 1);
                b.coeffs.resize(d + 1);
                for (size_t i = 0; i <= d; i++)
                {
                    fq_frombytes(a.coeffs[i].v, test_a_bytes);
                    fq_frombytes(b.coeffs[i].v, test_b_bytes);
                }
                benchmark(
                    [&]()
                    {
                        fq_poly_mul(&r, &a, &b);
                        benchmark_do_not_optimize(r.coeffs[0]);
                    },
                    name,
                    poly_iters(d),
                    poly_warmup(d));
            };

            bench_fp_poly_mul(32, "fp_poly_mul deg=32");
            bench_fp_poly_mul(64, "fp_poly_mul deg=64");
            bench_fp_poly_mul(128, "fp_poly_mul deg=128");
            bench_fp_poly_mul(256, "fp_poly_mul deg=256");

            std::cout << std::endl;
            bench_fq_poly_mul(32, "fq_poly_mul deg=32");
            bench_fq_poly_mul(64, "fq_poly_mul deg=64");
            bench_fq_poly_mul(128, "fq_poly_mul deg=128");
            bench_fq_poly_mul(256, "fq_poly_mul deg=256");
        }

#ifdef HELIOSELENE_ECFFT
        std::cout << std::endl;
        std::cout << "--- Polynomial ops (with ECFFT) ---" << std::endl;

        {
            ecfft_global_init();

            auto poly_iters = [](size_t d) -> size_t
            {
                if (d >= 256)
                    return 500;
                if (d >= 128)
                    return 2000;
                return BENCHMARK_PERFORMANCE_ITERATIONS;
            };

            auto poly_warmup = [](size_t d) -> size_t
            {
                if (d >= 256)
                    return 10;
                if (d >= 128)
                    return 50;
                return BENCHMARK_WARMUP_ITERATIONS;
            };

            auto bench_fp_poly_mul_ecfft = [&](size_t d, const char *name)
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
                    name,
                    poly_iters(d),
                    poly_warmup(d));
            };

            auto bench_fq_poly_mul_ecfft = [&](size_t d, const char *name)
            {
                fq_poly a, b, r;
                a.coeffs.resize(d + 1);
                b.coeffs.resize(d + 1);
                for (size_t i = 0; i <= d; i++)
                {
                    fq_frombytes(a.coeffs[i].v, test_a_bytes);
                    fq_frombytes(b.coeffs[i].v, test_b_bytes);
                }
                benchmark(
                    [&]()
                    {
                        fq_poly_mul(&r, &a, &b);
                        benchmark_do_not_optimize(r.coeffs[0]);
                    },
                    name,
                    poly_iters(d),
                    poly_warmup(d));
            };

            bench_fp_poly_mul_ecfft(128, "fp_poly_mul+ecfft deg=128");
            bench_fp_poly_mul_ecfft(256, "fp_poly_mul+ecfft deg=256");
            bench_fp_poly_mul_ecfft(1024, "fp_poly_mul+ecfft deg=1024");

            bench_fq_poly_mul_ecfft(128, "fq_poly_mul+ecfft deg=128");
            bench_fq_poly_mul_ecfft(256, "fq_poly_mul+ecfft deg=256");
            bench_fq_poly_mul_ecfft(1024, "fq_poly_mul+ecfft deg=1024");
        }
#endif

        std::cout << std::endl;
        std::cout << "--- Divisor ---" << std::endl;

        {
            auto divisor_warmup = [](size_t n) -> size_t
            {
                if (n >= 128)
                    return 10;
                if (n >= 64)
                    return 50;
                return BENCHMARK_WARMUP_ITERATIONS;
            };

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
                    name,
                    BENCHMARK_PERFORMANCE_ITERATIONS,
                    divisor_warmup(n));
            };

            const size_t divisor_sizes[] = {1, 2, 4, 8, 16, 32};
            for (size_t sz : divisor_sizes)
            {
                std::string label = "helios_divisor n=" + std::to_string(sz);
                bench_helios_divisor(sz, label.c_str());
            }
        }

        std::cout << std::endl;
        std::cout << "--- Eval-domain divisor ops ---" << std::endl;

        {
            /* Initialize eval-domain tables */
            helios_eval_divisor_init();
            selene_eval_divisor_init();

            /* Prepare eval-domain data: two random-ish fp_evals and fq_evals */
            fp_evals fp_ev_a, fp_ev_b, fp_ev_r;
            fq_evals fq_ev_a, fq_ev_b, fq_ev_r;
            for (size_t i = 0; i < EVAL_DOMAIN_SIZE; i++)
            {
                fp_fe fp_tmp_a, fp_tmp_b;
                fq_fe fq_tmp_a, fq_tmp_b;
                fp_frombytes(fp_tmp_a, test_a_bytes);
                fp_frombytes(fp_tmp_b, test_b_bytes);
                fq_frombytes(fq_tmp_a, test_a_bytes);
                fq_frombytes(fq_tmp_b, test_b_bytes);
                fp_evals_set(&fp_ev_a, i, fp_tmp_a);
                fp_evals_set(&fp_ev_b, i, fp_tmp_b);
                fq_evals_set(&fq_ev_a, i, fq_tmp_a);
                fq_evals_set(&fq_ev_b, i, fq_tmp_b);
            }
            fp_ev_a.degree = 10;
            fp_ev_b.degree = 10;
            fq_ev_a.degree = 10;
            fq_ev_b.degree = 10;

            benchmark(
                [&]()
                {
                    fp_evals_mul(&fp_ev_r, &fp_ev_a, &fp_ev_b);
                    benchmark_do_not_optimize(fp_ev_r.limbs[0][0]);
                },
                "fp_evals_mul");

            benchmark(
                [&]()
                {
                    fq_evals_mul(&fq_ev_r, &fq_ev_a, &fq_ev_b);
                    benchmark_do_not_optimize(fq_ev_r.limbs[0][0]);
                },
                "fq_evals_mul");

            /* Prepare eval-domain divisors for divisor_mul benchmark */
            helios_eval_divisor h_ed1, h_ed2, h_ed_r;
            h_ed1.a = fp_ev_a;
            h_ed1.b = fp_ev_b;
            h_ed2.a = fp_ev_b;
            h_ed2.b = fp_ev_a;

            selene_eval_divisor s_ed1, s_ed2, s_ed_r;
            s_ed1.a = fq_ev_a;
            s_ed1.b = fq_ev_b;
            s_ed2.a = fq_ev_b;
            s_ed2.b = fq_ev_a;

            benchmark(
                [&]()
                {
                    helios_eval_divisor_mul(&h_ed_r, &h_ed1, &h_ed2);
                    benchmark_do_not_optimize(h_ed_r.a.limbs[0][0]);
                },
                "helios_eval_div_mul");

            benchmark(
                [&]()
                {
                    selene_eval_divisor_mul(&s_ed_r, &s_ed1, &s_ed2);
                    benchmark_do_not_optimize(s_ed_r.a.limbs[0][0]);
                },
                "selene_eval_div_mul");

            /* Tree reduce benchmarks */
            auto bench_helios_tree = [&](size_t n, const char *name)
            {
                std::vector<helios_jacobian> jac_pts(n);
                helios_copy(&jac_pts[0], &h_G);
                for (size_t i = 1; i < n; i++)
                    helios_dbl(&jac_pts[i], &jac_pts[i - 1]);

                std::vector<helios_affine> aff_pts(n);
                for (size_t i = 0; i < n; i++)
                    helios_to_affine(&aff_pts[i], &jac_pts[i]);

                std::vector<helios_eval_divisor> divs(n);
                for (size_t i = 0; i < n; i++)
                    helios_eval_divisor_from_point(&divs[i], &aff_pts[i]);

                helios_eval_divisor result;
                benchmark(
                    [&]()
                    {
                        /* Must re-create divs each iteration since tree_reduce may modify */
                        std::vector<helios_eval_divisor> divs_copy(divs);
                        std::vector<helios_affine> pts_copy(aff_pts);
                        helios_eval_divisor_tree_reduce(&result, divs_copy.data(), pts_copy.data(), n);
                        benchmark_do_not_optimize(result.a.limbs[0][0]);
                    },
                    name,
                    BENCHMARK_PERFORMANCE_ITERATIONS,
                    (n >= 16) ? (size_t)50 : BENCHMARK_WARMUP_ITERATIONS);
            };

            auto bench_selene_tree = [&](size_t n, const char *name)
            {
                std::vector<selene_jacobian> jac_pts(n);
                selene_copy(&jac_pts[0], &s_G);
                for (size_t i = 1; i < n; i++)
                    selene_dbl(&jac_pts[i], &jac_pts[i - 1]);

                std::vector<selene_affine> aff_pts(n);
                for (size_t i = 0; i < n; i++)
                    selene_to_affine(&aff_pts[i], &jac_pts[i]);

                std::vector<selene_eval_divisor> divs(n);
                for (size_t i = 0; i < n; i++)
                    selene_eval_divisor_from_point(&divs[i], &aff_pts[i]);

                selene_eval_divisor result;
                benchmark(
                    [&]()
                    {
                        std::vector<selene_eval_divisor> divs_copy(divs);
                        std::vector<selene_affine> pts_copy(aff_pts);
                        selene_eval_divisor_tree_reduce(&result, divs_copy.data(), pts_copy.data(), n);
                        benchmark_do_not_optimize(result.a.limbs[0][0]);
                    },
                    name,
                    BENCHMARK_PERFORMANCE_ITERATIONS,
                    (n >= 16) ? (size_t)50 : BENCHMARK_WARMUP_ITERATIONS);
            };

            bench_helios_tree(4, "helios_eval_tree n=4");
            bench_helios_tree(16, "helios_eval_tree n=16");
            bench_selene_tree(4, "selene_eval_tree n=4");
            bench_selene_tree(16, "selene_eval_tree n=16");
        }

        std::cout << std::endl;

    } /* bench_all */

    benchmark_teardown(state);

    return 0;
}
