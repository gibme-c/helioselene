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
 * @file src/fuzz_tests/registry.h
 * @brief Declarations of every property-based fuzz topic function.
 */

#ifndef RANSHAW_SRC_FUZZ_TESTS_REGISTRY_H
#define RANSHAW_SRC_FUZZ_TESTS_REGISTRY_H

void fuzz_scalar_arithmetic();
void fuzz_scalar_edge_cases();
void fuzz_point_arithmetic();
void fuzz_ipa_edge_cases();
void fuzz_serialization_roundtrip();
void fuzz_cross_curve_cycle();
void fuzz_scalarmul_consistency();
void fuzz_msm_random();
void fuzz_msm_sparse();
void fuzz_map_to_curve();
void fuzz_wei25519_bridge();
void fuzz_pedersen();
void fuzz_batch_affine();
void fuzz_polynomial();
void fuzz_polynomial_protocol_sizes();
void fuzz_divisor();
void fuzz_divisor_scalar_mul();
void fuzz_operator_plus_regression();
void fuzz_verification_equation();
void fuzz_reduce_wide_distribution();
void fuzz_all_path_cross_validation();
void fuzz_ecfft_poly_mul();

#endif // RANSHAW_SRC_FUZZ_TESTS_REGISTRY_H
