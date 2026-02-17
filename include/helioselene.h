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
 * @file helioselene.h
 * @brief Master include header for the Helioselene library.
 *
 * Helioselene is an elliptic curve library implementing the Helios/Selene
 * curve cycle for Monero's FCMP++ integration. The two curves form a cycle:
 *
 * - **Helios**: y^2 = x^3 - 3x + b over F_p (p = 2^255 - 19), group order q
 * - **Selene**: y^2 = x^3 - 3x + b over F_q (q = 2^255 - gamma), group order p
 *
 * The library is organized in layers:
 *
 * - **Field elements (fp_*, fq_*)**: Arithmetic modulo p and q respectively.
 * - **Curve points (helios_*, selene_*)**: Jacobian coordinate point operations
 *   including addition, doubling, scalar multiplication, and MSM.
 * - **Polynomials (fp_poly_*, fq_poly_*)**: EC-divisor polynomial arithmetic.
 * - **Divisors (helios_divisor_*, selene_divisor_*)**: EC-divisor witness
 *   computation and evaluation.
 *
 * Including this header pulls in everything. You can also include individual
 * headers (e.g. fp_mul.h, helios_scalarmult.h) if you only need specific ops.
 *
 * @note **This is a low-level cryptographic primitive library.** Callers must:
 *
 * 1. Validate all externally-received points via frombytes (returns error for
 *    off-curve points). Weak twist security is ~99-107 bits.
 * 2. Use constant-time scalar multiplication for secret scalars, and _vartime
 *    functions only for public data.
 * 3. Zero sensitive data after use via helioselene_secure_erase().
 */

#ifndef HELIOSELENE_H
#define HELIOSELENE_H

/* Platform detection, CPUID, dispatch, and secure erase */
#include "ct_barrier.h"
#include "helioselene_cpuid.h"
#include "helioselene_dispatch.h"
#include "helioselene_platform.h"
#include "helioselene_secure_erase.h"

/* F_p field arithmetic (p = 2^255 - 19) */
#include "fp.h"
#include "fp_cmov.h"
#include "fp_frombytes.h"
#include "fp_invert.h"
#include "fp_mul.h"
#include "fp_ops.h"
#include "fp_pow22523.h"
#include "fp_sq.h"
#include "fp_sqrt.h"
#include "fp_tobytes.h"
#include "fp_utils.h"

/* F_q field arithmetic (q = 2^255 - gamma) */
#include "fq.h"
#include "fq_cmov.h"
#include "fq_frombytes.h"
#include "fq_invert.h"
#include "fq_mul.h"
#include "fq_ops.h"
#include "fq_sq.h"
#include "fq_sqrt.h"
#include "fq_tobytes.h"
#include "fq_utils.h"

/* Helios curve operations (over F_p) */
#include "helios.h"
#include "helios_add.h"
#include "helios_batch_affine.h"
#include "helios_constants.h"
#include "helios_dbl.h"
#include "helios_frombytes.h"
#include "helios_madd.h"
#include "helios_map_to_curve.h"
#include "helios_msm_vartime.h"
#include "helios_ops.h"
#include "helios_pedersen.h"
#include "helios_scalar.h"
#include "helios_scalarmult.h"
#include "helios_scalarmult_vartime.h"
#include "helios_to_scalar.h"
#include "helios_tobytes.h"
#include "helios_validate.h"

/* Selene curve operations (over F_q) */
#include "selene.h"
#include "selene_add.h"
#include "selene_batch_affine.h"
#include "selene_constants.h"
#include "selene_dbl.h"
#include "selene_frombytes.h"
#include "selene_madd.h"
#include "selene_map_to_curve.h"
#include "selene_msm_vartime.h"
#include "selene_ops.h"
#include "selene_pedersen.h"
#include "selene_scalar.h"
#include "selene_scalarmult.h"
#include "selene_scalarmult_vartime.h"
#include "selene_to_scalar.h"
#include "selene_tobytes.h"
#include "selene_validate.h"

/* Wei25519 bridge */
#include "helioselene_wei25519.h"

/* EC-divisor polynomials and divisors */
#include "divisor.h"
#include "poly.h"

#endif /* HELIOSELENE_H */
