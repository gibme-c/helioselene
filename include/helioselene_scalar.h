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
 * @file helioselene_scalar.h
 * @brief Type-safe C++ wrappers for Helios and Selene scalar field elements.
 *
 * HeliosScalar wraps fq_fe (elements of F_q, the Helios scalar field / Selene base field).
 * SeleneScalar wraps fp_fe (elements of F_p, the Selene scalar field / Helios base field).
 * This duality is the cycle property: each curve's scalar field is the other's base field.
 *
 * All arithmetic is modular and constant-time. Equality comparison uses CT XOR-accumulate.
 */

#ifndef HELIOSELENE_API_SCALAR_H
#define HELIOSELENE_API_SCALAR_H

#include "helios_scalar.h"
#include "helioselene_wei25519.h"
#include "selene_scalar.h"

#include <array>
#include <cstring>
#include <iomanip>
#include <optional>
#include <ostream>

namespace helioselene
{

    /**
     * @brief Scalar field element for the Helios curve (element of F_q).
     *
     * Represents an integer mod q where q = 2^255 - gamma (a Crandall prime, gamma ~ 2^126).
     * Internally stored as fq_fe in the active backend's representation.
     */
    class HeliosScalar
    {
      public:
        HeliosScalar()
        {
            helios_scalar_zero(fe_);
        }

        HeliosScalar(const HeliosScalar &other)
        {
            std::memcpy(fe_, other.fe_, sizeof(fq_fe));
        }

        HeliosScalar &operator=(const HeliosScalar &other)
        {
            std::memcpy(fe_, other.fe_, sizeof(fq_fe));
            return *this;
        }

        static HeliosScalar zero()
        {
            return HeliosScalar();
        }

        static HeliosScalar one()
        {
            HeliosScalar s;
            helios_scalar_one(s.fe_);
            return s;
        }

        bool is_zero() const
        {
            return helios_scalar_is_zero(fe_) != 0;
        }

        bool operator==(const HeliosScalar &other) const
        {
            auto a = to_bytes();
            auto b = other.to_bytes();
            unsigned diff = 0;
            for (size_t i = 0; i < 32; i++)
                diff |= static_cast<unsigned>(a[i] ^ b[i]);
            return diff == 0;
        }

        bool operator!=(const HeliosScalar &other) const
        {
            return !(*this == other);
        }

        HeliosScalar operator+(const HeliosScalar &other) const
        {
            HeliosScalar r;
            helios_scalar_add(r.fe_, fe_, other.fe_);
            return r;
        }

        HeliosScalar operator-(const HeliosScalar &other) const
        {
            HeliosScalar r;
            helios_scalar_sub(r.fe_, fe_, other.fe_);
            return r;
        }

        HeliosScalar operator*(const HeliosScalar &other) const
        {
            HeliosScalar r;
            helios_scalar_mul(r.fe_, fe_, other.fe_);
            return r;
        }

        HeliosScalar operator-() const
        {
            HeliosScalar r;
            helios_scalar_neg(r.fe_, fe_);
            return r;
        }

        HeliosScalar sq() const
        {
            HeliosScalar r;
            helios_scalar_sq(r.fe_, fe_);
            return r;
        }

        /// Serialize to 32-byte little-endian canonical form.
        std::array<uint8_t, 32> to_bytes() const;

        /// Deserialize from 32-byte LE. Returns nullopt if value >= q.
        static std::optional<HeliosScalar> from_bytes(const uint8_t bytes[32]);

        /// Modular inverse via Fermat's little theorem (a^{q-2} mod q). Returns nullopt for zero.
        std::optional<HeliosScalar> invert() const;

        /// Reduce a 64-byte (512-bit) wide integer mod q. Used for hash-to-scalar.
        static HeliosScalar reduce_wide(const uint8_t bytes[64]);

        /// Fused multiply-add: returns a*b + c (mod q).
        static HeliosScalar muladd(const HeliosScalar &a, const HeliosScalar &b, const HeliosScalar &c);

        /// Direct access to the underlying field element.
        const fq_fe &raw() const
        {
            return fe_;
        }

        fq_fe &raw()
        {
            return fe_;
        }

      private:
        fq_fe fe_;
    };

    /**
     * @brief Scalar field element for the Selene curve (element of F_p).
     *
     * Represents an integer mod p where p = 2^255 - 19.
     * Internally stored as fp_fe in the active backend's representation.
     */
    class SeleneScalar
    {
      public:
        SeleneScalar()
        {
            selene_scalar_zero(fe_);
        }

        SeleneScalar(const SeleneScalar &other)
        {
            std::memcpy(fe_, other.fe_, sizeof(fp_fe));
        }

        SeleneScalar &operator=(const SeleneScalar &other)
        {
            std::memcpy(fe_, other.fe_, sizeof(fp_fe));
            return *this;
        }

        static SeleneScalar zero()
        {
            return SeleneScalar();
        }

        static SeleneScalar one()
        {
            SeleneScalar s;
            selene_scalar_one(s.fe_);
            return s;
        }

        bool is_zero() const
        {
            return selene_scalar_is_zero(fe_) != 0;
        }

        bool operator==(const SeleneScalar &other) const
        {
            auto a = to_bytes();
            auto b = other.to_bytes();
            unsigned diff = 0;
            for (size_t i = 0; i < 32; i++)
                diff |= static_cast<unsigned>(a[i] ^ b[i]);
            return diff == 0;
        }

        bool operator!=(const SeleneScalar &other) const
        {
            return !(*this == other);
        }

        SeleneScalar operator+(const SeleneScalar &other) const
        {
            SeleneScalar r;
            selene_scalar_add(r.fe_, fe_, other.fe_);
            return r;
        }

        SeleneScalar operator-(const SeleneScalar &other) const
        {
            SeleneScalar r;
            selene_scalar_sub(r.fe_, fe_, other.fe_);
            return r;
        }

        SeleneScalar operator*(const SeleneScalar &other) const
        {
            SeleneScalar r;
            selene_scalar_mul(r.fe_, fe_, other.fe_);
            return r;
        }

        SeleneScalar operator-() const
        {
            SeleneScalar r;
            selene_scalar_neg(r.fe_, fe_);
            return r;
        }

        SeleneScalar sq() const
        {
            SeleneScalar r;
            selene_scalar_sq(r.fe_, fe_);
            return r;
        }

        /// Serialize to 32-byte little-endian canonical form.
        std::array<uint8_t, 32> to_bytes() const;

        /// Deserialize from 32-byte LE. Returns nullopt if value >= p.
        static std::optional<SeleneScalar> from_bytes(const uint8_t bytes[32]);

        /// Modular inverse via Fermat's little theorem (a^{p-2} mod p). Returns nullopt for zero.
        std::optional<SeleneScalar> invert() const;

        /// Reduce a 64-byte (512-bit) wide integer mod p. Used for hash-to-scalar.
        static SeleneScalar reduce_wide(const uint8_t bytes[64]);

        /// Fused multiply-add: returns a*b + c (mod p).
        static SeleneScalar muladd(const SeleneScalar &a, const SeleneScalar &b, const SeleneScalar &c);

        /// Direct access to the underlying field element.
        const fp_fe &raw() const
        {
            return fe_;
        }

        fp_fe &raw()
        {
            return fe_;
        }

      private:
        fp_fe fe_;
    };

    /// Convert a Wei25519 x-coordinate to a Selene scalar. Returns nullopt if not a valid Selene field element.
    std::optional<SeleneScalar> selene_scalar_from_wei25519_x(const uint8_t x_bytes[32]);

    inline std::ostream &operator<<(std::ostream &os, const HeliosScalar &s)
    {
        const auto bytes = s.to_bytes();
        const auto flags = os.flags();
        for (size_t i = 32; i-- > 0;)
            os << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(bytes[i]);
        os.flags(flags);
        return os;
    }

    inline std::ostream &operator<<(std::ostream &os, const SeleneScalar &s)
    {
        const auto bytes = s.to_bytes();
        const auto flags = os.flags();
        for (size_t i = 32; i-- > 0;)
            os << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(bytes[i]);
        os.flags(flags);
        return os;
    }

} // namespace helioselene

#endif // HELIOSELENE_API_SCALAR_H
