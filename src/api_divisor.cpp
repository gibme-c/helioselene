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

// api_divisor.cpp — Implementation of HeliosDivisor/SeleneDivisor C++ API methods.
// Batch-converts Jacobian points to affine before computing the divisor witness.

#include "fp_frombytes.h"
#include "fp_tobytes.h"
#include "fq_frombytes.h"
#include "fq_tobytes.h"
#include "helios_batch_affine.h"
#include "helioselene_divisor.h"
#include "selene_batch_affine.h"

#include <vector>

namespace helioselene
{

    /* Upper bound on divisor size: 1M points. Prevents unbounded allocations. */
    static constexpr size_t MAX_DIVISOR_SIZE = 1u << 20;

    /* ---- HeliosDivisor ---- */

    void HeliosDivisor::sync_wrappers()
    {
        a_.raw() = div_.a;
        b_.raw() = div_.b;
    }

    HeliosDivisor HeliosDivisor::compute(const HeliosPoint *points, size_t n)
    {
        if (n == 0 || !points || n > MAX_DIVISOR_SIZE)
            return HeliosDivisor();

        std::vector<helios_jacobian> jac(n);
        for (size_t i = 0; i < n; i++)
            helios_copy(&jac[i], &points[i].raw());

        std::vector<helios_affine> aff(n);
        helios_batch_to_affine(aff.data(), jac.data(), n);

        HeliosDivisor d;
        helios_compute_divisor(&d.div_, aff.data(), n);
        d.sync_wrappers();
        return d;
    }

    std::array<uint8_t, 32> HeliosDivisor::evaluate(const uint8_t x_bytes[32], const uint8_t y_bytes[32]) const
    {
        fp_fe x, y, result;
        fp_frombytes(x, x_bytes);
        fp_frombytes(y, y_bytes);
        helios_evaluate_divisor(result, &div_, x, y);

        std::array<uint8_t, 32> out;
        fp_tobytes(out.data(), result);
        return out;
    }

    /* ---- SeleneDivisor ---- */

    void SeleneDivisor::sync_wrappers()
    {
        a_.raw() = div_.a;
        b_.raw() = div_.b;
    }

    SeleneDivisor SeleneDivisor::compute(const SelenePoint *points, size_t n)
    {
        if (n == 0 || !points || n > MAX_DIVISOR_SIZE)
            return SeleneDivisor();

        std::vector<selene_jacobian> jac(n);
        for (size_t i = 0; i < n; i++)
            selene_copy(&jac[i], &points[i].raw());

        std::vector<selene_affine> aff(n);
        selene_batch_to_affine(aff.data(), jac.data(), n);

        SeleneDivisor d;
        selene_compute_divisor(&d.div_, aff.data(), n);
        d.sync_wrappers();
        return d;
    }

    std::array<uint8_t, 32> SeleneDivisor::evaluate(const uint8_t x_bytes[32], const uint8_t y_bytes[32]) const
    {
        fq_fe x, y, result;
        fq_frombytes(x, x_bytes);
        fq_frombytes(y, y_bytes);
        selene_evaluate_divisor(result, &div_, x, y);

        std::array<uint8_t, 32> out;
        fq_tobytes(out.data(), result);
        return out;
    }

} // namespace helioselene
