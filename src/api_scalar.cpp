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

// api_scalar.cpp — Implementation of HeliosScalar/SeleneScalar C++ API methods
// (serialization, deserialization with canonicality checks, inversion, wide reduction, muladd).

#include "helios_constants.h"
#include "helioselene_scalar.h"
#include "selene_constants.h"

namespace helioselene
{

    /* ---- HeliosScalar ---- */

    std::array<uint8_t, 32> HeliosScalar::to_bytes() const
    {
        std::array<uint8_t, 32> out;
        helios_scalar_to_bytes(out.data(), fe_);
        return out;
    }

    std::optional<HeliosScalar> HeliosScalar::from_bytes(const uint8_t bytes[32])
    {
        /* Reject if bit 255 is set (value >= 2^255, always out of range) */
        if (bytes[31] & 0x80)
            return std::nullopt;

        HeliosScalar s;
        helios_scalar_from_bytes(s.fe_, bytes);

        /* Reject non-canonical: round-trip must match */
        uint8_t check[32];
        helios_scalar_to_bytes(check, s.fe_);

        uint8_t diff = 0;
        for (int i = 0; i < 32; i++)
            diff |= check[i] ^ bytes[i];

        if (diff != 0)
            return std::nullopt;

        return s;
    }

    std::optional<HeliosScalar> HeliosScalar::invert() const
    {
        if (is_zero())
            return std::nullopt;

        HeliosScalar r;
        helios_scalar_invert(r.fe_, fe_);
        return r;
    }

    HeliosScalar HeliosScalar::reduce_wide(const uint8_t bytes[64])
    {
        HeliosScalar r;
        helios_scalar_reduce_wide(r.fe_, bytes);
        return r;
    }

    HeliosScalar HeliosScalar::muladd(const HeliosScalar &a, const HeliosScalar &b, const HeliosScalar &c)
    {
        HeliosScalar r;
        helios_scalar_muladd(r.fe_, a.fe_, b.fe_, c.fe_);
        return r;
    }

    /* ---- SeleneScalar ---- */

    std::array<uint8_t, 32> SeleneScalar::to_bytes() const
    {
        std::array<uint8_t, 32> out;
        selene_scalar_to_bytes(out.data(), fe_);
        return out;
    }

    std::optional<SeleneScalar> SeleneScalar::from_bytes(const uint8_t bytes[32])
    {
        if (bytes[31] & 0x80)
            return std::nullopt;

        SeleneScalar s;
        selene_scalar_from_bytes(s.fe_, bytes);

        uint8_t check[32];
        selene_scalar_to_bytes(check, s.fe_);

        uint8_t diff = 0;
        for (int i = 0; i < 32; i++)
            diff |= check[i] ^ bytes[i];

        if (diff != 0)
            return std::nullopt;

        return s;
    }

    std::optional<SeleneScalar> SeleneScalar::invert() const
    {
        if (is_zero())
            return std::nullopt;

        SeleneScalar r;
        selene_scalar_invert(r.fe_, fe_);
        return r;
    }

    SeleneScalar SeleneScalar::reduce_wide(const uint8_t bytes[64])
    {
        SeleneScalar r;
        selene_scalar_reduce_wide(r.fe_, bytes);
        return r;
    }

    SeleneScalar SeleneScalar::muladd(const SeleneScalar &a, const SeleneScalar &b, const SeleneScalar &c)
    {
        SeleneScalar r;
        selene_scalar_muladd(r.fe_, a.fe_, b.fe_, c.fe_);
        return r;
    }

    /* ---- Wei25519 bridge ---- */

    std::optional<SeleneScalar> selene_scalar_from_wei25519_x(const uint8_t x_bytes[32])
    {
        SeleneScalar s;
        if (helioselene_wei25519_to_fp(s.raw(), x_bytes) != 0)
            return std::nullopt;
        return s;
    }

} // namespace helioselene
