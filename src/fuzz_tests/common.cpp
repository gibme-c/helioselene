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

#include "fuzz_tests/common.h"

int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;
bool quiet_mode = false;
uint64_t global_seed = 0ULL;

std::string hex(const unsigned char *data, size_t len)
{
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i)
        oss << std::hex << std::setfill('0') << std::setw(2) << (int)data[i];
    return oss.str();
}

bool check_bytes(const char *test_name, const unsigned char *expected, const unsigned char *actual, size_t len)
{
    ++tests_run;
    if (std::memcmp(expected, actual, len) == 0)
    {
        ++tests_passed;
        if (!quiet_mode)
            std::cout << "  PASS: " << test_name << std::endl;
        return true;
    }
    else
    {
        ++tests_failed;
        std::cout << "  FAIL: " << test_name << std::endl;
        std::cout << "    expected: " << hex(expected, len) << std::endl;
        std::cout << "    actual:   " << hex(actual, len) << std::endl;
        return false;
    }
}

bool check_true(const char *test_name, bool condition)
{
    ++tests_run;
    if (condition)
    {
        ++tests_passed;
        if (!quiet_mode)
            std::cout << "  PASS: " << test_name << std::endl;
        return true;
    }
    else
    {
        ++tests_failed;
        std::cout << "  FAIL: " << test_name << std::endl;
        return false;
    }
}

RanScalar random_ran_scalar(xoshiro256ss &rng)
{
    uint8_t wide[64];
    rng.fill_bytes(wide, 64);
    return RanScalar::reduce_wide(wide);
}

ShawScalar random_shaw_scalar(xoshiro256ss &rng)
{
    uint8_t wide[64];
    rng.fill_bytes(wide, 64);
    return ShawScalar::reduce_wide(wide);
}

RanPoint random_ran_point(xoshiro256ss &rng)
{
    return RanPoint::generator().scalar_mul_vartime(random_ran_scalar(rng));
}

ShawPoint random_shaw_point(xoshiro256ss &rng)
{
    return ShawPoint::generator().scalar_mul_vartime(random_shaw_scalar(rng));
}

bool ran_points_equal(const RanPoint &a, const RanPoint &b)
{
    auto ab = a.to_bytes();
    auto bb = b.to_bytes();
    return std::memcmp(ab.data(), bb.data(), 32) == 0;
}

bool shaw_points_equal(const ShawPoint &a, const ShawPoint &b)
{
    auto ab = a.to_bytes();
    auto bb = b.to_bytes();
    return std::memcmp(ab.data(), bb.data(), 32) == 0;
}
