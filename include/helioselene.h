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
 * @brief Public C++ API for the Helioselene library.
 *
 * Helioselene is an elliptic curve library implementing the Helios/Selene
 * curve cycle for FCMP++ integration. The two curves form a cycle:
 *
 * - **Helios**: y^2 = x^3 - 3x + b over F_p (p = 2^255 - 19), group order q
 * - **Selene**: y^2 = x^3 - 3x + b over F_q (q = 2^255 - gamma), group order p
 *
 * This header provides the idiomatic C++ API with type-safe classes,
 * std::optional validation, and RAII:
 *
 * - **HeliosScalar / SeleneScalar**: Scalar field elements with arithmetic operators.
 * - **HeliosPoint / SelenePoint**: Curve points with scalar multiplication and MSM.
 * - **FpPolynomial / FqPolynomial**: Polynomial arithmetic over the base fields.
 * - **HeliosDivisor / SeleneDivisor**: EC-divisor witness computation and evaluation.
 *
 * All classes live in the `helioselene` namespace.
 *
 * For low-level C-style primitives (field elements, Jacobian coordinates,
 * raw function pointers), include helioselene_primitives.h instead.
 */

#ifndef HELIOSELENE_H
#define HELIOSELENE_H

/* Low-level C-style primitives */
#include "helioselene_primitives.h"

/* Public C++ API classes */
#include "helioselene_divisor.h"
#include "helioselene_point.h"
#include "helioselene_polynomial.h"
#include "helioselene_scalar.h"

namespace helioselene
{

    /// Initialize the library: detect CPU features and select optimal backends. Thread-safe (std::call_once).
    inline void init()
    {
        helioselene_init();
    }

    /// Benchmark all available backends and select the fastest for each dispatch slot.
    inline void autotune()
    {
        helioselene_autotune();
    }

} // namespace helioselene

#endif /* HELIOSELENE_H */
