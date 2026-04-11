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
 * @file src/tests/concurrency.cpp
 * @brief Concurrency smoke tests for the --concurrency-only mode.
 *
 * Designed to run under TSan on Linux/macOS Clang. Exercises the paths
 * most likely to hide a data race:
 *   - ranshaw_init() called concurrently from multiple threads
 *   - ranshaw_autotune() called concurrently from multiple threads
 *   - RanPoint/ShawPoint scalar multiplication from the same base point
 *     with distinct scalars across multiple threads
 *
 * TSan will report a data race if any library state is mutated without
 * synchronization while another thread reads or writes it. Functionally
 * the test just checks that every thread produces a well-formed result.
 */

#include "tests/common.h"
#include "tests/registry.h"

#include <atomic>
#include <thread>
#include <vector>

namespace
{
    constexpr int kNumThreads = 4;
    constexpr int kOpsPerThread = 256;

    /* Deterministic scalar source — avoids seeding std::random_device under
     * TSan (the default RNG pulls from /dev/urandom on Linux and Windows
     * CNG on MSVC, and we want the same sequence every run regardless of
     * host state). */
    uint64_t splitmix64(uint64_t &state)
    {
        uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    void fill_scalar_bytes(uint8_t out[64], uint64_t seed)
    {
        uint64_t state = seed;
        for (int i = 0; i < 8; i++)
        {
            uint64_t v = splitmix64(state);
            std::memcpy(out + i * 8, &v, 8);
        }
    }

    void test_init_race()
    {
        std::atomic<int> ok {0};
        std::vector<std::thread> threads;
        threads.reserve(kNumThreads);
        for (int i = 0; i < kNumThreads; i++)
        {
            threads.emplace_back(
                [&ok]()
                {
                    ranshaw_init();
                    ok.fetch_add(1, std::memory_order_relaxed);
                });
        }
        for (auto &t : threads)
            t.join();
        check_int("concurrency: ranshaw_init() race", kNumThreads, ok.load());
    }

    void test_autotune_race()
    {
        std::atomic<int> ok {0};
        std::vector<std::thread> threads;
        threads.reserve(kNumThreads);
        for (int i = 0; i < kNumThreads; i++)
        {
            threads.emplace_back(
                [&ok]()
                {
                    ranshaw_autotune();
                    ok.fetch_add(1, std::memory_order_relaxed);
                });
        }
        for (auto &t : threads)
            t.join();
        check_int("concurrency: ranshaw_autotune() race", kNumThreads, ok.load());
    }

    template<class Point, class Scalar> void test_scalarmult_race(const char *label)
    {
        auto G = Point::generator();
        std::atomic<int> ok {0};
        std::vector<std::thread> threads;
        threads.reserve(kNumThreads);
        for (int tid = 0; tid < kNumThreads; tid++)
        {
            threads.emplace_back(
                [tid, &ok, &G]()
                {
                    int local_ok = 0;
                    for (int i = 0; i < kOpsPerThread; i++)
                    {
                        uint8_t bytes[64];
                        fill_scalar_bytes(bytes, static_cast<uint64_t>(tid) * 0x10000 + static_cast<uint64_t>(i));
                        Scalar s = Scalar::reduce_wide(bytes);
                        auto P = G.scalar_mul_vartime(s);
                        /* Any well-formed point satisfies P + (-P) == identity.
                         * A data race that corrupts dispatch state would likely
                         * produce an invalid point that fails this invariant. */
                        if ((P + (-P)).is_identity())
                            local_ok++;
                    }
                    if (local_ok == kOpsPerThread)
                        ok.fetch_add(1, std::memory_order_relaxed);
                });
        }
        for (auto &t : threads)
            t.join();
        check_int((std::string("concurrency: ") + label + " scalarmul race").c_str(), kNumThreads, ok.load());
    }
} // namespace

void test_concurrency()
{
    std::cout << std::endl << "=== Concurrency (TSan) ===" << std::endl;
    test_init_race();
    test_autotune_race();
    test_scalarmult_race<ranshaw::RanPoint, ranshaw::RanScalar>("Ran");
    test_scalarmult_race<ranshaw::ShawPoint, ranshaw::ShawScalar>("Shaw");
}
