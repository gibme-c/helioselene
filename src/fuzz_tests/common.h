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
 * @file src/fuzz_tests/common.h
 * @brief Shared state, PRNG, and helpers for ranshaw property-based fuzz TUs.
 *
 * Separate from src/tests/common.h because ranshaw-fuzz-tests is a distinct
 * binary with its own counters, a --quiet toggle, a seedable PRNG, and a set
 * of random-generator helpers. The xoshiro256ss struct is defined inline in
 * the header so every topic TU gets the same ABI.
 */

#ifndef RANSHAW_SRC_FUZZ_TESTS_COMMON_H
#define RANSHAW_SRC_FUZZ_TESTS_COMMON_H

#include "ranshaw.h"
#include "ranshaw_primitives.h"

#ifdef RANSHAW_ECFFT
#include "ecfft_fp.h"
#include "ecfft_fq.h"
#endif

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace ranshaw;

extern int tests_run;
extern int tests_passed;
extern int tests_failed;
extern bool quiet_mode;
extern uint64_t global_seed;

std::string hex(const unsigned char *data, size_t len);
bool check_bytes(const char *test_name, const unsigned char *expected, const unsigned char *actual, size_t len);
bool check_true(const char *test_name, bool condition);

/* ======================================================================
 * PRNG: xoshiro256** with splitmix64 seeding
 * ====================================================================== */

struct xoshiro256ss
{
    uint64_t s[4];

    static uint64_t splitmix64(uint64_t &state)
    {
        uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    void seed(uint64_t seed_val)
    {
        uint64_t sm = seed_val;
        s[0] = splitmix64(sm);
        s[1] = splitmix64(sm);
        s[2] = splitmix64(sm);
        s[3] = splitmix64(sm);
    }

    static uint64_t rotl(uint64_t x, int k)
    {
        return (x << k) | (x >> (64 - k));
    }

    uint64_t next()
    {
        const uint64_t result = rotl(s[1] * 5, 7) * 9;
        const uint64_t t = s[1] << 17;
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = rotl(s[3], 45);
        return result;
    }

    void fill_bytes(uint8_t *buf, size_t len)
    {
        size_t i = 0;
        while (i + 8 <= len)
        {
            uint64_t v = next();
            std::memcpy(buf + i, &v, 8);
            i += 8;
        }
        if (i < len)
        {
            uint64_t v = next();
            std::memcpy(buf + i, &v, len - i);
        }
    }
};

RanScalar random_ran_scalar(xoshiro256ss &rng);
ShawScalar random_shaw_scalar(xoshiro256ss &rng);
RanPoint random_ran_point(xoshiro256ss &rng);
ShawPoint random_shaw_point(xoshiro256ss &rng);
bool ran_points_equal(const RanPoint &a, const RanPoint &b);
bool shaw_points_equal(const ShawPoint &a, const ShawPoint &b);

#endif // RANSHAW_SRC_FUZZ_TESTS_COMMON_H
