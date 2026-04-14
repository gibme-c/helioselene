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

/**
 * @file src/tests_timing.cpp
 * @brief Statistical timing-leak harness for the constant-time entry points.
 *
 * Builds only when the build is configured with -DENABLE_TIMING_TESTS=ON.
 * Runs a dudect-style two-class timing comparison on RanPoint::pedersen_commit
 * (the default CT entry point after C7): class A uses a "low" blinding scalar
 * (mostly zero bytes), class B uses a "high" blinding scalar (mostly 0xFF-
 * masked bytes). Under the CT-ness hypothesis, the two classes' wall-clock
 * timings should be drawn from the same distribution.
 *
 * Statistic: Welch's t-test on binned (low-quantile) samples. Welch's is
 * simpler than Mann-Whitney U at 100k samples and is the statistic dudect
 * itself uses. We retain only the lower 10th percentile per class (dropping
 * noise-heavy tails from scheduling jitter) before computing the t value.
 *
 * Gate: |t| < 4.5 (roughly p > 10^-6 at ~10k degrees of freedom after
 * filtering). A failure here is advisory -- this harness is intentionally
 * non-blocking in CI because shared-runner jitter swamps the signal. Use
 * locally on a quiet box before a release, and inspect the raw numbers in
 * the log if the gate trips.
 *
 * This file only participates in builds where ENABLE_TIMING_TESTS is ON;
 * the CMake block in CMakeLists.txt adds it to a separate executable
 * (ranshaw-tests-timing) so it never lands on the default PR path.
 */

#include "ranshaw.h"
#include "ranshaw_primitives.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace ranshaw;

namespace
{

    using hrclock = std::chrono::high_resolution_clock;
    using ns = std::chrono::nanoseconds;

    constexpr int SAMPLES_PER_CLASS = 50000;
    constexpr int WARMUP = 2000;
    constexpr double TRIM_FRACTION = 0.10; /* keep lowest 10 percent (quiet samples) */
    constexpr double T_CRITICAL = 4.5;

    /* Welch's t statistic on the lowest-quantile samples of each class. */
    double welch_t_trim(std::vector<int64_t> a, std::vector<int64_t> b)
    {
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        const size_t keep_a = static_cast<size_t>(static_cast<double>(a.size()) * TRIM_FRACTION);
        const size_t keep_b = static_cast<size_t>(static_cast<double>(b.size()) * TRIM_FRACTION);
        a.resize(keep_a);
        b.resize(keep_b);

        double ma = 0.0, mb = 0.0;
        for (auto v : a)
            ma += static_cast<double>(v);
        for (auto v : b)
            mb += static_cast<double>(v);
        ma /= static_cast<double>(static_cast<long long>(keep_a));
        mb /= static_cast<double>(static_cast<long long>(keep_b));

        double va = 0.0, vb = 0.0;
        for (auto v : a)
        {
            double d = static_cast<double>(v) - ma;
            va += d * d;
        }
        for (auto v : b)
        {
            double d = static_cast<double>(v) - mb;
            vb += d * d;
        }
        va /= static_cast<double>(static_cast<long long>(keep_a) - 1);
        vb /= static_cast<double>(static_cast<long long>(keep_b) - 1);

        const double se = std::sqrt(
            va / static_cast<double>(static_cast<long long>(keep_a))
            + vb / static_cast<double>(static_cast<long long>(keep_b)));
        if (se == 0.0)
            return 0.0;
        return (ma - mb) / se;
    }

    template<typename Callable> int64_t time_one(Callable &&fn)
    {
        auto t0 = hrclock::now();
        fn();
        auto t1 = hrclock::now();
        return std::chrono::duration_cast<ns>(t1 - t0).count();
    }

    void build_low_blinding(unsigned char out[32])
    {
        std::memset(out, 0, 32);
        out[0] = 1;
    }

    void build_high_blinding(unsigned char out[32])
    {
        std::memset(out, 0xFF, 32);
        out[31] = 0x0F; /* keep within group order for Ran */
    }

    int run_ran_pedersen_timing()
    {
        std::cout << "  ran pedersen_commit CT timing classes:" << std::endl;

        unsigned char low_bytes[32];
        unsigned char high_bytes[32];
        build_low_blinding(low_bytes);
        build_high_blinding(high_bytes);

        auto s_low = RanScalar::from_bytes(low_bytes).value();
        auto s_high = RanScalar::from_bytes(high_bytes).value();

        /* Fixed H and small value/generator set: the leak-sensitive scalar here
         * is the blinding factor, so we hold everything else constant. */
        auto H = RanPoint::generator().scalar_mul_vartime(RanScalar::from_bytes(low_bytes).value());
        std::vector<RanScalar> values(size_t {4});
        std::vector<RanPoint> generators(size_t {4});
        unsigned char buf[32] = {0};
        for (size_t i = 0; i < 4; i++)
        {
            buf[0] = static_cast<unsigned char>(i + 1);
            values[i] = RanScalar::from_bytes(buf).value();
            generators[i] = RanPoint::generator().scalar_mul_vartime(values[i]);
        }

        /* Warm-up: page caches, branch predictors, turbo ramp. */
        for (int i = 0; i < WARMUP; i++)
        {
            (void)RanPoint::pedersen_commit(s_low, H, values.data(), generators.data(), 4);
            (void)RanPoint::pedersen_commit(s_high, H, values.data(), generators.data(), 4);
        }

        std::vector<int64_t> class_a, class_b;
        class_a.reserve(SAMPLES_PER_CLASS);
        class_b.reserve(SAMPLES_PER_CLASS);
        for (int i = 0; i < SAMPLES_PER_CLASS; i++)
        {
            /* Interleave the two classes to share scheduler jitter. */
            class_a.push_back(
                time_one([&] { (void)RanPoint::pedersen_commit(s_low, H, values.data(), generators.data(), 4); }));
            class_b.push_back(
                time_one([&] { (void)RanPoint::pedersen_commit(s_high, H, values.data(), generators.data(), 4); }));
        }

        const double t = welch_t_trim(class_a, class_b);
        std::cout << "    samples=" << SAMPLES_PER_CLASS << " per class, t=" << std::fixed << std::setprecision(3) << t
                  << " (gate: |t| < " << T_CRITICAL << ")" << std::endl;
        if (std::abs(t) < T_CRITICAL)
        {
            std::cout << "    PASS" << std::endl;
            return 0;
        }
        std::cout << "    FAIL (timing difference exceeds threshold)" << std::endl;
        return 1;
    }

    int run_shaw_pedersen_timing()
    {
        std::cout << "  shaw pedersen_commit CT timing classes:" << std::endl;

        unsigned char low_bytes[32];
        unsigned char high_bytes[32];
        build_low_blinding(low_bytes);
        build_high_blinding(high_bytes);

        auto s_low = ShawScalar::from_bytes(low_bytes).value();
        auto s_high = ShawScalar::from_bytes(high_bytes).value();

        auto H = ShawPoint::generator().scalar_mul_vartime(ShawScalar::from_bytes(low_bytes).value());
        std::vector<ShawScalar> values(size_t {4});
        std::vector<ShawPoint> generators(size_t {4});
        unsigned char buf[32] = {0};
        for (size_t i = 0; i < 4; i++)
        {
            buf[0] = static_cast<unsigned char>(i + 1);
            values[i] = ShawScalar::from_bytes(buf).value();
            generators[i] = ShawPoint::generator().scalar_mul_vartime(values[i]);
        }

        for (int i = 0; i < WARMUP; i++)
        {
            (void)ShawPoint::pedersen_commit(s_low, H, values.data(), generators.data(), 4);
            (void)ShawPoint::pedersen_commit(s_high, H, values.data(), generators.data(), 4);
        }

        std::vector<int64_t> class_a, class_b;
        class_a.reserve(SAMPLES_PER_CLASS);
        class_b.reserve(SAMPLES_PER_CLASS);
        for (int i = 0; i < SAMPLES_PER_CLASS; i++)
        {
            class_a.push_back(
                time_one([&] { (void)ShawPoint::pedersen_commit(s_low, H, values.data(), generators.data(), 4); }));
            class_b.push_back(
                time_one([&] { (void)ShawPoint::pedersen_commit(s_high, H, values.data(), generators.data(), 4); }));
        }

        const double t = welch_t_trim(class_a, class_b);
        std::cout << "    samples=" << SAMPLES_PER_CLASS << " per class, t=" << std::fixed << std::setprecision(3) << t
                  << " (gate: |t| < " << T_CRITICAL << ")" << std::endl;
        if (std::abs(t) < T_CRITICAL)
        {
            std::cout << "    PASS" << std::endl;
            return 0;
        }
        std::cout << "    FAIL (timing difference exceeds threshold)" << std::endl;
        return 1;
    }

} // anonymous namespace

int main()
{
    std::cout << "=== ranshaw-tests-timing ===" << std::endl;
    std::cout << "Initializing dispatch (autotune)..." << std::endl;
    ranshaw_autotune();

    int failed = 0;
    failed += run_ran_pedersen_timing();
    failed += run_shaw_pedersen_timing();

    std::cout << std::endl;
    if (failed == 0)
    {
        std::cout << "ALL TIMING CLASSES PASSED" << std::endl;
        return 0;
    }
    std::cout << failed << " TIMING CLASS(ES) FAILED" << std::endl;
    /* Advisory-only: return 0 so CI can log a failure without blocking. The
     * build option was explicitly requested; the user sees the result and
     * decides what to do with it. */
    return 0;
}
