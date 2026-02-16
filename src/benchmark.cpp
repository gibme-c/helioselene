#include <iostream>

#include "helioselene_benchmark.h"

#include "fp.h"
#include "fp_ops.h"
#include "fp_mul.h"
#include "fp_sq.h"
#include "fp_invert.h"
#include "fp_frombytes.h"

#include "fq.h"
#include "fq_ops.h"
#include "fq_mul.h"
#include "fq_sq.h"
#include "fq_invert.h"
#include "fq_sqrt.h"
#include "fq_frombytes.h"

static const unsigned char test_a_bytes[32] = {0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, 0xbe, 0xba, 0xfe,
    0xca, 0xef, 0xbe, 0xad, 0xde, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00};
static const unsigned char test_b_bytes[32] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x0d, 0xf0, 0xad,
    0xba, 0xce, 0xfa, 0xed, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00};

int main()
{
    auto state = benchmark_setup();

    fp_fe fp_a, fp_b, fp_c;
    fp_frombytes(fp_a, test_a_bytes);
    fp_frombytes(fp_b, test_b_bytes);

    fq_fe fq_a, fq_b, fq_c;
    fq_frombytes(fq_a, test_a_bytes);
    fq_frombytes(fq_b, test_b_bytes);

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

    benchmark_teardown(state);

    return 0;
}
