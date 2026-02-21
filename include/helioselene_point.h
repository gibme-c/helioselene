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
 * @file helioselene_point.h
 * @brief Type-safe C++ wrappers for Helios and Selene elliptic curve points.
 *
 * HeliosPoint and SelenePoint represent points on the Helios/Selene curve cycle
 * (y^2 = x^3 - 3x + b). Internally stored in Jacobian projective coordinates (X:Y:Z)
 * for efficient group operations. Serialization uses compressed form (32 bytes, bit 255
 * encodes y-parity).
 *
 * Constant-time scalar multiplication is available via scalar_mul(); use the _vartime
 * variants only when the scalar is public knowledge.
 */

#ifndef HELIOSELENE_API_POINT_H
#define HELIOSELENE_API_POINT_H

#include "helios_add.h"
#include "helios_constants.h"
#include "helios_dbl.h"
#include "helios_ops.h"
#include "helioselene_scalar.h"
#include "selene_add.h"
#include "selene_constants.h"
#include "selene_dbl.h"
#include "selene_ops.h"

#include <array>
#include <cstddef>
#include <iomanip>
#include <optional>
#include <ostream>

namespace helioselene
{

    /**
     * @brief Point on the Helios curve: y^2 = x^3 - 3x + b over F_p (p = 2^255 - 19).
     *
     * Group order is q (the Selene base field prime). Cofactor 1.
     * Internally stored as Jacobian coordinates (X:Y:Z) where affine x = X/Z^2, y = Y/Z^3.
     */
    class HeliosPoint
    {
      public:
        HeliosPoint()
        {
            helios_identity(&jac_);
        }

        HeliosPoint(const HeliosPoint &other)
        {
            helios_copy(&jac_, &other.jac_);
        }

        HeliosPoint &operator=(const HeliosPoint &other)
        {
            helios_copy(&jac_, &other.jac_);
            return *this;
        }

        static HeliosPoint identity()
        {
            return HeliosPoint();
        }

        static HeliosPoint generator()
        {
            HeliosPoint p;
            fp_copy(p.jac_.X, HELIOS_GX);
            fp_copy(p.jac_.Y, HELIOS_GY);
            fp_1(p.jac_.Z);
            return p;
        }

        bool is_identity() const
        {
            return helios_is_identity(&jac_) != 0;
        }

        HeliosPoint operator-() const
        {
            HeliosPoint r;
            helios_neg(&r.jac_, &jac_);
            return r;
        }

        HeliosPoint operator+(const HeliosPoint &other) const
        {
            if (is_identity())
                return other;
            if (other.is_identity())
                return *this;
            /* Check if x-coordinates match (projective: X1*Z2^2 == X2*Z1^2) */
            fp_fe z1z1, z2z2, u1, u2, diff;
            fp_sq(z1z1, jac_.Z);
            fp_sq(z2z2, other.jac_.Z);
            fp_mul(u1, jac_.X, z2z2);
            fp_mul(u2, other.jac_.X, z1z1);
            fp_sub(diff, u1, u2);
            if (!fp_isnonzero(diff))
            {
                fp_fe s1, s2, t;
                fp_mul(t, other.jac_.Z, z2z2);
                fp_mul(s1, jac_.Y, t);
                fp_mul(t, jac_.Z, z1z1);
                fp_mul(s2, other.jac_.Y, t);
                fp_sub(diff, s1, s2);
                if (!fp_isnonzero(diff))
                    return dbl(); /* P == P */
                return identity(); /* P == -P */
            }
            HeliosPoint r;
            helios_add(&r.jac_, &jac_, &other.jac_);
            return r;
        }

        HeliosPoint dbl() const
        {
            HeliosPoint r;
            helios_dbl(&r.jac_, &jac_);
            return r;
        }

        /// Decompress from 32-byte encoding. Returns nullopt if not a valid on-curve point.
        static std::optional<HeliosPoint> from_bytes(const uint8_t bytes[32]);

        /// Compress to 32 bytes (x-coordinate LE, bit 255 = y parity).
        std::array<uint8_t, 32> to_bytes() const;

        /// Return just the x-coordinate as 32-byte LE (no y-parity bit).
        std::array<uint8_t, 32> x_coordinate_bytes() const;

        /// Constant-time scalar multiplication using signed 4-bit windowed method.
        HeliosPoint scalar_mul(const HeliosScalar &s) const;

        /// Variable-time scalar multiplication using wNAF w=5. Only use with public scalars.
        HeliosPoint scalar_mul_vartime(const HeliosScalar &s) const;

        /// Multi-scalar multiplication: sum(scalars[i] * points[i]). Uses Straus (n<=32) or Pippenger.
        static HeliosPoint multi_scalar_mul(const HeliosScalar *scalars, const HeliosPoint *points, size_t n);

        /// Pedersen commitment: blinding*H + sum(values[i]*generators[i]).
        static HeliosPoint pedersen_commit(
            const HeliosScalar &blinding,
            const HeliosPoint &H,
            const HeliosScalar *values,
            const HeliosPoint *generators,
            size_t n);

        /// Hash-to-curve (single field element) via RFC 9380 Simplified SWU.
        static HeliosPoint map_to_curve(const uint8_t u[32]);

        /// Hash-to-curve (two field elements, full RFC 9380 encode-to-curve).
        static HeliosPoint map_to_curve(const uint8_t u0[32], const uint8_t u1[32]);

        const helios_jacobian &raw() const
        {
            return jac_;
        }

        helios_jacobian &raw()
        {
            return jac_;
        }

      private:
        helios_jacobian jac_;
    };

    /**
     * @brief Point on the Selene curve: y^2 = x^3 - 3x + b over F_q (q = 2^255 - gamma).
     *
     * Group order is p (the Helios base field prime, 2^255 - 19). Cofactor 1.
     * Internally stored as Jacobian coordinates (X:Y:Z) where affine x = X/Z^2, y = Y/Z^3.
     */
    class SelenePoint
    {
      public:
        SelenePoint()
        {
            selene_identity(&jac_);
        }

        SelenePoint(const SelenePoint &other)
        {
            selene_copy(&jac_, &other.jac_);
        }

        SelenePoint &operator=(const SelenePoint &other)
        {
            selene_copy(&jac_, &other.jac_);
            return *this;
        }

        static SelenePoint identity()
        {
            return SelenePoint();
        }

        static SelenePoint generator()
        {
            SelenePoint p;
            fq_copy(p.jac_.X, SELENE_GX);
            fq_copy(p.jac_.Y, SELENE_GY);
            fq_1(p.jac_.Z);
            return p;
        }

        bool is_identity() const
        {
            return selene_is_identity(&jac_) != 0;
        }

        SelenePoint operator-() const
        {
            SelenePoint r;
            selene_neg(&r.jac_, &jac_);
            return r;
        }

        SelenePoint operator+(const SelenePoint &other) const
        {
            if (is_identity())
                return other;
            if (other.is_identity())
                return *this;
            fq_fe z1z1, z2z2, u1, u2, diff;
            fq_sq(z1z1, jac_.Z);
            fq_sq(z2z2, other.jac_.Z);
            fq_mul(u1, jac_.X, z2z2);
            fq_mul(u2, other.jac_.X, z1z1);
            fq_sub(diff, u1, u2);
            if (!fq_isnonzero(diff))
            {
                fq_fe s1, s2, t;
                fq_mul(t, other.jac_.Z, z2z2);
                fq_mul(s1, jac_.Y, t);
                fq_mul(t, jac_.Z, z1z1);
                fq_mul(s2, other.jac_.Y, t);
                fq_sub(diff, s1, s2);
                if (!fq_isnonzero(diff))
                    return dbl();
                return identity();
            }
            SelenePoint r;
            selene_add(&r.jac_, &jac_, &other.jac_);
            return r;
        }

        SelenePoint dbl() const
        {
            SelenePoint r;
            selene_dbl(&r.jac_, &jac_);
            return r;
        }

        /// Decompress from 32-byte encoding. Returns nullopt if not a valid on-curve point.
        static std::optional<SelenePoint> from_bytes(const uint8_t bytes[32]);

        /// Compress to 32 bytes (x-coordinate LE, bit 255 = y parity).
        std::array<uint8_t, 32> to_bytes() const;

        /// Return just the x-coordinate as 32-byte LE (no y-parity bit).
        std::array<uint8_t, 32> x_coordinate_bytes() const;

        /// Constant-time scalar multiplication using signed 4-bit windowed method.
        SelenePoint scalar_mul(const SeleneScalar &s) const;

        /// Variable-time scalar multiplication using wNAF w=5. Only use with public scalars.
        SelenePoint scalar_mul_vartime(const SeleneScalar &s) const;

        /// Multi-scalar multiplication: sum(scalars[i] * points[i]). Uses Straus (n<=32) or Pippenger.
        static SelenePoint multi_scalar_mul(const SeleneScalar *scalars, const SelenePoint *points, size_t n);

        /// Pedersen commitment: blinding*H + sum(values[i]*generators[i]).
        static SelenePoint pedersen_commit(
            const SeleneScalar &blinding,
            const SelenePoint &H,
            const SeleneScalar *values,
            const SelenePoint *generators,
            size_t n);

        /// Hash-to-curve (single field element) via RFC 9380 Simplified SWU.
        static SelenePoint map_to_curve(const uint8_t u[32]);

        /// Hash-to-curve (two field elements, full RFC 9380 encode-to-curve).
        static SelenePoint map_to_curve(const uint8_t u0[32], const uint8_t u1[32]);

        const selene_jacobian &raw() const
        {
            return jac_;
        }

        selene_jacobian &raw()
        {
            return jac_;
        }

      private:
        selene_jacobian jac_;
    };

    inline std::ostream &operator<<(std::ostream &os, const HeliosPoint &p)
    {
        const auto bytes = p.to_bytes();
        const auto flags = os.flags();
        for (int i = 31; i >= 0; --i)
            os << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(bytes[i]);
        os.flags(flags);
        return os;
    }

    inline std::ostream &operator<<(std::ostream &os, const SelenePoint &p)
    {
        const auto bytes = p.to_bytes();
        const auto flags = os.flags();
        for (int i = 31; i >= 0; --i)
            os << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(bytes[i]);
        os.flags(flags);
        return os;
    }

} // namespace helioselene

#endif // HELIOSELENE_API_POINT_H
