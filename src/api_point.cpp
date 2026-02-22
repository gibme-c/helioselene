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

// api_point.cpp — Implementation of HeliosPoint/SelenePoint C++ API methods
// (serialization, scalar multiplication, MSM, Pedersen commit, hash-to-curve).

#include "helios_frombytes.h"
#include "helios_map_to_curve.h"
#include "helios_msm_vartime.h"
#include "helios_pedersen.h"
#include "helios_scalarmult.h"
#include "helios_scalarmult_vartime.h"
#include "helios_to_scalar.h"
#include "helios_tobytes.h"
#include "helioselene_point.h"
#include "selene_frombytes.h"
#include "selene_map_to_curve.h"
#include "selene_msm_vartime.h"
#include "selene_pedersen.h"
#include "selene_scalarmult.h"
#include "selene_scalarmult_vartime.h"
#include "selene_to_scalar.h"
#include "selene_tobytes.h"

#include <climits>
#include <cstdint>
#include <vector>

namespace helioselene
{

    /* ---- HeliosPoint ---- */

    std::optional<HeliosPoint> HeliosPoint::from_bytes(const uint8_t bytes[32])
    {
        HeliosPoint p;
        if (helios_frombytes(&p.jac_, bytes) != 0)
            return std::nullopt;
        return p;
    }

    std::array<uint8_t, 32> HeliosPoint::to_bytes() const
    {
        std::array<uint8_t, 32> out;
        helios_tobytes(out.data(), &jac_);
        return out;
    }

    std::array<uint8_t, 32> HeliosPoint::x_coordinate_bytes() const
    {
        std::array<uint8_t, 32> out;
        helios_point_to_bytes(out.data(), &jac_);
        return out;
    }

    HeliosPoint HeliosPoint::scalar_mul(const HeliosScalar &s) const
    {
        auto sb = s.to_bytes();
        HeliosPoint r;
        helios_scalarmult(&r.jac_, sb.data(), &jac_);
        return r;
    }

    HeliosPoint HeliosPoint::scalar_mul_vartime(const HeliosScalar &s) const
    {
        auto sb = s.to_bytes();
        HeliosPoint r;
        helios_scalarmult_vartime(&r.jac_, sb.data(), &jac_);
        return r;
    }

    HeliosPoint HeliosPoint::multi_scalar_mul(const HeliosScalar *scalars, const HeliosPoint *points, size_t n)
    {
        if (n == 0 || !scalars || !points || n > SIZE_MAX / 32)
            return HeliosPoint();

        std::vector<unsigned char> scalar_bytes(32 * n);
        std::vector<helios_jacobian> jac_points(n);

        for (size_t i = 0; i < n; i++)
        {
            auto sb = scalars[i].to_bytes();
            std::memcpy(scalar_bytes.data() + 32 * i, sb.data(), 32);
            helios_copy(&jac_points[i], &points[i].raw());
        }

        HeliosPoint r;
        helios_msm_vartime(&r.jac_, scalar_bytes.data(), jac_points.data(), n);
        return r;
    }

    HeliosPoint HeliosPoint::pedersen_commit(
        const HeliosScalar &blinding,
        const HeliosPoint &H,
        const HeliosScalar *values,
        const HeliosPoint *generators,
        size_t n)
    {
        if (n == 0 || !values || !generators || n > SIZE_MAX / 32)
            return HeliosPoint();

        auto blind_bytes = blinding.to_bytes();
        std::vector<unsigned char> val_bytes(32 * n);
        std::vector<helios_jacobian> gen_points(n);

        for (size_t i = 0; i < n; i++)
        {
            auto vb = values[i].to_bytes();
            std::memcpy(val_bytes.data() + 32 * i, vb.data(), 32);
            helios_copy(&gen_points[i], &generators[i].raw());
        }

        HeliosPoint r;
        helios_pedersen_commit(&r.jac_, blind_bytes.data(), &H.raw(), val_bytes.data(), gen_points.data(), n);
        return r;
    }

    HeliosPoint HeliosPoint::map_to_curve(const uint8_t u[32])
    {
        HeliosPoint r;
        helios_map_to_curve(&r.jac_, u);
        return r;
    }

    HeliosPoint HeliosPoint::map_to_curve(const uint8_t u0[32], const uint8_t u1[32])
    {
        HeliosPoint r;
        helios_map_to_curve2(&r.jac_, u0, u1);
        return r;
    }

    /* ---- SelenePoint ---- */

    std::optional<SelenePoint> SelenePoint::from_bytes(const uint8_t bytes[32])
    {
        SelenePoint p;
        if (selene_frombytes(&p.jac_, bytes) != 0)
            return std::nullopt;
        return p;
    }

    std::array<uint8_t, 32> SelenePoint::to_bytes() const
    {
        std::array<uint8_t, 32> out;
        selene_tobytes(out.data(), &jac_);
        return out;
    }

    std::array<uint8_t, 32> SelenePoint::x_coordinate_bytes() const
    {
        std::array<uint8_t, 32> out;
        selene_point_to_bytes(out.data(), &jac_);
        return out;
    }

    SelenePoint SelenePoint::scalar_mul(const SeleneScalar &s) const
    {
        auto sb = s.to_bytes();
        SelenePoint r;
        selene_scalarmult(&r.jac_, sb.data(), &jac_);
        return r;
    }

    SelenePoint SelenePoint::scalar_mul_vartime(const SeleneScalar &s) const
    {
        auto sb = s.to_bytes();
        SelenePoint r;
        selene_scalarmult_vartime(&r.jac_, sb.data(), &jac_);
        return r;
    }

    SelenePoint SelenePoint::multi_scalar_mul(const SeleneScalar *scalars, const SelenePoint *points, size_t n)
    {
        if (n == 0 || !scalars || !points || n > SIZE_MAX / 32)
            return SelenePoint();

        std::vector<unsigned char> scalar_bytes(32 * n);
        std::vector<selene_jacobian> jac_points(n);

        for (size_t i = 0; i < n; i++)
        {
            auto sb = scalars[i].to_bytes();
            std::memcpy(scalar_bytes.data() + 32 * i, sb.data(), 32);
            selene_copy(&jac_points[i], &points[i].raw());
        }

        SelenePoint r;
        selene_msm_vartime(&r.jac_, scalar_bytes.data(), jac_points.data(), n);
        return r;
    }

    SelenePoint SelenePoint::pedersen_commit(
        const SeleneScalar &blinding,
        const SelenePoint &H,
        const SeleneScalar *values,
        const SelenePoint *generators,
        size_t n)
    {
        if (n == 0 || !values || !generators || n > SIZE_MAX / 32)
            return SelenePoint();

        auto blind_bytes = blinding.to_bytes();
        std::vector<unsigned char> val_bytes(32 * n);
        std::vector<selene_jacobian> gen_points(n);

        for (size_t i = 0; i < n; i++)
        {
            auto vb = values[i].to_bytes();
            std::memcpy(val_bytes.data() + 32 * i, vb.data(), 32);
            selene_copy(&gen_points[i], &generators[i].raw());
        }

        SelenePoint r;
        selene_pedersen_commit(&r.jac_, blind_bytes.data(), &H.raw(), val_bytes.data(), gen_points.data(), n);
        return r;
    }

    SelenePoint SelenePoint::map_to_curve(const uint8_t u[32])
    {
        SelenePoint r;
        selene_map_to_curve(&r.jac_, u);
        return r;
    }

    SelenePoint SelenePoint::map_to_curve(const uint8_t u0[32], const uint8_t u1[32])
    {
        SelenePoint r;
        selene_map_to_curve2(&r.jac_, u0, u1);
        return r;
    }

} // namespace helioselene
