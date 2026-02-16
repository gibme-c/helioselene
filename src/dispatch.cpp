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
 * @file dispatch.cpp
 * @brief Runtime dispatch table implementation: CPUID-based init and per-slot autotune.
 *
 * Manages 6 function pointer slots for {helios,selene} x {scalarmult, scalarmult_vartime,
 * msm_vartime}. init() uses CPUID heuristic (IFMA > AVX2 > x64). autotune() benchmarks
 * each available backend and picks the fastest per-slot.
 *
 * Thread safety: init() and autotune() build a complete dispatch table in a local variable,
 * then publish it with a release fence. get_dispatch() uses an acquire fence before reading.
 * This ensures no reader ever sees a partially-written table.
 */

#include "helioselene_dispatch.h"

#if HELIOSELENE_SIMD

#include "fp_ops.h"
#include "fq_ops.h"
#include "helios_constants.h"
#include "helioselene_cpuid.h"
#include "helioselene_secure_erase.h"
#include "selene_constants.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>

// ── Forward declarations of all implementation functions ──

// x64 baseline (always available on 64-bit)
void helios_scalarmult_x64(helios_jacobian *r, const unsigned char scalar[32], const helios_jacobian *p);
void helios_scalarmult_vartime_x64(helios_jacobian *r, const unsigned char scalar[32], const helios_jacobian *p);
void helios_msm_vartime_x64(
    helios_jacobian *result,
    const unsigned char *scalars,
    const helios_jacobian *points,
    size_t n);
void selene_scalarmult_x64(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p);
void selene_scalarmult_vartime_x64(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p);
void selene_msm_vartime_x64(
    selene_jacobian *result,
    const unsigned char *scalars,
    const selene_jacobian *points,
    size_t n);

// AVX2 (compiled when ENABLE_AVX2=ON)
#if !HELIOSELENE_NO_AVX2
void helios_scalarmult_avx2(helios_jacobian *r, const unsigned char scalar[32], const helios_jacobian *p);
void helios_scalarmult_vartime_avx2(helios_jacobian *r, const unsigned char scalar[32], const helios_jacobian *p);
void helios_msm_vartime_avx2(
    helios_jacobian *result,
    const unsigned char *scalars,
    const helios_jacobian *points,
    size_t n);
void selene_scalarmult_avx2(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p);
void selene_scalarmult_vartime_avx2(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p);
void selene_msm_vartime_avx2(
    selene_jacobian *result,
    const unsigned char *scalars,
    const selene_jacobian *points,
    size_t n);
#endif

// IFMA (compiled when ENABLE_AVX512=ON)
#if !HELIOSELENE_NO_AVX512
void helios_scalarmult_ifma(helios_jacobian *r, const unsigned char scalar[32], const helios_jacobian *p);
void helios_scalarmult_vartime_ifma(helios_jacobian *r, const unsigned char scalar[32], const helios_jacobian *p);
void helios_msm_vartime_ifma(
    helios_jacobian *result,
    const unsigned char *scalars,
    const helios_jacobian *points,
    size_t n);
void selene_scalarmult_ifma(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p);
void selene_scalarmult_vartime_ifma(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p);
void selene_msm_vartime_ifma(
    selene_jacobian *result,
    const unsigned char *scalars,
    const selene_jacobian *points,
    size_t n);
#endif

// ── File-local dispatch table — initialized to x64 baseline ──
static helioselene_dispatch_table dispatch_table = {
    helios_scalarmult_x64,
    helios_scalarmult_vartime_x64,
    helios_msm_vartime_x64,
    selene_scalarmult_x64,
    selene_scalarmult_vartime_x64,
    selene_msm_vartime_x64,
};

const helioselene_dispatch_table &helioselene_get_dispatch()
{
    std::atomic_thread_fence(std::memory_order_acquire);
    return dispatch_table;
}

// ── CPUID-based heuristic initialization ──

static std::atomic<bool> init_done {false};
static std::atomic<bool> autotune_done {false};

void helioselene_init(void)
{
    bool expected = false;
    if (init_done.compare_exchange_strong(expected, true))
    {
        // Build complete table in a local, then publish atomically
        helioselene_dispatch_table local = {
            helios_scalarmult_x64,
            helios_scalarmult_vartime_x64,
            helios_msm_vartime_x64,
            selene_scalarmult_x64,
            selene_scalarmult_vartime_x64,
            selene_msm_vartime_x64,
        };

        const uint32_t features = helioselene_cpu_features();

        // IFMA is the fastest overall backend when available.
#if !HELIOSELENE_NO_AVX512
        if (features & HELIOSELENE_CPU_AVX512IFMA)
        {
            local.helios_scalarmult = helios_scalarmult_ifma;
            local.helios_scalarmult_vartime = helios_scalarmult_vartime_ifma;
            local.helios_msm_vartime = helios_msm_vartime_ifma;
            local.selene_scalarmult = selene_scalarmult_ifma;
            local.selene_scalarmult_vartime = selene_scalarmult_vartime_ifma;
            local.selene_msm_vartime = selene_msm_vartime_ifma;
        }
        else
#endif
        {
            // AVX2 available for all 6 slots.
#if !HELIOSELENE_NO_AVX2
            if (features & HELIOSELENE_CPU_AVX2)
            {
                local.helios_scalarmult = helios_scalarmult_avx2;
                local.helios_scalarmult_vartime = helios_scalarmult_vartime_avx2;
                local.helios_msm_vartime = helios_msm_vartime_avx2;
                local.selene_scalarmult = selene_scalarmult_avx2;
                local.selene_scalarmult_vartime = selene_scalarmult_vartime_avx2;
                local.selene_msm_vartime = selene_msm_vartime_avx2;
            }
#endif
        }

        (void)features;

        // Publish: copy complete table, then release fence ensures all
        // writes are visible before any reader sees the updated table.
        dispatch_table = local;
        std::atomic_thread_fence(std::memory_order_release);
    }
}

// ── Auto-tune implementation ──

namespace
{

    using hrclock = std::chrono::high_resolution_clock;
    using ns = std::chrono::nanoseconds;

    constexpr int TUNE_WARMUP = 8;
    constexpr int TUNE_ITERS = 32;

    static int64_t bench_scalarmult(
        void (*fn)(helios_jacobian *, const unsigned char[32], const helios_jacobian *),
        const unsigned char *scalar,
        const helios_jacobian *point)
    {
        helios_jacobian result;
        for (int i = 0; i < TUNE_WARMUP; i++)
            fn(&result, scalar, point);

        int64_t best = INT64_MAX;
        for (int i = 0; i < TUNE_ITERS; i++)
        {
            auto start = hrclock::now();
            fn(&result, scalar, point);
            auto elapsed = std::chrono::duration_cast<ns>(hrclock::now() - start).count();
            if (elapsed < best)
                best = elapsed;
        }
        return best;
    }

    static int64_t bench_msm(
        void (*fn)(helios_jacobian *, const unsigned char *, const helios_jacobian *, size_t),
        const unsigned char *scalars,
        const helios_jacobian *points,
        size_t count)
    {
        helios_jacobian result;
        for (int i = 0; i < TUNE_WARMUP; i++)
            fn(&result, scalars, points, count);

        int64_t best = INT64_MAX;
        for (int i = 0; i < TUNE_ITERS; i++)
        {
            auto start = hrclock::now();
            fn(&result, scalars, points, count);
            auto elapsed = std::chrono::duration_cast<ns>(hrclock::now() - start).count();
            if (elapsed < best)
                best = elapsed;
        }
        return best;
    }

    static int64_t bench_selene_scalarmult(
        void (*fn)(selene_jacobian *, const unsigned char[32], const selene_jacobian *),
        const unsigned char *scalar,
        const selene_jacobian *point)
    {
        selene_jacobian result;
        for (int i = 0; i < TUNE_WARMUP; i++)
            fn(&result, scalar, point);

        int64_t best = INT64_MAX;
        for (int i = 0; i < TUNE_ITERS; i++)
        {
            auto start = hrclock::now();
            fn(&result, scalar, point);
            auto elapsed = std::chrono::duration_cast<ns>(hrclock::now() - start).count();
            if (elapsed < best)
                best = elapsed;
        }
        return best;
    }

    static int64_t bench_selene_msm(
        void (*fn)(selene_jacobian *, const unsigned char *, const selene_jacobian *, size_t),
        const unsigned char *scalars,
        const selene_jacobian *points,
        size_t count)
    {
        selene_jacobian result;
        for (int i = 0; i < TUNE_WARMUP; i++)
            fn(&result, scalars, points, count);

        int64_t best = INT64_MAX;
        for (int i = 0; i < TUNE_ITERS; i++)
        {
            auto start = hrclock::now();
            fn(&result, scalars, points, count);
            auto elapsed = std::chrono::duration_cast<ns>(hrclock::now() - start).count();
            if (elapsed < best)
                best = elapsed;
        }
        return best;
    }

} // anonymous namespace

void helioselene_autotune(void)
{
    bool expected = false;
    if (autotune_done.compare_exchange_strong(expected, true))
    {
        // Ensure init has run first
        helioselene_init();

        const uint32_t features = helioselene_cpu_features();

        // Build complete table in a local, then publish atomically
        helioselene_dispatch_table local = {
            helios_scalarmult_x64,
            helios_scalarmult_vartime_x64,
            helios_msm_vartime_x64,
            selene_scalarmult_x64,
            selene_scalarmult_vartime_x64,
            selene_msm_vartime_x64,
        };

        // Generate test inputs
        unsigned char s1[32];
        for (size_t i = 0; i < 32; i++)
            s1[i] = static_cast<unsigned char>(i + 1);

        // Set up Helios test point (generator)
        helios_jacobian h_point;
        fp_copy(h_point.X, HELIOS_GX);
        fp_copy(h_point.Y, HELIOS_GY);
        fp_1(h_point.Z);

        // Set up Selene test point (generator)
        selene_jacobian s_point;
        fq_copy(s_point.X, SELENE_GX);
        fq_copy(s_point.Y, SELENE_GY);
        fq_1(s_point.Z);

        // ── helios_scalarmult ──
        {
            int64_t best_time = bench_scalarmult(helios_scalarmult_x64, s1, &h_point);
            decltype(local.helios_scalarmult) best_fn = helios_scalarmult_x64;

#if !HELIOSELENE_NO_AVX2
            if (features & HELIOSELENE_CPU_AVX2)
            {
                auto t = bench_scalarmult(helios_scalarmult_avx2, s1, &h_point);
                if (t < best_time)
                {
                    best_time = t;
                    best_fn = helios_scalarmult_avx2;
                }
            }
#endif
#if !HELIOSELENE_NO_AVX512
            if (features & HELIOSELENE_CPU_AVX512IFMA)
            {
                auto t = bench_scalarmult(helios_scalarmult_ifma, s1, &h_point);
                if (t < best_time)
                {
                    best_time = t;
                    best_fn = helios_scalarmult_ifma;
                }
            }
#endif
            local.helios_scalarmult = best_fn;
        }

        // ── helios_scalarmult_vartime ──
        {
            int64_t best_time = bench_scalarmult(helios_scalarmult_vartime_x64, s1, &h_point);
            decltype(local.helios_scalarmult_vartime) best_fn = helios_scalarmult_vartime_x64;

#if !HELIOSELENE_NO_AVX2
            if (features & HELIOSELENE_CPU_AVX2)
            {
                auto t = bench_scalarmult(helios_scalarmult_vartime_avx2, s1, &h_point);
                if (t < best_time)
                {
                    best_time = t;
                    best_fn = helios_scalarmult_vartime_avx2;
                }
            }
#endif
#if !HELIOSELENE_NO_AVX512
            if (features & HELIOSELENE_CPU_AVX512IFMA)
            {
                auto t = bench_scalarmult(helios_scalarmult_vartime_ifma, s1, &h_point);
                if (t < best_time)
                {
                    best_time = t;
                    best_fn = helios_scalarmult_vartime_ifma;
                }
            }
#endif
            local.helios_scalarmult_vartime = best_fn;
        }

        // ── helios_msm_vartime ──
        {
            constexpr size_t MSM_N = 16;
            unsigned char msm_scalars[MSM_N * 32];
            helios_jacobian msm_points[MSM_N];
            for (size_t i = 0; i < MSM_N; i++)
            {
                for (size_t j = 0; j < 32; j++)
                    msm_scalars[i * 32 + j] = static_cast<unsigned char>(j + i + 1);
                msm_points[i] = h_point;
            }

            int64_t best_time = bench_msm(helios_msm_vartime_x64, msm_scalars, msm_points, MSM_N);
            decltype(local.helios_msm_vartime) best_fn = helios_msm_vartime_x64;

#if !HELIOSELENE_NO_AVX2
            if (features & HELIOSELENE_CPU_AVX2)
            {
                auto t = bench_msm(helios_msm_vartime_avx2, msm_scalars, msm_points, MSM_N);
                if (t < best_time)
                {
                    best_time = t;
                    best_fn = helios_msm_vartime_avx2;
                }
            }
#endif
#if !HELIOSELENE_NO_AVX512
            if (features & HELIOSELENE_CPU_AVX512IFMA)
            {
                auto t = bench_msm(helios_msm_vartime_ifma, msm_scalars, msm_points, MSM_N);
                if (t < best_time)
                {
                    best_time = t;
                    best_fn = helios_msm_vartime_ifma;
                }
            }
#endif
            local.helios_msm_vartime = best_fn;

            helioselene_secure_erase(msm_scalars, sizeof(msm_scalars));
        }

        // ── selene_scalarmult ──
        {
            int64_t best_time = bench_selene_scalarmult(selene_scalarmult_x64, s1, &s_point);
            decltype(local.selene_scalarmult) best_fn = selene_scalarmult_x64;

#if !HELIOSELENE_NO_AVX2
            if (features & HELIOSELENE_CPU_AVX2)
            {
                auto t = bench_selene_scalarmult(selene_scalarmult_avx2, s1, &s_point);
                if (t < best_time)
                {
                    best_time = t;
                    best_fn = selene_scalarmult_avx2;
                }
            }
#endif
#if !HELIOSELENE_NO_AVX512
            if (features & HELIOSELENE_CPU_AVX512IFMA)
            {
                auto t = bench_selene_scalarmult(selene_scalarmult_ifma, s1, &s_point);
                if (t < best_time)
                {
                    best_time = t;
                    best_fn = selene_scalarmult_ifma;
                }
            }
#endif
            local.selene_scalarmult = best_fn;
        }

        // ── selene_scalarmult_vartime ──
        {
            int64_t best_time = bench_selene_scalarmult(selene_scalarmult_vartime_x64, s1, &s_point);
            decltype(local.selene_scalarmult_vartime) best_fn = selene_scalarmult_vartime_x64;

#if !HELIOSELENE_NO_AVX2
            if (features & HELIOSELENE_CPU_AVX2)
            {
                auto t = bench_selene_scalarmult(selene_scalarmult_vartime_avx2, s1, &s_point);
                if (t < best_time)
                {
                    best_time = t;
                    best_fn = selene_scalarmult_vartime_avx2;
                }
            }
#endif
#if !HELIOSELENE_NO_AVX512
            if (features & HELIOSELENE_CPU_AVX512IFMA)
            {
                auto t = bench_selene_scalarmult(selene_scalarmult_vartime_ifma, s1, &s_point);
                if (t < best_time)
                {
                    best_time = t;
                    best_fn = selene_scalarmult_vartime_ifma;
                }
            }
#endif
            local.selene_scalarmult_vartime = best_fn;
        }

        // ── selene_msm_vartime ──
        {
            constexpr size_t MSM_N = 16;
            unsigned char msm_scalars[MSM_N * 32];
            selene_jacobian msm_points[MSM_N];
            for (size_t i = 0; i < MSM_N; i++)
            {
                for (size_t j = 0; j < 32; j++)
                    msm_scalars[i * 32 + j] = static_cast<unsigned char>(j + i + 1);
                msm_points[i] = s_point;
            }

            int64_t best_time = bench_selene_msm(selene_msm_vartime_x64, msm_scalars, msm_points, MSM_N);
            decltype(local.selene_msm_vartime) best_fn = selene_msm_vartime_x64;

#if !HELIOSELENE_NO_AVX2
            if (features & HELIOSELENE_CPU_AVX2)
            {
                auto t = bench_selene_msm(selene_msm_vartime_avx2, msm_scalars, msm_points, MSM_N);
                if (t < best_time)
                {
                    best_time = t;
                    best_fn = selene_msm_vartime_avx2;
                }
            }
#endif
#if !HELIOSELENE_NO_AVX512
            if (features & HELIOSELENE_CPU_AVX512IFMA)
            {
                auto t = bench_selene_msm(selene_msm_vartime_ifma, msm_scalars, msm_points, MSM_N);
                if (t < best_time)
                {
                    best_time = t;
                    best_fn = selene_msm_vartime_ifma;
                }
            }
#endif
            local.selene_msm_vartime = best_fn;

            helioselene_secure_erase(msm_scalars, sizeof(msm_scalars));
        }

        (void)features;

        // Defense-in-depth: erase test scalar
        helioselene_secure_erase(s1, sizeof(s1));

        // Publish: copy complete table, then release fence ensures all
        // writes are visible before any reader sees the updated table.
        dispatch_table = local;
        std::atomic_thread_fence(std::memory_order_release);
    }
}

#endif // HELIOSELENE_SIMD
