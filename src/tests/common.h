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
 * @file src/tests/common.h
 * @brief Shared state and assertion helpers for ranshaw unit test TUs.
 *
 * Declarations for the counters, byte/int/nonzero assertion helpers, shared
 * constant buffers, and the `tv` namespace alias. Each topic TU in src/tests/
 * includes this header and references the externs defined in common.cpp.
 */

#ifndef RANSHAW_SRC_TESTS_COMMON_H
#define RANSHAW_SRC_TESTS_COMMON_H

#include "ranshaw.h"
#include "ranshaw_primitives.h"
#include "ranshaw_test_vectors.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace tv = ranshaw_test_vectors;

extern int tests_run;
extern int tests_passed;
extern int tests_failed;

std::string hex(const unsigned char *data, size_t len);
bool check_bytes(const char *test_name, const unsigned char *expected, const unsigned char *actual, size_t len);
bool check_int(const char *test_name, int expected, int actual);
bool check_nonzero(const char *test_name, int actual);
bool check_true(const char *test_name, bool condition);

extern const unsigned char test_a_bytes[32];
extern const unsigned char test_b_bytes[32];
extern const unsigned char one_bytes[32];
extern const unsigned char zero_bytes[32];
extern const unsigned char four_bytes[32];

#endif // RANSHAW_SRC_TESTS_COMMON_H
