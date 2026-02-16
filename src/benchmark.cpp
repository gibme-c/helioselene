#include <iostream>

#include "helioselene_benchmark.h"

#include "fp.h"
#include "fp_ops.h"
#include "fp_mul.h"
#include "fp_sq.h"
#include "fp_invert.h"
#include "fp_sqrt.h"
#include "fp_frombytes.h"

#include "fq.h"
#include "fq_ops.h"
#include "fq_mul.h"
#include "fq_sq.h"
#include "fq_invert.h"
#include "fq_sqrt.h"
#include "fq_frombytes.h"

#include "helios.h"
#include "helios_constants.h"
#include "helios_ops.h"
#include "helios_dbl.h"
#include "helios_madd.h"
#include "helios_add.h"
#include "helios_tobytes.h"
#include "helios_frombytes.h"
#include "helios_scalarmult.h"
#include "helios_scalarmult_vartime.h"

#include "selene.h"
#include "selene_constants.h"
#include "selene_ops.h"
#include "selene_dbl.h"
#include "selene_madd.h"
#include "selene_add.h"
#include "selene_tobytes.h"
#include "selene_frombytes.h"
#include "selene_scalarmult.h"
#include "selene_scalarmult_vartime.h"

static const unsigned char test_a_bytes[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
    0xca, 0xef, 0xbe, 0xad, 0xde, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00};
static const unsigned char test_b_bytes[32] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x0d, 0xf0, 0xad,
    0xba, 0xce, 0xfa, 0xed, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00};

static const unsigned char test_scalar[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12,
    0xbe, 0xba, 0xfe, 0xca, 0xef, 0xbe, 0xad, 0xde,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10};

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
        [&]() {
            fp_add(fp_c, fp_a, fp_b);
            benchmark_do_not_optimize(fp_c);
        },
        "fp_add");

    benchmark_long(
        [&]() {
            fp_sub(fp_c, fp_a, fp_b);
            benchmark_do_not_optimize(fp_c);
        },
        "fp_sub");

    benchmark_long(
        [&]() {
            fp_mul(fp_c, fp_a, fp_b);
            benchmark_do_not_optimize(fp_c);
        },
        "fp_mul");

    benchmark_long(
        [&]() {
            fp_sq(fp_c, fp_a);
            benchmark_do_not_optimize(fp_c);
        },
        "fp_sq");

    benchmark(
        [&]() {
            fp_invert(fp_c, fp_a);
            benchmark_do_not_optimize(fp_c);
        },
        "fp_invert");

    benchmark(
        [&]() {
            fp_sqrt(fp_c, fp_a);
            benchmark_do_not_optimize(fp_c);
        },
        "fp_sqrt");

    std::cout << std::endl;
    std::cout << "--- F_q (2^255 - gamma) ---" << std::endl;

    benchmark_long(
        [&]() {
            fq_add(fq_c, fq_a, fq_b);
            benchmark_do_not_optimize(fq_c);
        },
        "fq_add");

    benchmark_long(
        [&]() {
            fq_sub(fq_c, fq_a, fq_b);
            benchmark_do_not_optimize(fq_c);
        },
        "fq_sub");

    benchmark_long(
        [&]() {
            fq_mul(fq_c, fq_a, fq_b);
            benchmark_do_not_optimize(fq_c);
        },
        "fq_mul");

    benchmark_long(
        [&]() {
            fq_sq(fq_c, fq_a);
            benchmark_do_not_optimize(fq_c);
        },
        "fq_sq");

    benchmark(
        [&]() {
            fq_invert(fq_c, fq_a);
            benchmark_do_not_optimize(fq_c);
        },
        "fq_invert");

    benchmark(
        [&]() {
            fq_sqrt(fq_c, fq_a);
            benchmark_do_not_optimize(fq_c);
        },
        "fq_sqrt");

    std::cout << std::endl;
    std::cout << "--- Helios (over F_p) ---" << std::endl;

    benchmark_long(
        [&]() {
            helios_dbl(&h_result, &h_G);
            benchmark_do_not_optimize(h_result);
        },
        "helios_dbl");

    benchmark_long(
        [&]() {
            helios_madd(&h_result, &h_2G, &h_G_aff);
            benchmark_do_not_optimize(h_result);
        },
        "helios_madd");

    benchmark_long(
        [&]() {
            helios_add(&h_result, &h_2G, &h_G);
            benchmark_do_not_optimize(h_result);
        },
        "helios_add");

    benchmark(
        [&]() {
            helios_tobytes(point_bytes, &h_G);
            benchmark_do_not_optimize(point_bytes);
        },
        "helios_tobytes");

    benchmark(
        [&]() {
            helios_frombytes(&h_result, point_bytes);
            benchmark_do_not_optimize(h_result);
        },
        "helios_frombytes");

    benchmark(
        [&]() {
            helios_scalarmult(&h_result, test_scalar, &h_G);
            benchmark_do_not_optimize(h_result);
        },
        "helios_scalarmult");

    benchmark(
        [&]() {
            helios_scalarmult_vartime(&h_result, test_scalar, &h_G);
            benchmark_do_not_optimize(h_result);
        },
        "helios_scalarmult_vt");

    std::cout << std::endl;
    std::cout << "--- Selene (over F_q) ---" << std::endl;

    benchmark_long(
        [&]() {
            selene_dbl(&s_result, &s_G);
            benchmark_do_not_optimize(s_result);
        },
        "selene_dbl");

    benchmark_long(
        [&]() {
            selene_madd(&s_result, &s_2G, &s_G_aff);
            benchmark_do_not_optimize(s_result);
        },
        "selene_madd");

    benchmark_long(
        [&]() {
            selene_add(&s_result, &s_2G, &s_G);
            benchmark_do_not_optimize(s_result);
        },
        "selene_add");

    benchmark(
        [&]() {
            selene_tobytes(point_bytes, &s_G);
            benchmark_do_not_optimize(point_bytes);
        },
        "selene_tobytes");

    benchmark(
        [&]() {
            selene_frombytes(&s_result, point_bytes);
            benchmark_do_not_optimize(s_result);
        },
        "selene_frombytes");

    benchmark(
        [&]() {
            selene_scalarmult(&s_result, test_scalar, &s_G);
            benchmark_do_not_optimize(s_result);
        },
        "selene_scalarmult");

    benchmark(
        [&]() {
            selene_scalarmult_vartime(&s_result, test_scalar, &s_G);
            benchmark_do_not_optimize(s_result);
        },
        "selene_scalarmult_vt");

    std::cout << std::endl;

    benchmark_teardown(state);

    return 0;
}
