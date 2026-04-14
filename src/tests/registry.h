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
 * @file src/tests/registry.h
 * @brief Declarations of every topic test function.
 *
 * tests_main.cpp includes this header and invokes each function in order.
 * Each function is defined in the corresponding src/tests/<topic>.cpp TU.
 */

#ifndef RANSHAW_SRC_TESTS_REGISTRY_H
#define RANSHAW_SRC_TESTS_REGISTRY_H

// ── field_fp.cpp ──
void test_fp();
void test_fp_sqrt();
void test_fp_sqrt_sswu();
void test_fp_extended();

// ── field_fq.cpp ──
void test_fq();
void test_fq_extended();
void test_fq_is_qr();

// ── field_fq51x2_ifma.cpp (x64 IFMA 2-way primitives) ──
void test_fq51x2_ifma();

// ── point_ran.cpp ──
void test_ran_points();
void test_ran_point_edges();

// ── point_shaw.cpp ──
void test_shaw_points();
void test_shaw_point_edges();

// ── scalar_mul.cpp ──
void test_ran_scalarmult();
void test_shaw_scalarmult();
void test_scalarmult_extended();
void test_fixed_base_scalarmult();
void test_precomputed_tables();
void test_point_to_scalar();

// ── wei25519_bridge.cpp ──
void test_wei25519();

// ── msm.cpp ──
void test_ran_msm();
void test_shaw_msm();
void test_msm_extended();
void test_msm_fixed();

// ── complete_add.cpp ──
void test_complete_add();

// ── msm_ct.cpp ──
void test_msm_ct_cross_check();

// ── sswu.cpp ──
void test_ran_sswu();
void test_shaw_sswu();

// ── batch_affine.cpp ──
void test_ran_batch_affine();
void test_shaw_batch_affine();
void test_batch_affine_extended();
void test_batch_invert();

// ── pedersen.cpp ──
void test_ran_pedersen();
void test_shaw_pedersen();
void test_pedersen_extended();

// ── polynomial.cpp ──
void test_fp_poly();
void test_fq_poly();
void test_poly_extended();
void test_poly_interpolate();
void test_karatsuba();

// ── ecfft.cpp ──
void test_ecfft();

// ── divisor.cpp ──
void test_ran_divisor();
void test_shaw_divisor();
void test_divisor_extended();
void test_eval_divisor();

// ── scalar.cpp ──
void test_ran_scalar();
void test_shaw_scalar();

// ── serialization.cpp ──
void test_serialization_edges();
void test_serialization_roundtrip();

// ── dispatch.cpp ──
void test_dispatch();
void test_cpp_api();

// ── verification.cpp ──
void test_vector_validation();
void test_vector_validation_c_primitives();

// ── edge_fields.cpp (Phase 3) ──
void test_edge_fields();

// ── malformed_decoders.cpp (Phase 3) ──
void test_malformed_decoders();

// ── concurrency.cpp (Phase 3, --concurrency-only mode) ──
void test_concurrency();

#endif // RANSHAW_SRC_TESTS_REGISTRY_H
