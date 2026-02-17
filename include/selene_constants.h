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

#ifndef HELIOSELENE_SELENE_CONSTANTS_H
#define HELIOSELENE_SELENE_CONSTANTS_H

#include "fq.h"

/*
 * Selene curve: y^2 = x^3 - 3x + b over F_q (q = 2^255 - gamma)
 *
 * b = 50691664119640283727448954162351551669994268339720539671652090628799494505816
 */
#if HELIOSELENE_PLATFORM_64BIT
static const fq_fe SELENE_B =
    {0x60983cb5a4558ULL, 0x3e0d5d201cd1bULL, 0x7ff89e7ce512fULL, 0x360bfa8ddd2caULL, 0x7012771369587ULL};
static const fq_fe SELENE_GX = {0x1ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL};
static const fq_fe SELENE_GY =
    {0x60aa6a1d3fdd2ULL, 0x3191e1366ee83ULL, 0x572097e4e2ec6ULL, 0x5492be498bba2ULL, 0x7a19d927b85ccULL};
#else
static const fq_fe SELENE_B =
    {56247640, 25321714, 33672475, 16266612, 63852847, 33546873, 14537418, 14168042, 20354439, 29379036};
static const fq_fe SELENE_GX = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const fq_fe SELENE_GY =
    {30670290, 25340328, 57077379, 12994436, 38678214, 22839903, 10009506, 22170361, 41649612, 32008036};
#endif

/*
 * Selene group order = p = 2^255 - 19
 * = 57896044618658097711785492504343953926634992332820282019728792003956564819949
 *
 * Stored as 32 bytes little-endian for scalar operations.
 */
static const unsigned char SELENE_ORDER[32] = {0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                               0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                               0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};

#endif // HELIOSELENE_SELENE_CONSTANTS_H
