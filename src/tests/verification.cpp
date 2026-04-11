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

#include "tests/common.h"
#include "tests/registry.h"

void test_vector_validation()
{
    using namespace ranshaw;
    namespace tv = ranshaw_test_vectors;

    std::cout << std::endl << "=== Test Vector Validation ===" << std::endl;

    /* ---- Ran Scalar ---- */
    std::cout << "  --- Ran Scalar ---" << std::endl;
    for (size_t i = 0; i < tv::ran_scalar::from_bytes_count; i++)
    {
        auto &v = tv::ran_scalar::from_bytes_vectors[i];
        auto r = RanScalar::from_bytes(v.input);
        std::string name = std::string("tv: ran scalar from_bytes ") + v.label;
        if (v.valid)
        {
            check_int((name + " valid").c_str(), 1, r.has_value() ? 1 : 0);
            if (r)
                check_bytes((name + " value").c_str(), v.result, r->to_bytes().data(), 32);
        }
        else
        {
            check_int((name + " invalid").c_str(), 0, r.has_value() ? 1 : 0);
        }
    }
    for (size_t i = 0; i < tv::ran_scalar::add_count; i++)
    {
        auto &v = tv::ran_scalar::add_vectors[i];
        auto a = RanScalar::from_bytes(v.a).value();
        auto b = RanScalar::from_bytes(v.b).value();
        auto r = a + b;
        check_bytes((std::string("tv: ran scalar add ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::sub_count; i++)
    {
        auto &v = tv::ran_scalar::sub_vectors[i];
        auto a = RanScalar::from_bytes(v.a).value();
        auto b = RanScalar::from_bytes(v.b).value();
        auto r = a - b;
        check_bytes((std::string("tv: ran scalar sub ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::mul_count; i++)
    {
        auto &v = tv::ran_scalar::mul_vectors[i];
        auto a = RanScalar::from_bytes(v.a).value();
        auto b = RanScalar::from_bytes(v.b).value();
        auto r = a * b;
        check_bytes((std::string("tv: ran scalar mul ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::sq_count; i++)
    {
        auto &v = tv::ran_scalar::sq_vectors[i];
        auto a = RanScalar::from_bytes(v.a).value();
        auto r = a.sq();
        check_bytes((std::string("tv: ran scalar sq ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::negate_count; i++)
    {
        auto &v = tv::ran_scalar::negate_vectors[i];
        auto a = RanScalar::from_bytes(v.a).value();
        auto r = -a;
        check_bytes((std::string("tv: ran scalar neg ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::invert_count; i++)
    {
        auto &v = tv::ran_scalar::invert_vectors[i];
        auto a = RanScalar::from_bytes(v.a).value();
        auto r = a.invert();
        std::string name = std::string("tv: ran scalar inv ") + v.label;
        if (v.valid)
        {
            check_int((name + " valid").c_str(), 1, r.has_value() ? 1 : 0);
            if (r)
                check_bytes((name + " value").c_str(), v.result, r->to_bytes().data(), 32);
        }
        else
        {
            check_int((name + " invalid").c_str(), 0, r.has_value() ? 1 : 0);
        }
    }
    for (size_t i = 0; i < tv::ran_scalar::reduce_wide_count; i++)
    {
        auto &v = tv::ran_scalar::reduce_wide_vectors[i];
        auto r = RanScalar::reduce_wide(v.input);
        check_bytes((std::string("tv: ran scalar reduce_wide ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::muladd_count; i++)
    {
        auto &v = tv::ran_scalar::muladd_vectors[i];
        auto a = RanScalar::from_bytes(v.a).value();
        auto b = RanScalar::from_bytes(v.b).value();
        auto c = RanScalar::from_bytes(v.c).value();
        auto r = RanScalar::muladd(a, b, c);
        check_bytes((std::string("tv: ran scalar muladd ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::is_zero_count; i++)
    {
        auto &v = tv::ran_scalar::is_zero_vectors[i];
        auto a = RanScalar::from_bytes(v.a).value();
        check_int((std::string("tv: ran scalar is_zero ") + v.label).c_str(), v.result ? 1 : 0, a.is_zero() ? 1 : 0);
    }

    /* ---- Shaw Scalar ---- */
    std::cout << "  --- Shaw Scalar ---" << std::endl;
    for (size_t i = 0; i < tv::shaw_scalar::from_bytes_count; i++)
    {
        auto &v = tv::shaw_scalar::from_bytes_vectors[i];
        auto r = ShawScalar::from_bytes(v.input);
        std::string name = std::string("tv: shaw scalar from_bytes ") + v.label;
        if (v.valid)
        {
            check_int((name + " valid").c_str(), 1, r.has_value() ? 1 : 0);
            if (r)
                check_bytes((name + " value").c_str(), v.result, r->to_bytes().data(), 32);
        }
        else
        {
            check_int((name + " invalid").c_str(), 0, r.has_value() ? 1 : 0);
        }
    }
    for (size_t i = 0; i < tv::shaw_scalar::add_count; i++)
    {
        auto &v = tv::shaw_scalar::add_vectors[i];
        auto a = ShawScalar::from_bytes(v.a).value();
        auto b = ShawScalar::from_bytes(v.b).value();
        auto r = a + b;
        check_bytes((std::string("tv: shaw scalar add ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::sub_count; i++)
    {
        auto &v = tv::shaw_scalar::sub_vectors[i];
        auto a = ShawScalar::from_bytes(v.a).value();
        auto b = ShawScalar::from_bytes(v.b).value();
        auto r = a - b;
        check_bytes((std::string("tv: shaw scalar sub ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::mul_count; i++)
    {
        auto &v = tv::shaw_scalar::mul_vectors[i];
        auto a = ShawScalar::from_bytes(v.a).value();
        auto b = ShawScalar::from_bytes(v.b).value();
        auto r = a * b;
        check_bytes((std::string("tv: shaw scalar mul ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::sq_count; i++)
    {
        auto &v = tv::shaw_scalar::sq_vectors[i];
        auto a = ShawScalar::from_bytes(v.a).value();
        auto r = a.sq();
        check_bytes((std::string("tv: shaw scalar sq ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::negate_count; i++)
    {
        auto &v = tv::shaw_scalar::negate_vectors[i];
        auto a = ShawScalar::from_bytes(v.a).value();
        auto r = -a;
        check_bytes((std::string("tv: shaw scalar neg ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::invert_count; i++)
    {
        auto &v = tv::shaw_scalar::invert_vectors[i];
        auto a = ShawScalar::from_bytes(v.a).value();
        auto r = a.invert();
        std::string name = std::string("tv: shaw scalar inv ") + v.label;
        if (v.valid)
        {
            check_int((name + " valid").c_str(), 1, r.has_value() ? 1 : 0);
            if (r)
                check_bytes((name + " value").c_str(), v.result, r->to_bytes().data(), 32);
        }
        else
        {
            check_int((name + " invalid").c_str(), 0, r.has_value() ? 1 : 0);
        }
    }
    for (size_t i = 0; i < tv::shaw_scalar::reduce_wide_count; i++)
    {
        auto &v = tv::shaw_scalar::reduce_wide_vectors[i];
        auto r = ShawScalar::reduce_wide(v.input);
        check_bytes((std::string("tv: shaw scalar reduce_wide ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::muladd_count; i++)
    {
        auto &v = tv::shaw_scalar::muladd_vectors[i];
        auto a = ShawScalar::from_bytes(v.a).value();
        auto b = ShawScalar::from_bytes(v.b).value();
        auto c = ShawScalar::from_bytes(v.c).value();
        auto r = ShawScalar::muladd(a, b, c);
        check_bytes((std::string("tv: shaw scalar muladd ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::is_zero_count; i++)
    {
        auto &v = tv::shaw_scalar::is_zero_vectors[i];
        auto a = ShawScalar::from_bytes(v.a).value();
        check_int((std::string("tv: shaw scalar is_zero ") + v.label).c_str(), v.result ? 1 : 0, a.is_zero() ? 1 : 0);
    }

    /* ---- Ran Point ---- */
    std::cout << "  --- Ran Point ---" << std::endl;
    for (size_t i = 0; i < tv::ran_point::from_bytes_count; i++)
    {
        auto &v = tv::ran_point::from_bytes_vectors[i];
        auto r = RanPoint::from_bytes(v.input);
        std::string name = std::string("tv: ran point from_bytes ") + v.label;
        if (v.valid)
        {
            check_int((name + " valid").c_str(), 1, r.has_value() ? 1 : 0);
            if (r)
                check_bytes((name + " value").c_str(), v.result, r->to_bytes().data(), 32);
        }
        else
        {
            check_int((name + " invalid").c_str(), 0, r.has_value() ? 1 : 0);
        }
    }
    {
        auto hp_from = [](const uint8_t bytes[32]) -> RanPoint
        {
            auto r = RanPoint::from_bytes(bytes);
            return r ? *r : RanPoint::identity();
        };
        for (size_t i = 0; i < tv::ran_point::add_count; i++)
        {
            auto &v = tv::ran_point::add_vectors[i];
            auto a = hp_from(v.a);
            auto b = hp_from(v.b);
            auto r = a + b;
            check_bytes((std::string("tv: ran point add ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
        }
    }
    {
        auto hp_from = [](const uint8_t bytes[32]) -> RanPoint
        {
            auto r = RanPoint::from_bytes(bytes);
            return r ? *r : RanPoint::identity();
        };
        for (size_t i = 0; i < tv::ran_point::dbl_count; i++)
        {
            auto &v = tv::ran_point::dbl_vectors[i];
            auto a = hp_from(v.a);
            auto r = a.dbl();
            check_bytes((std::string("tv: ran point dbl ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
        }
        for (size_t i = 0; i < tv::ran_point::negate_count; i++)
        {
            auto &v = tv::ran_point::negate_vectors[i];
            auto a = hp_from(v.a);
            auto r = -a;
            check_bytes((std::string("tv: ran point neg ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
        }
    }
    {
        auto hp_from = [](const uint8_t bytes[32]) -> RanPoint
        {
            auto r = RanPoint::from_bytes(bytes);
            return r ? *r : RanPoint::identity();
        };
        for (size_t i = 0; i < tv::ran_point::scalar_mul_count; i++)
        {
            auto &v = tv::ran_point::scalar_mul_vectors[i];
            auto s = RanScalar::from_bytes(v.scalar).value();
            auto p = hp_from(v.point);
            auto r = p.scalar_mul(s);
            check_bytes(
                (std::string("tv: ran point scalar_mul ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
        }
    }
    /* MSM */
    {
        auto test_msm = [](const char *label,
                           size_t n,
                           const uint8_t scalars[][32],
                           const uint8_t points[][32],
                           const uint8_t expected[32])
        {
            std::vector<RanScalar> sv;
            std::vector<RanPoint> pv;
            for (size_t i = 0; i < n; i++)
            {
                sv.push_back(RanScalar::from_bytes(scalars[i]).value());
                pv.push_back(RanPoint::from_bytes(points[i]).value());
            }
            auto r = RanPoint::multi_scalar_mul(sv.data(), pv.data(), n);
            check_bytes(label, expected, r.to_bytes().data(), 32);
        };
        test_msm(
            "tv: ran msm n_1",
            1,
            tv::ran_point::msm_n_1_scalars,
            tv::ran_point::msm_n_1_points,
            tv::ran_point::msm_n_1_result);
        test_msm(
            "tv: ran msm n_2",
            2,
            tv::ran_point::msm_n_2_scalars,
            tv::ran_point::msm_n_2_points,
            tv::ran_point::msm_n_2_result);
        test_msm(
            "tv: ran msm n_4",
            4,
            tv::ran_point::msm_n_4_scalars,
            tv::ran_point::msm_n_4_points,
            tv::ran_point::msm_n_4_result);
        test_msm(
            "tv: ran msm n_16",
            16,
            tv::ran_point::msm_n_16_scalars,
            tv::ran_point::msm_n_16_points,
            tv::ran_point::msm_n_16_result);
        test_msm(
            "tv: ran msm n_32_straus",
            32,
            tv::ran_point::msm_n_32_straus_scalars,
            tv::ran_point::msm_n_32_straus_points,
            tv::ran_point::msm_n_32_straus_result);
        test_msm(
            "tv: ran msm n_33_pippenger",
            33,
            tv::ran_point::msm_n_33_pippenger_scalars,
            tv::ran_point::msm_n_33_pippenger_points,
            tv::ran_point::msm_n_33_pippenger_result);
        test_msm(
            "tv: ran msm n_64_pippenger",
            64,
            tv::ran_point::msm_n_64_pippenger_scalars,
            tv::ran_point::msm_n_64_pippenger_points,
            tv::ran_point::msm_n_64_pippenger_result);
    }
    /* Pedersen */
    {
        auto test_ped = [](const char *label,
                           const uint8_t blinding[32],
                           const uint8_t H[32],
                           size_t n,
                           const uint8_t values[][32],
                           const uint8_t generators[][32],
                           const uint8_t expected[32])
        {
            auto s_blind = RanScalar::from_bytes(blinding).value();
            auto p_H = RanPoint::from_bytes(H).value();
            std::vector<RanScalar> vals;
            std::vector<RanPoint> gens;
            for (size_t i = 0; i < n; i++)
            {
                vals.push_back(RanScalar::from_bytes(values[i]).value());
                gens.push_back(RanPoint::from_bytes(generators[i]).value());
            }
            auto r = RanPoint::pedersen_commit(s_blind, p_H, vals.data(), gens.data(), n);
            check_bytes(label, expected, r.to_bytes().data(), 32);
        };
        test_ped(
            "tv: ran pedersen n_1",
            tv::ran_point::pedersen_n_1_blinding,
            tv::ran_point::pedersen_n_1_H,
            1,
            tv::ran_point::pedersen_n_1_values,
            tv::ran_point::pedersen_n_1_generators,
            tv::ran_point::pedersen_n_1_result);
        test_ped(
            "tv: ran pedersen blinding_zero",
            tv::ran_point::pedersen_blinding_zero_blinding,
            tv::ran_point::pedersen_blinding_zero_H,
            1,
            tv::ran_point::pedersen_blinding_zero_values,
            tv::ran_point::pedersen_blinding_zero_generators,
            tv::ran_point::pedersen_blinding_zero_result);
        test_ped(
            "tv: ran pedersen n_4",
            tv::ran_point::pedersen_n_4_blinding,
            tv::ran_point::pedersen_n_4_H,
            4,
            tv::ran_point::pedersen_n_4_values,
            tv::ran_point::pedersen_n_4_generators,
            tv::ran_point::pedersen_n_4_result);
    }
    for (size_t i = 0; i < tv::ran_point::map_to_curve_single_count; i++)
    {
        auto &v = tv::ran_point::map_to_curve_single_vectors[i];
        auto r = RanPoint::map_to_curve(v.u);
        check_bytes((std::string("tv: ran point map_to_curve ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::ran_point::map_to_curve_double_count; i++)
    {
        auto &v = tv::ran_point::map_to_curve_double_vectors[i];
        auto r = RanPoint::map_to_curve(v.u0, v.u1);
        check_bytes((std::string("tv: ran point map_to_curve2 ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::ran_point::x_coordinate_count; i++)
    {
        auto &v = tv::ran_point::x_coordinate_vectors[i];
        auto p = RanPoint::from_bytes(v.point).value();
        auto r = p.x_coordinate_bytes();
        check_bytes((std::string("tv: ran point x_coord ") + v.label).c_str(), v.x_bytes, r.data(), 32);
    }

    /* ---- Shaw Point ---- */
    std::cout << "  --- Shaw Point ---" << std::endl;
    for (size_t i = 0; i < tv::shaw_point::from_bytes_count; i++)
    {
        auto &v = tv::shaw_point::from_bytes_vectors[i];
        auto r = ShawPoint::from_bytes(v.input);
        std::string name = std::string("tv: shaw point from_bytes ") + v.label;
        if (v.valid)
        {
            check_int((name + " valid").c_str(), 1, r.has_value() ? 1 : 0);
            if (r)
                check_bytes((name + " value").c_str(), v.result, r->to_bytes().data(), 32);
        }
        else
        {
            check_int((name + " invalid").c_str(), 0, r.has_value() ? 1 : 0);
        }
    }
    {
        auto sp_from = [](const uint8_t bytes[32]) -> ShawPoint
        {
            auto r = ShawPoint::from_bytes(bytes);
            return r ? *r : ShawPoint::identity();
        };
        for (size_t i = 0; i < tv::shaw_point::add_count; i++)
        {
            auto &v = tv::shaw_point::add_vectors[i];
            auto a = sp_from(v.a);
            auto b = sp_from(v.b);
            auto r = a + b;
            check_bytes((std::string("tv: shaw point add ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
        }
    }
    {
        auto sp_from = [](const uint8_t bytes[32]) -> ShawPoint
        {
            auto r = ShawPoint::from_bytes(bytes);
            return r ? *r : ShawPoint::identity();
        };
        for (size_t i = 0; i < tv::shaw_point::dbl_count; i++)
        {
            auto &v = tv::shaw_point::dbl_vectors[i];
            auto a = sp_from(v.a);
            auto r = a.dbl();
            check_bytes((std::string("tv: shaw point dbl ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
        }
        for (size_t i = 0; i < tv::shaw_point::negate_count; i++)
        {
            auto &v = tv::shaw_point::negate_vectors[i];
            auto a = sp_from(v.a);
            auto r = -a;
            check_bytes((std::string("tv: shaw point neg ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
        }
    }
    {
        auto sp_from = [](const uint8_t bytes[32]) -> ShawPoint
        {
            auto r = ShawPoint::from_bytes(bytes);
            return r ? *r : ShawPoint::identity();
        };
        for (size_t i = 0; i < tv::shaw_point::scalar_mul_count; i++)
        {
            auto &v = tv::shaw_point::scalar_mul_vectors[i];
            auto s = ShawScalar::from_bytes(v.scalar).value();
            auto p = sp_from(v.point);
            auto r = p.scalar_mul(s);
            check_bytes(
                (std::string("tv: shaw point scalar_mul ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
        }
    }
    /* MSM */
    {
        auto test_msm = [](const char *label,
                           size_t n,
                           const uint8_t scalars[][32],
                           const uint8_t points[][32],
                           const uint8_t expected[32])
        {
            std::vector<ShawScalar> sv;
            std::vector<ShawPoint> pv;
            for (size_t i = 0; i < n; i++)
            {
                sv.push_back(ShawScalar::from_bytes(scalars[i]).value());
                pv.push_back(ShawPoint::from_bytes(points[i]).value());
            }
            auto r = ShawPoint::multi_scalar_mul(sv.data(), pv.data(), n);
            check_bytes(label, expected, r.to_bytes().data(), 32);
        };
        test_msm(
            "tv: shaw msm n_1",
            1,
            tv::shaw_point::msm_n_1_scalars,
            tv::shaw_point::msm_n_1_points,
            tv::shaw_point::msm_n_1_result);
        test_msm(
            "tv: shaw msm n_2",
            2,
            tv::shaw_point::msm_n_2_scalars,
            tv::shaw_point::msm_n_2_points,
            tv::shaw_point::msm_n_2_result);
        test_msm(
            "tv: shaw msm n_4",
            4,
            tv::shaw_point::msm_n_4_scalars,
            tv::shaw_point::msm_n_4_points,
            tv::shaw_point::msm_n_4_result);
        test_msm(
            "tv: shaw msm n_16",
            16,
            tv::shaw_point::msm_n_16_scalars,
            tv::shaw_point::msm_n_16_points,
            tv::shaw_point::msm_n_16_result);
        test_msm(
            "tv: shaw msm n_32_straus",
            32,
            tv::shaw_point::msm_n_32_straus_scalars,
            tv::shaw_point::msm_n_32_straus_points,
            tv::shaw_point::msm_n_32_straus_result);
        test_msm(
            "tv: shaw msm n_33_pippenger",
            33,
            tv::shaw_point::msm_n_33_pippenger_scalars,
            tv::shaw_point::msm_n_33_pippenger_points,
            tv::shaw_point::msm_n_33_pippenger_result);
        test_msm(
            "tv: shaw msm n_64_pippenger",
            64,
            tv::shaw_point::msm_n_64_pippenger_scalars,
            tv::shaw_point::msm_n_64_pippenger_points,
            tv::shaw_point::msm_n_64_pippenger_result);
    }
    /* Pedersen */
    {
        auto test_ped = [](const char *label,
                           const uint8_t blinding[32],
                           const uint8_t H[32],
                           size_t n,
                           const uint8_t values[][32],
                           const uint8_t generators[][32],
                           const uint8_t expected[32])
        {
            auto s_blind = ShawScalar::from_bytes(blinding).value();
            auto p_H = ShawPoint::from_bytes(H).value();
            std::vector<ShawScalar> vals;
            std::vector<ShawPoint> gens;
            for (size_t i = 0; i < n; i++)
            {
                vals.push_back(ShawScalar::from_bytes(values[i]).value());
                gens.push_back(ShawPoint::from_bytes(generators[i]).value());
            }
            auto r = ShawPoint::pedersen_commit(s_blind, p_H, vals.data(), gens.data(), n);
            check_bytes(label, expected, r.to_bytes().data(), 32);
        };
        test_ped(
            "tv: shaw pedersen n_1",
            tv::shaw_point::pedersen_n_1_blinding,
            tv::shaw_point::pedersen_n_1_H,
            1,
            tv::shaw_point::pedersen_n_1_values,
            tv::shaw_point::pedersen_n_1_generators,
            tv::shaw_point::pedersen_n_1_result);
        test_ped(
            "tv: shaw pedersen blinding_zero",
            tv::shaw_point::pedersen_blinding_zero_blinding,
            tv::shaw_point::pedersen_blinding_zero_H,
            1,
            tv::shaw_point::pedersen_blinding_zero_values,
            tv::shaw_point::pedersen_blinding_zero_generators,
            tv::shaw_point::pedersen_blinding_zero_result);
        test_ped(
            "tv: shaw pedersen n_4",
            tv::shaw_point::pedersen_n_4_blinding,
            tv::shaw_point::pedersen_n_4_H,
            4,
            tv::shaw_point::pedersen_n_4_values,
            tv::shaw_point::pedersen_n_4_generators,
            tv::shaw_point::pedersen_n_4_result);
    }
    for (size_t i = 0; i < tv::shaw_point::map_to_curve_single_count; i++)
    {
        auto &v = tv::shaw_point::map_to_curve_single_vectors[i];
        auto r = ShawPoint::map_to_curve(v.u);
        check_bytes((std::string("tv: shaw point map_to_curve ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::shaw_point::map_to_curve_double_count; i++)
    {
        auto &v = tv::shaw_point::map_to_curve_double_vectors[i];
        auto r = ShawPoint::map_to_curve(v.u0, v.u1);
        check_bytes(
            (std::string("tv: shaw point map_to_curve2 ") + v.label).c_str(), v.result, r.to_bytes().data(), 32);
    }
    for (size_t i = 0; i < tv::shaw_point::x_coordinate_count; i++)
    {
        auto &v = tv::shaw_point::x_coordinate_vectors[i];
        auto p = ShawPoint::from_bytes(v.point).value();
        auto r = p.x_coordinate_bytes();
        check_bytes((std::string("tv: shaw point x_coord ") + v.label).c_str(), v.x_bytes, r.data(), 32);
    }

    /* ---- Batch Invert ---- */
    std::cout << "  --- Batch Invert ---" << std::endl;
    {
        /* fp n=1 */
        {
            uint8_t result[32];
            std::memcpy(result, tv::batch_invert::fp_n_1_inputs[0], 32);
            fp_fe fe;
            fp_frombytes(fe, result);
            fp_invert(fe, fe);
            fp_tobytes(result, fe);
            check_bytes("tv: batch invert fp n_1", tv::batch_invert::fp_n_1_results[0], result, 32);
        }
        /* fp n=4 */
        {
            fp_fe fes[4];
            for (int i = 0; i < 4; i++)
                fp_frombytes(fes[i], tv::batch_invert::fp_n_4_inputs[i]);
            fp_batch_invert(fes, fes, 4);
            for (int i = 0; i < 4; i++)
            {
                uint8_t result[32];
                fp_tobytes(result, fes[i]);
                check_bytes(
                    (std::string("tv: batch invert fp n_4 [") + std::to_string(i) + "]").c_str(),
                    tv::batch_invert::fp_n_4_results[i],
                    result,
                    32);
            }
        }
        /* fq n=1 */
        {
            uint8_t result[32];
            std::memcpy(result, tv::batch_invert::fq_n_1_inputs[0], 32);
            fq_fe fe;
            fq_frombytes(fe, result);
            fq_invert(fe, fe);
            fq_tobytes(result, fe);
            check_bytes("tv: batch invert fq n_1", tv::batch_invert::fq_n_1_results[0], result, 32);
        }
        /* fq n=4 */
        {
            fq_fe fes[4];
            for (int i = 0; i < 4; i++)
                fq_frombytes(fes[i], tv::batch_invert::fq_n_4_inputs[i]);
            fq_batch_invert(fes, fes, 4);
            for (int i = 0; i < 4; i++)
            {
                uint8_t result[32];
                fq_tobytes(result, fes[i]);
                check_bytes(
                    (std::string("tv: batch invert fq n_4 [") + std::to_string(i) + "]").c_str(),
                    tv::batch_invert::fq_n_4_results[i],
                    result,
                    32);
            }
        }
    }

    /* ---- Fp Polynomial ---- */
    std::cout << "  --- Fp Polynomial ---" << std::endl;
    {
        namespace fp = tv::fp_polynomial;

        // from_roots: one root
        {
            auto p = FpPolynomial::from_roots(fp::from_roots_one_root_roots[0], 1);
            size_t n = sizeof(fp::from_roots_one_root_coefficients) / sizeof(fp::from_roots_one_root_coefficients[0]);
            check_int("tv: fp poly from_roots one_root degree", (int)(n - 1), (int)p.degree());
            auto rebuilt = FpPolynomial::from_coefficients(fp::from_roots_one_root_coefficients[0], n);
            for (size_t i = 0; i < n; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, p.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly from_roots one_root coeff[") + std::to_string(i) + "]").c_str(),
                    fp::from_roots_one_root_coefficients[i],
                    c,
                    32);
            }
        }
        // from_roots: two roots
        {
            auto p = FpPolynomial::from_roots(fp::from_roots_two_roots_roots[0], 2);
            size_t n = sizeof(fp::from_roots_two_roots_coefficients) / sizeof(fp::from_roots_two_roots_coefficients[0]);
            check_int("tv: fp poly from_roots two_roots degree", (int)(n - 1), (int)p.degree());
            for (size_t i = 0; i < n; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, p.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly from_roots two_roots coeff[") + std::to_string(i) + "]").c_str(),
                    fp::from_roots_two_roots_coefficients[i],
                    c,
                    32);
            }
        }
        // from_roots: four roots
        {
            auto p = FpPolynomial::from_roots(fp::from_roots_four_roots_roots[0], 4);
            size_t n =
                sizeof(fp::from_roots_four_roots_coefficients) / sizeof(fp::from_roots_four_roots_coefficients[0]);
            check_int("tv: fp poly from_roots four_roots degree", (int)(n - 1), (int)p.degree());
            for (size_t i = 0; i < n; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, p.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly from_roots four_roots coeff[") + std::to_string(i) + "]").c_str(),
                    fp::from_roots_four_roots_coefficients[i],
                    c,
                    32);
            }
        }

        // evaluate: constant at 7
        {
            auto p = FpPolynomial::from_coefficients(fp::eval_constant_at_7_coefficients[0], 1);
            auto r = p.evaluate(fp::eval_constant_at_7_x);
            check_bytes("tv: fp poly eval constant_at_7", fp::eval_constant_at_7_result, r.data(), 32);
        }
        // evaluate: linear at 0
        {
            auto p = FpPolynomial::from_coefficients(fp::eval_linear_at_0_coefficients[0], 2);
            auto r = p.evaluate(fp::eval_linear_at_0_x);
            check_bytes("tv: fp poly eval linear_at_0", fp::eval_linear_at_0_result, r.data(), 32);
        }
        // evaluate: linear at test_a
        {
            auto p = FpPolynomial::from_coefficients(fp::eval_linear_at_test_a_coefficients[0], 2);
            auto r = p.evaluate(fp::eval_linear_at_test_a_x);
            check_bytes("tv: fp poly eval linear_at_test_a", fp::eval_linear_at_test_a_result, r.data(), 32);
        }
        // evaluate: quadratic at 7
        {
            auto p = FpPolynomial::from_coefficients(fp::eval_quadratic_at_7_coefficients[0], 3);
            auto r = p.evaluate(fp::eval_quadratic_at_7_x);
            check_bytes("tv: fp poly eval quadratic_at_7", fp::eval_quadratic_at_7_result, r.data(), 32);
        }

        // mul: deg1 * deg1
        {
            size_t na = sizeof(fp::mul_deg1_times_deg1_a) / sizeof(fp::mul_deg1_times_deg1_a[0]);
            size_t nb = sizeof(fp::mul_deg1_times_deg1_b) / sizeof(fp::mul_deg1_times_deg1_b[0]);
            size_t nr = sizeof(fp::mul_deg1_times_deg1_result) / sizeof(fp::mul_deg1_times_deg1_result[0]);
            auto a = FpPolynomial::from_coefficients(fp::mul_deg1_times_deg1_a[0], na);
            auto b = FpPolynomial::from_coefficients(fp::mul_deg1_times_deg1_b[0], nb);
            auto r = a * b;
            check_int("tv: fp poly mul deg1*deg1 degree", (int)(nr - 1), (int)r.degree());
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly mul deg1*deg1 coeff[") + std::to_string(i) + "]").c_str(),
                    fp::mul_deg1_times_deg1_result[i],
                    c,
                    32);
            }
        }
        // mul: deg5 * deg5
        {
            size_t na = sizeof(fp::mul_deg5_times_deg5_a) / sizeof(fp::mul_deg5_times_deg5_a[0]);
            size_t nb = sizeof(fp::mul_deg5_times_deg5_b) / sizeof(fp::mul_deg5_times_deg5_b[0]);
            size_t nr = sizeof(fp::mul_deg5_times_deg5_result) / sizeof(fp::mul_deg5_times_deg5_result[0]);
            auto a = FpPolynomial::from_coefficients(fp::mul_deg5_times_deg5_a[0], na);
            auto b = FpPolynomial::from_coefficients(fp::mul_deg5_times_deg5_b[0], nb);
            auto r = a * b;
            check_int("tv: fp poly mul deg5*deg5 degree", (int)(nr - 1), (int)r.degree());
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly mul deg5*deg5 coeff[") + std::to_string(i) + "]").c_str(),
                    fp::mul_deg5_times_deg5_result[i],
                    c,
                    32);
            }
        }
        // mul: deg15 * deg15
        {
            size_t na = sizeof(fp::mul_deg15_times_deg15_a) / sizeof(fp::mul_deg15_times_deg15_a[0]);
            size_t nb = sizeof(fp::mul_deg15_times_deg15_b) / sizeof(fp::mul_deg15_times_deg15_b[0]);
            size_t nr = sizeof(fp::mul_deg15_times_deg15_result) / sizeof(fp::mul_deg15_times_deg15_result[0]);
            auto a = FpPolynomial::from_coefficients(fp::mul_deg15_times_deg15_a[0], na);
            auto b = FpPolynomial::from_coefficients(fp::mul_deg15_times_deg15_b[0], nb);
            auto r = a * b;
            check_int("tv: fp poly mul deg15*deg15 degree", (int)(nr - 1), (int)r.degree());
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly mul deg15*deg15 coeff[") + std::to_string(i) + "]").c_str(),
                    fp::mul_deg15_times_deg15_result[i],
                    c,
                    32);
            }
        }
        // mul: deg16 * deg16 (karatsuba)
        {
            size_t na =
                sizeof(fp::mul_deg16_times_deg16_karatsuba_a) / sizeof(fp::mul_deg16_times_deg16_karatsuba_a[0]);
            size_t nb =
                sizeof(fp::mul_deg16_times_deg16_karatsuba_b) / sizeof(fp::mul_deg16_times_deg16_karatsuba_b[0]);
            size_t nr = sizeof(fp::mul_deg16_times_deg16_karatsuba_result)
                        / sizeof(fp::mul_deg16_times_deg16_karatsuba_result[0]);
            auto a = FpPolynomial::from_coefficients(fp::mul_deg16_times_deg16_karatsuba_a[0], na);
            auto b = FpPolynomial::from_coefficients(fp::mul_deg16_times_deg16_karatsuba_b[0], nb);
            auto r = a * b;
            check_int("tv: fp poly mul deg16*deg16 karatsuba degree", (int)(nr - 1), (int)r.degree());
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly mul deg16*deg16 karatsuba coeff[") + std::to_string(i) + "]").c_str(),
                    fp::mul_deg16_times_deg16_karatsuba_result[i],
                    c,
                    32);
            }
        }

        // add: same degree
        {
            size_t na = sizeof(fp::add_same_degree_a) / sizeof(fp::add_same_degree_a[0]);
            size_t nb = sizeof(fp::add_same_degree_b) / sizeof(fp::add_same_degree_b[0]);
            size_t nr = sizeof(fp::add_same_degree_result) / sizeof(fp::add_same_degree_result[0]);
            auto a = FpPolynomial::from_coefficients(fp::add_same_degree_a[0], na);
            auto b = FpPolynomial::from_coefficients(fp::add_same_degree_b[0], nb);
            auto r = a + b;
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly add same_deg coeff[") + std::to_string(i) + "]").c_str(),
                    fp::add_same_degree_result[i],
                    c,
                    32);
            }
        }
        // add: different degree
        {
            size_t na = sizeof(fp::add_different_degree_a) / sizeof(fp::add_different_degree_a[0]);
            size_t nb = sizeof(fp::add_different_degree_b) / sizeof(fp::add_different_degree_b[0]);
            size_t nr = sizeof(fp::add_different_degree_result) / sizeof(fp::add_different_degree_result[0]);
            auto a = FpPolynomial::from_coefficients(fp::add_different_degree_a[0], na);
            auto b = FpPolynomial::from_coefficients(fp::add_different_degree_b[0], nb);
            auto r = a + b;
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly add diff_deg coeff[") + std::to_string(i) + "]").c_str(),
                    fp::add_different_degree_result[i],
                    c,
                    32);
            }
        }

        // sub: same degree
        {
            size_t na = sizeof(fp::sub_same_degree_a) / sizeof(fp::sub_same_degree_a[0]);
            size_t nb = sizeof(fp::sub_same_degree_b) / sizeof(fp::sub_same_degree_b[0]);
            size_t nr = sizeof(fp::sub_same_degree_result) / sizeof(fp::sub_same_degree_result[0]);
            auto a = FpPolynomial::from_coefficients(fp::sub_same_degree_a[0], na);
            auto b = FpPolynomial::from_coefficients(fp::sub_same_degree_b[0], nb);
            auto r = a - b;
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly sub same_deg coeff[") + std::to_string(i) + "]").c_str(),
                    fp::sub_same_degree_result[i],
                    c,
                    32);
            }
        }
        // sub: different degree
        {
            size_t na = sizeof(fp::sub_different_degree_a) / sizeof(fp::sub_different_degree_a[0]);
            size_t nb = sizeof(fp::sub_different_degree_b) / sizeof(fp::sub_different_degree_b[0]);
            size_t nr = sizeof(fp::sub_different_degree_result) / sizeof(fp::sub_different_degree_result[0]);
            auto a = FpPolynomial::from_coefficients(fp::sub_different_degree_a[0], na);
            auto b = FpPolynomial::from_coefficients(fp::sub_different_degree_b[0], nb);
            auto r = a - b;
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly sub diff_deg coeff[") + std::to_string(i) + "]").c_str(),
                    fp::sub_different_degree_result[i],
                    c,
                    32);
            }
        }

        // divmod: exact division
        {
            size_t nn = sizeof(fp::divmod_exact_division_numerator) / sizeof(fp::divmod_exact_division_numerator[0]);
            size_t nd =
                sizeof(fp::divmod_exact_division_denominator) / sizeof(fp::divmod_exact_division_denominator[0]);
            size_t nq = sizeof(fp::divmod_exact_division_quotient) / sizeof(fp::divmod_exact_division_quotient[0]);
            size_t nrem = sizeof(fp::divmod_exact_division_remainder) / sizeof(fp::divmod_exact_division_remainder[0]);
            auto num = FpPolynomial::from_coefficients(fp::divmod_exact_division_numerator[0], nn);
            auto den = FpPolynomial::from_coefficients(fp::divmod_exact_division_denominator[0], nd);
            auto [q, rem] = num.divmod(den);
            for (size_t i = 0; i < nq; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, q.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly divmod exact q[") + std::to_string(i) + "]").c_str(),
                    fp::divmod_exact_division_quotient[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nrem; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, rem.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly divmod exact r[") + std::to_string(i) + "]").c_str(),
                    fp::divmod_exact_division_remainder[i],
                    c,
                    32);
            }
        }
        // divmod: nonzero remainder
        {
            size_t nn =
                sizeof(fp::divmod_nonzero_remainder_numerator) / sizeof(fp::divmod_nonzero_remainder_numerator[0]);
            size_t nd =
                sizeof(fp::divmod_nonzero_remainder_denominator) / sizeof(fp::divmod_nonzero_remainder_denominator[0]);
            size_t nq =
                sizeof(fp::divmod_nonzero_remainder_quotient) / sizeof(fp::divmod_nonzero_remainder_quotient[0]);
            size_t nrem =
                sizeof(fp::divmod_nonzero_remainder_remainder) / sizeof(fp::divmod_nonzero_remainder_remainder[0]);
            auto num = FpPolynomial::from_coefficients(fp::divmod_nonzero_remainder_numerator[0], nn);
            auto den = FpPolynomial::from_coefficients(fp::divmod_nonzero_remainder_denominator[0], nd);
            auto [q, rem] = num.divmod(den);
            for (size_t i = 0; i < nq; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, q.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly divmod nonzero_rem q[") + std::to_string(i) + "]").c_str(),
                    fp::divmod_nonzero_remainder_quotient[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nrem; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, rem.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly divmod nonzero_rem r[") + std::to_string(i) + "]").c_str(),
                    fp::divmod_nonzero_remainder_remainder[i],
                    c,
                    32);
            }
        }
        // divmod: divide by linear
        {
            size_t nn =
                sizeof(fp::divmod_divide_by_linear_numerator) / sizeof(fp::divmod_divide_by_linear_numerator[0]);
            size_t nd =
                sizeof(fp::divmod_divide_by_linear_denominator) / sizeof(fp::divmod_divide_by_linear_denominator[0]);
            size_t nq = sizeof(fp::divmod_divide_by_linear_quotient) / sizeof(fp::divmod_divide_by_linear_quotient[0]);
            size_t nrem =
                sizeof(fp::divmod_divide_by_linear_remainder) / sizeof(fp::divmod_divide_by_linear_remainder[0]);
            auto num = FpPolynomial::from_coefficients(fp::divmod_divide_by_linear_numerator[0], nn);
            auto den = FpPolynomial::from_coefficients(fp::divmod_divide_by_linear_denominator[0], nd);
            auto [q, rem] = num.divmod(den);
            for (size_t i = 0; i < nq; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, q.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly divmod by_linear q[") + std::to_string(i) + "]").c_str(),
                    fp::divmod_divide_by_linear_quotient[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nrem; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, rem.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly divmod by_linear r[") + std::to_string(i) + "]").c_str(),
                    fp::divmod_divide_by_linear_remainder[i],
                    c,
                    32);
            }
        }

        // interpolate: three points
        {
            size_t nc = sizeof(fp::interp_three_points_coefficients) / sizeof(fp::interp_three_points_coefficients[0]);
            auto p = FpPolynomial::interpolate(fp::interp_three_points_xs[0], fp::interp_three_points_ys[0], 3);
            check_int("tv: fp poly interp three_points degree", (int)(nc - 1), (int)p.degree());
            for (size_t i = 0; i < nc; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, p.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly interp three_points coeff[") + std::to_string(i) + "]").c_str(),
                    fp::interp_three_points_coefficients[i],
                    c,
                    32);
            }
        }
        // interpolate: four points
        {
            size_t nc = sizeof(fp::interp_four_points_coefficients) / sizeof(fp::interp_four_points_coefficients[0]);
            auto p = FpPolynomial::interpolate(fp::interp_four_points_xs[0], fp::interp_four_points_ys[0], 4);
            check_int("tv: fp poly interp four_points degree", (int)(nc - 1), (int)p.degree());
            for (size_t i = 0; i < nc; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, p.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fp poly interp four_points coeff[") + std::to_string(i) + "]").c_str(),
                    fp::interp_four_points_coefficients[i],
                    c,
                    32);
            }
        }
    }

    /* ---- Fq Polynomial ---- */
    std::cout << "  --- Fq Polynomial ---" << std::endl;
    {
        namespace fq = tv::fq_polynomial;

        // from_roots: one root
        {
            auto p = FqPolynomial::from_roots(fq::from_roots_one_root_roots[0], 1);
            size_t n = sizeof(fq::from_roots_one_root_coefficients) / sizeof(fq::from_roots_one_root_coefficients[0]);
            check_int("tv: fq poly from_roots one_root degree", (int)(n - 1), (int)p.degree());
            for (size_t i = 0; i < n; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, p.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly from_roots one_root coeff[") + std::to_string(i) + "]").c_str(),
                    fq::from_roots_one_root_coefficients[i],
                    c,
                    32);
            }
        }
        // from_roots: two roots
        {
            auto p = FqPolynomial::from_roots(fq::from_roots_two_roots_roots[0], 2);
            size_t n = sizeof(fq::from_roots_two_roots_coefficients) / sizeof(fq::from_roots_two_roots_coefficients[0]);
            check_int("tv: fq poly from_roots two_roots degree", (int)(n - 1), (int)p.degree());
            for (size_t i = 0; i < n; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, p.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly from_roots two_roots coeff[") + std::to_string(i) + "]").c_str(),
                    fq::from_roots_two_roots_coefficients[i],
                    c,
                    32);
            }
        }
        // from_roots: four roots
        {
            auto p = FqPolynomial::from_roots(fq::from_roots_four_roots_roots[0], 4);
            size_t n =
                sizeof(fq::from_roots_four_roots_coefficients) / sizeof(fq::from_roots_four_roots_coefficients[0]);
            check_int("tv: fq poly from_roots four_roots degree", (int)(n - 1), (int)p.degree());
            for (size_t i = 0; i < n; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, p.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly from_roots four_roots coeff[") + std::to_string(i) + "]").c_str(),
                    fq::from_roots_four_roots_coefficients[i],
                    c,
                    32);
            }
        }

        // evaluate: constant at 7
        {
            auto p = FqPolynomial::from_coefficients(fq::eval_constant_at_7_coefficients[0], 1);
            auto r = p.evaluate(fq::eval_constant_at_7_x);
            check_bytes("tv: fq poly eval constant_at_7", fq::eval_constant_at_7_result, r.data(), 32);
        }
        // evaluate: linear at 0
        {
            auto p = FqPolynomial::from_coefficients(fq::eval_linear_at_0_coefficients[0], 2);
            auto r = p.evaluate(fq::eval_linear_at_0_x);
            check_bytes("tv: fq poly eval linear_at_0", fq::eval_linear_at_0_result, r.data(), 32);
        }
        // evaluate: linear at test_a
        {
            auto p = FqPolynomial::from_coefficients(fq::eval_linear_at_test_a_coefficients[0], 2);
            auto r = p.evaluate(fq::eval_linear_at_test_a_x);
            check_bytes("tv: fq poly eval linear_at_test_a", fq::eval_linear_at_test_a_result, r.data(), 32);
        }
        // evaluate: quadratic at 7
        {
            auto p = FqPolynomial::from_coefficients(fq::eval_quadratic_at_7_coefficients[0], 3);
            auto r = p.evaluate(fq::eval_quadratic_at_7_x);
            check_bytes("tv: fq poly eval quadratic_at_7", fq::eval_quadratic_at_7_result, r.data(), 32);
        }

        // mul: deg1 * deg1
        {
            size_t na = sizeof(fq::mul_deg1_times_deg1_a) / sizeof(fq::mul_deg1_times_deg1_a[0]);
            size_t nb = sizeof(fq::mul_deg1_times_deg1_b) / sizeof(fq::mul_deg1_times_deg1_b[0]);
            size_t nr = sizeof(fq::mul_deg1_times_deg1_result) / sizeof(fq::mul_deg1_times_deg1_result[0]);
            auto a = FqPolynomial::from_coefficients(fq::mul_deg1_times_deg1_a[0], na);
            auto b = FqPolynomial::from_coefficients(fq::mul_deg1_times_deg1_b[0], nb);
            auto r = a * b;
            check_int("tv: fq poly mul deg1*deg1 degree", (int)(nr - 1), (int)r.degree());
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly mul deg1*deg1 coeff[") + std::to_string(i) + "]").c_str(),
                    fq::mul_deg1_times_deg1_result[i],
                    c,
                    32);
            }
        }
        // mul: deg5 * deg5
        {
            size_t na = sizeof(fq::mul_deg5_times_deg5_a) / sizeof(fq::mul_deg5_times_deg5_a[0]);
            size_t nb = sizeof(fq::mul_deg5_times_deg5_b) / sizeof(fq::mul_deg5_times_deg5_b[0]);
            size_t nr = sizeof(fq::mul_deg5_times_deg5_result) / sizeof(fq::mul_deg5_times_deg5_result[0]);
            auto a = FqPolynomial::from_coefficients(fq::mul_deg5_times_deg5_a[0], na);
            auto b = FqPolynomial::from_coefficients(fq::mul_deg5_times_deg5_b[0], nb);
            auto r = a * b;
            check_int("tv: fq poly mul deg5*deg5 degree", (int)(nr - 1), (int)r.degree());
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly mul deg5*deg5 coeff[") + std::to_string(i) + "]").c_str(),
                    fq::mul_deg5_times_deg5_result[i],
                    c,
                    32);
            }
        }
        // mul: deg15 * deg15
        {
            size_t na = sizeof(fq::mul_deg15_times_deg15_a) / sizeof(fq::mul_deg15_times_deg15_a[0]);
            size_t nb = sizeof(fq::mul_deg15_times_deg15_b) / sizeof(fq::mul_deg15_times_deg15_b[0]);
            size_t nr = sizeof(fq::mul_deg15_times_deg15_result) / sizeof(fq::mul_deg15_times_deg15_result[0]);
            auto a = FqPolynomial::from_coefficients(fq::mul_deg15_times_deg15_a[0], na);
            auto b = FqPolynomial::from_coefficients(fq::mul_deg15_times_deg15_b[0], nb);
            auto r = a * b;
            check_int("tv: fq poly mul deg15*deg15 degree", (int)(nr - 1), (int)r.degree());
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly mul deg15*deg15 coeff[") + std::to_string(i) + "]").c_str(),
                    fq::mul_deg15_times_deg15_result[i],
                    c,
                    32);
            }
        }
        // mul: deg16 * deg16 (karatsuba)
        {
            size_t na =
                sizeof(fq::mul_deg16_times_deg16_karatsuba_a) / sizeof(fq::mul_deg16_times_deg16_karatsuba_a[0]);
            size_t nb =
                sizeof(fq::mul_deg16_times_deg16_karatsuba_b) / sizeof(fq::mul_deg16_times_deg16_karatsuba_b[0]);
            size_t nr = sizeof(fq::mul_deg16_times_deg16_karatsuba_result)
                        / sizeof(fq::mul_deg16_times_deg16_karatsuba_result[0]);
            auto a = FqPolynomial::from_coefficients(fq::mul_deg16_times_deg16_karatsuba_a[0], na);
            auto b = FqPolynomial::from_coefficients(fq::mul_deg16_times_deg16_karatsuba_b[0], nb);
            auto r = a * b;
            check_int("tv: fq poly mul deg16*deg16 karatsuba degree", (int)(nr - 1), (int)r.degree());
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly mul deg16*deg16 karatsuba coeff[") + std::to_string(i) + "]").c_str(),
                    fq::mul_deg16_times_deg16_karatsuba_result[i],
                    c,
                    32);
            }
        }

        // add: same degree
        {
            size_t na = sizeof(fq::add_same_degree_a) / sizeof(fq::add_same_degree_a[0]);
            size_t nb = sizeof(fq::add_same_degree_b) / sizeof(fq::add_same_degree_b[0]);
            size_t nr = sizeof(fq::add_same_degree_result) / sizeof(fq::add_same_degree_result[0]);
            auto a = FqPolynomial::from_coefficients(fq::add_same_degree_a[0], na);
            auto b = FqPolynomial::from_coefficients(fq::add_same_degree_b[0], nb);
            auto r = a + b;
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly add same_deg coeff[") + std::to_string(i) + "]").c_str(),
                    fq::add_same_degree_result[i],
                    c,
                    32);
            }
        }
        // add: different degree
        {
            size_t na = sizeof(fq::add_different_degree_a) / sizeof(fq::add_different_degree_a[0]);
            size_t nb = sizeof(fq::add_different_degree_b) / sizeof(fq::add_different_degree_b[0]);
            size_t nr = sizeof(fq::add_different_degree_result) / sizeof(fq::add_different_degree_result[0]);
            auto a = FqPolynomial::from_coefficients(fq::add_different_degree_a[0], na);
            auto b = FqPolynomial::from_coefficients(fq::add_different_degree_b[0], nb);
            auto r = a + b;
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly add diff_deg coeff[") + std::to_string(i) + "]").c_str(),
                    fq::add_different_degree_result[i],
                    c,
                    32);
            }
        }

        // sub: same degree
        {
            size_t na = sizeof(fq::sub_same_degree_a) / sizeof(fq::sub_same_degree_a[0]);
            size_t nb = sizeof(fq::sub_same_degree_b) / sizeof(fq::sub_same_degree_b[0]);
            size_t nr = sizeof(fq::sub_same_degree_result) / sizeof(fq::sub_same_degree_result[0]);
            auto a = FqPolynomial::from_coefficients(fq::sub_same_degree_a[0], na);
            auto b = FqPolynomial::from_coefficients(fq::sub_same_degree_b[0], nb);
            auto r = a - b;
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly sub same_deg coeff[") + std::to_string(i) + "]").c_str(),
                    fq::sub_same_degree_result[i],
                    c,
                    32);
            }
        }
        // sub: different degree
        {
            size_t na = sizeof(fq::sub_different_degree_a) / sizeof(fq::sub_different_degree_a[0]);
            size_t nb = sizeof(fq::sub_different_degree_b) / sizeof(fq::sub_different_degree_b[0]);
            size_t nr = sizeof(fq::sub_different_degree_result) / sizeof(fq::sub_different_degree_result[0]);
            auto a = FqPolynomial::from_coefficients(fq::sub_different_degree_a[0], na);
            auto b = FqPolynomial::from_coefficients(fq::sub_different_degree_b[0], nb);
            auto r = a - b;
            for (size_t i = 0; i < nr; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, r.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly sub diff_deg coeff[") + std::to_string(i) + "]").c_str(),
                    fq::sub_different_degree_result[i],
                    c,
                    32);
            }
        }

        // divmod: exact division
        {
            size_t nn = sizeof(fq::divmod_exact_division_numerator) / sizeof(fq::divmod_exact_division_numerator[0]);
            size_t nd =
                sizeof(fq::divmod_exact_division_denominator) / sizeof(fq::divmod_exact_division_denominator[0]);
            size_t nq = sizeof(fq::divmod_exact_division_quotient) / sizeof(fq::divmod_exact_division_quotient[0]);
            size_t nrem = sizeof(fq::divmod_exact_division_remainder) / sizeof(fq::divmod_exact_division_remainder[0]);
            auto num = FqPolynomial::from_coefficients(fq::divmod_exact_division_numerator[0], nn);
            auto den = FqPolynomial::from_coefficients(fq::divmod_exact_division_denominator[0], nd);
            auto [q, rem] = num.divmod(den);
            for (size_t i = 0; i < nq; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, q.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly divmod exact q[") + std::to_string(i) + "]").c_str(),
                    fq::divmod_exact_division_quotient[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nrem; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, rem.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly divmod exact r[") + std::to_string(i) + "]").c_str(),
                    fq::divmod_exact_division_remainder[i],
                    c,
                    32);
            }
        }
        // divmod: nonzero remainder
        {
            size_t nn =
                sizeof(fq::divmod_nonzero_remainder_numerator) / sizeof(fq::divmod_nonzero_remainder_numerator[0]);
            size_t nd =
                sizeof(fq::divmod_nonzero_remainder_denominator) / sizeof(fq::divmod_nonzero_remainder_denominator[0]);
            size_t nq =
                sizeof(fq::divmod_nonzero_remainder_quotient) / sizeof(fq::divmod_nonzero_remainder_quotient[0]);
            size_t nrem =
                sizeof(fq::divmod_nonzero_remainder_remainder) / sizeof(fq::divmod_nonzero_remainder_remainder[0]);
            auto num = FqPolynomial::from_coefficients(fq::divmod_nonzero_remainder_numerator[0], nn);
            auto den = FqPolynomial::from_coefficients(fq::divmod_nonzero_remainder_denominator[0], nd);
            auto [q, rem] = num.divmod(den);
            for (size_t i = 0; i < nq; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, q.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly divmod nonzero_rem q[") + std::to_string(i) + "]").c_str(),
                    fq::divmod_nonzero_remainder_quotient[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nrem; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, rem.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly divmod nonzero_rem r[") + std::to_string(i) + "]").c_str(),
                    fq::divmod_nonzero_remainder_remainder[i],
                    c,
                    32);
            }
        }
        // divmod: divide by linear
        {
            size_t nn =
                sizeof(fq::divmod_divide_by_linear_numerator) / sizeof(fq::divmod_divide_by_linear_numerator[0]);
            size_t nd =
                sizeof(fq::divmod_divide_by_linear_denominator) / sizeof(fq::divmod_divide_by_linear_denominator[0]);
            size_t nq = sizeof(fq::divmod_divide_by_linear_quotient) / sizeof(fq::divmod_divide_by_linear_quotient[0]);
            size_t nrem =
                sizeof(fq::divmod_divide_by_linear_remainder) / sizeof(fq::divmod_divide_by_linear_remainder[0]);
            auto num = FqPolynomial::from_coefficients(fq::divmod_divide_by_linear_numerator[0], nn);
            auto den = FqPolynomial::from_coefficients(fq::divmod_divide_by_linear_denominator[0], nd);
            auto [q, rem] = num.divmod(den);
            for (size_t i = 0; i < nq; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, q.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly divmod by_linear q[") + std::to_string(i) + "]").c_str(),
                    fq::divmod_divide_by_linear_quotient[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nrem; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, rem.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly divmod by_linear r[") + std::to_string(i) + "]").c_str(),
                    fq::divmod_divide_by_linear_remainder[i],
                    c,
                    32);
            }
        }

        // interpolate: three points
        {
            size_t nc = sizeof(fq::interp_three_points_coefficients) / sizeof(fq::interp_three_points_coefficients[0]);
            auto p = FqPolynomial::interpolate(fq::interp_three_points_xs[0], fq::interp_three_points_ys[0], 3);
            check_int("tv: fq poly interp three_points degree", (int)(nc - 1), (int)p.degree());
            for (size_t i = 0; i < nc; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, p.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly interp three_points coeff[") + std::to_string(i) + "]").c_str(),
                    fq::interp_three_points_coefficients[i],
                    c,
                    32);
            }
        }
        // interpolate: four points
        {
            size_t nc = sizeof(fq::interp_four_points_coefficients) / sizeof(fq::interp_four_points_coefficients[0]);
            auto p = FqPolynomial::interpolate(fq::interp_four_points_xs[0], fq::interp_four_points_ys[0], 4);
            check_int("tv: fq poly interp four_points degree", (int)(nc - 1), (int)p.degree());
            for (size_t i = 0; i < nc; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, p.raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: fq poly interp four_points coeff[") + std::to_string(i) + "]").c_str(),
                    fq::interp_four_points_coefficients[i],
                    c,
                    32);
            }
        }
    }

    /* ---- Ran Divisor ---- */
    std::cout << "  --- Ran Divisor ---" << std::endl;
    {
        namespace hd = tv::ran_divisor;

        // n=2
        {
            RanPoint pts[2];
            for (int i = 0; i < 2; i++)
                pts[i] = RanPoint::from_bytes(hd::n_2_points[i]).value();
            auto d = RanDivisor::compute(pts, 2).value();
            size_t na = sizeof(hd::n_2_a_coefficients) / sizeof(hd::n_2_a_coefficients[0]);
            size_t nb = sizeof(hd::n_2_b_coefficients) / sizeof(hd::n_2_b_coefficients[0]);
            for (size_t i = 0; i < na; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, d.a().raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: ran divisor n=2 a[") + std::to_string(i) + "]").c_str(),
                    hd::n_2_a_coefficients[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nb; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, d.b().raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: ran divisor n=2 b[") + std::to_string(i) + "]").c_str(),
                    hd::n_2_b_coefficients[i],
                    c,
                    32);
            }
            auto ev = d.evaluate(hd::n_2_eval_point_x, hd::n_2_eval_point_y).value();
            check_bytes("tv: ran divisor n=2 eval", hd::n_2_eval_result, ev.data(), 32);
        }
        // n=4
        {
            RanPoint pts[4];
            for (int i = 0; i < 4; i++)
                pts[i] = RanPoint::from_bytes(hd::n_4_points[i]).value();
            auto d = RanDivisor::compute(pts, 4).value();
            size_t na = sizeof(hd::n_4_a_coefficients) / sizeof(hd::n_4_a_coefficients[0]);
            size_t nb = sizeof(hd::n_4_b_coefficients) / sizeof(hd::n_4_b_coefficients[0]);
            for (size_t i = 0; i < na; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, d.a().raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: ran divisor n=4 a[") + std::to_string(i) + "]").c_str(),
                    hd::n_4_a_coefficients[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nb; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, d.b().raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: ran divisor n=4 b[") + std::to_string(i) + "]").c_str(),
                    hd::n_4_b_coefficients[i],
                    c,
                    32);
            }
            auto ev = d.evaluate(hd::n_4_eval_point_x, hd::n_4_eval_point_y).value();
            check_bytes("tv: ran divisor n=4 eval", hd::n_4_eval_result, ev.data(), 32);
        }
        // n=8
        {
            RanPoint pts[8];
            for (int i = 0; i < 8; i++)
                pts[i] = RanPoint::from_bytes(hd::n_8_points[i]).value();
            auto d = RanDivisor::compute(pts, 8).value();
            size_t na = sizeof(hd::n_8_a_coefficients) / sizeof(hd::n_8_a_coefficients[0]);
            size_t nb = sizeof(hd::n_8_b_coefficients) / sizeof(hd::n_8_b_coefficients[0]);
            for (size_t i = 0; i < na; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, d.a().raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: ran divisor n=8 a[") + std::to_string(i) + "]").c_str(),
                    hd::n_8_a_coefficients[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nb; i++)
            {
                uint8_t c[32];
                fp_tobytes(c, d.b().raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: ran divisor n=8 b[") + std::to_string(i) + "]").c_str(),
                    hd::n_8_b_coefficients[i],
                    c,
                    32);
            }
            auto ev = d.evaluate(hd::n_8_eval_point_x, hd::n_8_eval_point_y).value();
            check_bytes("tv: ran divisor n=8 eval", hd::n_8_eval_result, ev.data(), 32);
        }
    }

    /* ---- Shaw Divisor ---- */
    std::cout << "  --- Shaw Divisor ---" << std::endl;
    {
        namespace sd = tv::shaw_divisor;

        // n=2
        {
            ShawPoint pts[2];
            for (int i = 0; i < 2; i++)
                pts[i] = ShawPoint::from_bytes(sd::n_2_points[i]).value();
            auto d = ShawDivisor::compute(pts, 2).value();
            size_t na = sizeof(sd::n_2_a_coefficients) / sizeof(sd::n_2_a_coefficients[0]);
            size_t nb = sizeof(sd::n_2_b_coefficients) / sizeof(sd::n_2_b_coefficients[0]);
            for (size_t i = 0; i < na; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, d.a().raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: shaw divisor n=2 a[") + std::to_string(i) + "]").c_str(),
                    sd::n_2_a_coefficients[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nb; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, d.b().raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: shaw divisor n=2 b[") + std::to_string(i) + "]").c_str(),
                    sd::n_2_b_coefficients[i],
                    c,
                    32);
            }
            auto ev = d.evaluate(sd::n_2_eval_point_x, sd::n_2_eval_point_y).value();
            check_bytes("tv: shaw divisor n=2 eval", sd::n_2_eval_result, ev.data(), 32);
        }
        // n=4
        {
            ShawPoint pts[4];
            for (int i = 0; i < 4; i++)
                pts[i] = ShawPoint::from_bytes(sd::n_4_points[i]).value();
            auto d = ShawDivisor::compute(pts, 4).value();
            size_t na = sizeof(sd::n_4_a_coefficients) / sizeof(sd::n_4_a_coefficients[0]);
            size_t nb = sizeof(sd::n_4_b_coefficients) / sizeof(sd::n_4_b_coefficients[0]);
            for (size_t i = 0; i < na; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, d.a().raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: shaw divisor n=4 a[") + std::to_string(i) + "]").c_str(),
                    sd::n_4_a_coefficients[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nb; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, d.b().raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: shaw divisor n=4 b[") + std::to_string(i) + "]").c_str(),
                    sd::n_4_b_coefficients[i],
                    c,
                    32);
            }
            auto ev = d.evaluate(sd::n_4_eval_point_x, sd::n_4_eval_point_y).value();
            check_bytes("tv: shaw divisor n=4 eval", sd::n_4_eval_result, ev.data(), 32);
        }
        // n=8
        {
            ShawPoint pts[8];
            for (int i = 0; i < 8; i++)
                pts[i] = ShawPoint::from_bytes(sd::n_8_points[i]).value();
            auto d = ShawDivisor::compute(pts, 8).value();
            size_t na = sizeof(sd::n_8_a_coefficients) / sizeof(sd::n_8_a_coefficients[0]);
            size_t nb = sizeof(sd::n_8_b_coefficients) / sizeof(sd::n_8_b_coefficients[0]);
            for (size_t i = 0; i < na; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, d.a().raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: shaw divisor n=8 a[") + std::to_string(i) + "]").c_str(),
                    sd::n_8_a_coefficients[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nb; i++)
            {
                uint8_t c[32];
                fq_tobytes(c, d.b().raw().coeffs[i].v);
                check_bytes(
                    (std::string("tv: shaw divisor n=8 b[") + std::to_string(i) + "]").c_str(),
                    sd::n_8_b_coefficients[i],
                    c,
                    32);
            }
            auto ev = d.evaluate(sd::n_8_eval_point_x, sd::n_8_eval_point_y).value();
            check_bytes("tv: shaw divisor n=8 eval", sd::n_8_eval_result, ev.data(), 32);
        }
    }

    /* ---- High-Degree Poly Mul ---- */
    std::cout << "  --- High-Degree Poly Mul ---" << std::endl;
    {
        namespace hdp = tv::high_degree_poly_mul;

        // Fp vectors
        for (size_t vi = 0; vi < hdp::fp_count; vi++)
        {
            auto &v = hdp::fp_vectors[vi];
            size_t n = (size_t)v.n_coeffs;

            // Build deterministic polynomials: a[i] = (i+1) mod p, b[i] = (i+n+1) mod p
            std::vector<uint8_t> a_bytes(n * 32, 0), b_bytes(n * 32, 0);
            for (size_t i = 0; i < n; i++)
            {
                uint32_t va = (uint32_t)(i + 1);
                uint32_t vb = (uint32_t)(i + n + 1);
                std::memcpy(&a_bytes[i * 32], &va, sizeof(va));
                std::memcpy(&b_bytes[i * 32], &vb, sizeof(vb));
            }
            auto a = FpPolynomial::from_coefficients(a_bytes.data(), n);
            auto b = FpPolynomial::from_coefficients(b_bytes.data(), n);
            auto r = a * b;

            std::string prefix = std::string("tv: highdeg fp ") + v.label;
            check_int((prefix + " result_degree").c_str(), v.result_degree, (int)r.degree());

            for (size_t ci = 0; ci < 3; ci++)
            {
                auto &chk = v.checks[ci];
                // Verify a(x)
                auto a_at_x = a.evaluate(chk.x);
                check_bytes((prefix + " " + chk.point + " a(x)").c_str(), chk.a_of_x, a_at_x.data(), 32);
                // Verify b(x)
                auto b_at_x = b.evaluate(chk.x);
                check_bytes((prefix + " " + chk.point + " b(x)").c_str(), chk.b_of_x, b_at_x.data(), 32);
                // Verify result(x) = a(x)*b(x)
                auto r_at_x = r.evaluate(chk.x);
                check_bytes((prefix + " " + chk.point + " result(x)").c_str(), chk.result_of_x, r_at_x.data(), 32);
            }
        }

        // Fq vectors
        for (size_t vi = 0; vi < hdp::fq_count; vi++)
        {
            auto &v = hdp::fq_vectors[vi];
            size_t n = (size_t)v.n_coeffs;

            std::vector<uint8_t> a_bytes(n * 32, 0), b_bytes(n * 32, 0);
            for (size_t i = 0; i < n; i++)
            {
                uint32_t va = (uint32_t)(i + 1);
                uint32_t vb = (uint32_t)(i + n + 1);
                std::memcpy(&a_bytes[i * 32], &va, sizeof(va));
                std::memcpy(&b_bytes[i * 32], &vb, sizeof(vb));
            }
            auto a = FqPolynomial::from_coefficients(a_bytes.data(), n);
            auto b = FqPolynomial::from_coefficients(b_bytes.data(), n);
            auto r = a * b;

            std::string prefix = std::string("tv: highdeg fq ") + v.label;
            check_int((prefix + " result_degree").c_str(), v.result_degree, (int)r.degree());

            for (size_t ci = 0; ci < 3; ci++)
            {
                auto &chk = v.checks[ci];
                auto a_at_x = a.evaluate(chk.x);
                check_bytes((prefix + " " + chk.point + " a(x)").c_str(), chk.a_of_x, a_at_x.data(), 32);
                auto b_at_x = b.evaluate(chk.x);
                check_bytes((prefix + " " + chk.point + " b(x)").c_str(), chk.b_of_x, b_at_x.data(), 32);
                auto r_at_x = r.evaluate(chk.x);
                check_bytes((prefix + " " + chk.point + " result(x)").c_str(), chk.result_of_x, r_at_x.data(), 32);
            }
        }
    }

    /* ---- Wei25519 ---- */
    std::cout << "  --- Wei25519 ---" << std::endl;
    for (size_t i = 0; i < tv::wei25519::x_to_scalar_count; i++)
    {
        auto &v = tv::wei25519::x_to_scalar_vectors[i];
        auto r = shaw_scalar_from_wei25519_x(v.input);
        std::string name = std::string("tv: wei25519 x_to_scalar ") + v.label;
        if (v.valid)
        {
            check_int((name + " valid").c_str(), 1, r.has_value() ? 1 : 0);
            if (r)
                check_bytes((name + " value").c_str(), v.result, r->to_bytes().data(), 32);
        }
        else
        {
            check_int((name + " invalid").c_str(), 0, r.has_value() ? 1 : 0);
        }
    }
}


void test_vector_validation_c_primitives()
{
    namespace tv = ranshaw_test_vectors;

    std::cout << std::endl << "=== Test Vector Validation (C Primitives) ===" << std::endl;

    /* Helper: load a ran_jacobian from 32-byte encoding, handling identity */
    auto h_load = [](ran_jacobian *out, const uint8_t bytes[32]) -> bool
    {
        bool all_zero = true;
        for (int i = 0; i < 32; i++)
        {
            if (bytes[i])
            {
                all_zero = false;
                break;
            }
        }
        if (all_zero)
        {
            ran_identity(out);
            return true;
        }
        return ran_frombytes(out, bytes) == 0;
    };

    auto s_load = [](shaw_jacobian *out, const uint8_t bytes[32]) -> bool
    {
        bool all_zero = true;
        for (int i = 0; i < 32; i++)
        {
            if (bytes[i])
            {
                all_zero = false;
                break;
            }
        }
        if (all_zero)
        {
            shaw_identity(out);
            return true;
        }
        return shaw_frombytes(out, bytes) == 0;
    };

    /* ==== Ran Scalar (C primitives) ==== */
    std::cout << "  --- Ran Scalar (C) ---" << std::endl;
    for (size_t i = 0; i < tv::ran_scalar::add_count; i++)
    {
        auto &v = tv::ran_scalar::add_vectors[i];
        fq_fe a, b, r;
        unsigned char out[32];
        ran_scalar_from_bytes(a, v.a);
        ran_scalar_from_bytes(b, v.b);
        ran_scalar_add(r, a, b);
        ran_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): ran scalar add ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::sub_count; i++)
    {
        auto &v = tv::ran_scalar::sub_vectors[i];
        fq_fe a, b, r;
        unsigned char out[32];
        ran_scalar_from_bytes(a, v.a);
        ran_scalar_from_bytes(b, v.b);
        ran_scalar_sub(r, a, b);
        ran_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): ran scalar sub ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::mul_count; i++)
    {
        auto &v = tv::ran_scalar::mul_vectors[i];
        fq_fe a, b, r;
        unsigned char out[32];
        ran_scalar_from_bytes(a, v.a);
        ran_scalar_from_bytes(b, v.b);
        ran_scalar_mul(r, a, b);
        ran_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): ran scalar mul ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::sq_count; i++)
    {
        auto &v = tv::ran_scalar::sq_vectors[i];
        fq_fe a, r;
        unsigned char out[32];
        ran_scalar_from_bytes(a, v.a);
        ran_scalar_sq(r, a);
        ran_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): ran scalar sq ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::negate_count; i++)
    {
        auto &v = tv::ran_scalar::negate_vectors[i];
        fq_fe a, r;
        unsigned char out[32];
        ran_scalar_from_bytes(a, v.a);
        ran_scalar_neg(r, a);
        ran_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): ran scalar neg ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::invert_count; i++)
    {
        auto &v = tv::ran_scalar::invert_vectors[i];
        if (!v.valid)
            continue; /* C-level invert(0) is undefined */
        fq_fe a, r;
        unsigned char out[32];
        ran_scalar_from_bytes(a, v.a);
        ran_scalar_invert(r, a);
        ran_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): ran scalar inv ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::reduce_wide_count; i++)
    {
        auto &v = tv::ran_scalar::reduce_wide_vectors[i];
        fq_fe r;
        unsigned char out[32];
        ran_scalar_reduce_wide(r, v.input);
        ran_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): ran scalar reduce_wide ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::muladd_count; i++)
    {
        auto &v = tv::ran_scalar::muladd_vectors[i];
        fq_fe a, b, c, r;
        unsigned char out[32];
        ran_scalar_from_bytes(a, v.a);
        ran_scalar_from_bytes(b, v.b);
        ran_scalar_from_bytes(c, v.c);
        ran_scalar_muladd(r, a, b, c);
        ran_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): ran scalar muladd ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::ran_scalar::is_zero_count; i++)
    {
        auto &v = tv::ran_scalar::is_zero_vectors[i];
        fq_fe a;
        ran_scalar_from_bytes(a, v.a);
        check_int(
            (std::string("tv(C): ran scalar is_zero ") + v.label).c_str(), v.result ? 1 : 0, ran_scalar_is_zero(a));
    }

    /* ==== Shaw Scalar (C primitives) ==== */
    std::cout << "  --- Shaw Scalar (C) ---" << std::endl;
    for (size_t i = 0; i < tv::shaw_scalar::add_count; i++)
    {
        auto &v = tv::shaw_scalar::add_vectors[i];
        fp_fe a, b, r;
        unsigned char out[32];
        shaw_scalar_from_bytes(a, v.a);
        shaw_scalar_from_bytes(b, v.b);
        shaw_scalar_add(r, a, b);
        shaw_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): shaw scalar add ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::sub_count; i++)
    {
        auto &v = tv::shaw_scalar::sub_vectors[i];
        fp_fe a, b, r;
        unsigned char out[32];
        shaw_scalar_from_bytes(a, v.a);
        shaw_scalar_from_bytes(b, v.b);
        shaw_scalar_sub(r, a, b);
        shaw_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): shaw scalar sub ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::mul_count; i++)
    {
        auto &v = tv::shaw_scalar::mul_vectors[i];
        fp_fe a, b, r;
        unsigned char out[32];
        shaw_scalar_from_bytes(a, v.a);
        shaw_scalar_from_bytes(b, v.b);
        shaw_scalar_mul(r, a, b);
        shaw_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): shaw scalar mul ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::sq_count; i++)
    {
        auto &v = tv::shaw_scalar::sq_vectors[i];
        fp_fe a, r;
        unsigned char out[32];
        shaw_scalar_from_bytes(a, v.a);
        shaw_scalar_sq(r, a);
        shaw_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): shaw scalar sq ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::negate_count; i++)
    {
        auto &v = tv::shaw_scalar::negate_vectors[i];
        fp_fe a, r;
        unsigned char out[32];
        shaw_scalar_from_bytes(a, v.a);
        shaw_scalar_neg(r, a);
        shaw_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): shaw scalar neg ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::invert_count; i++)
    {
        auto &v = tv::shaw_scalar::invert_vectors[i];
        if (!v.valid)
            continue;
        fp_fe a, r;
        unsigned char out[32];
        shaw_scalar_from_bytes(a, v.a);
        shaw_scalar_invert(r, a);
        shaw_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): shaw scalar inv ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::reduce_wide_count; i++)
    {
        auto &v = tv::shaw_scalar::reduce_wide_vectors[i];
        fp_fe r;
        unsigned char out[32];
        shaw_scalar_reduce_wide(r, v.input);
        shaw_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): shaw scalar reduce_wide ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::muladd_count; i++)
    {
        auto &v = tv::shaw_scalar::muladd_vectors[i];
        fp_fe a, b, c, r;
        unsigned char out[32];
        shaw_scalar_from_bytes(a, v.a);
        shaw_scalar_from_bytes(b, v.b);
        shaw_scalar_from_bytes(c, v.c);
        shaw_scalar_muladd(r, a, b, c);
        shaw_scalar_to_bytes(out, r);
        check_bytes((std::string("tv(C): shaw scalar muladd ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::shaw_scalar::is_zero_count; i++)
    {
        auto &v = tv::shaw_scalar::is_zero_vectors[i];
        fp_fe a;
        shaw_scalar_from_bytes(a, v.a);
        check_int(
            (std::string("tv(C): shaw scalar is_zero ") + v.label).c_str(), v.result ? 1 : 0, shaw_scalar_is_zero(a));
    }

    /* ==== Ran Point (C primitives) ==== */
    std::cout << "  --- Ran Point (C) ---" << std::endl;
    for (size_t i = 0; i < tv::ran_point::from_bytes_count; i++)
    {
        auto &v = tv::ran_point::from_bytes_vectors[i];
        ran_jacobian p;
        int ok = ran_frombytes(&p, v.input);
        std::string name = std::string("tv(C): ran point from_bytes ") + v.label;
        if (v.valid)
        {
            check_int((name + " valid").c_str(), 0, ok);
            if (ok == 0)
            {
                unsigned char out[32];
                ran_tobytes(out, &p);
                check_bytes((name + " value").c_str(), v.result, out, 32);
            }
        }
        else
        {
            check_int((name + " invalid").c_str(), 1, (ok != 0) ? 1 : 0);
        }
    }
    for (size_t i = 0; i < tv::ran_point::add_count; i++)
    {
        auto &v = tv::ran_point::add_vectors[i];
        ran_jacobian a, b, r;
        h_load(&a, v.a);
        h_load(&b, v.b);
        ran_add(&r, &a, &b);
        unsigned char out[32];
        ran_tobytes(out, &r);
        check_bytes((std::string("tv(C): ran point add ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::ran_point::dbl_count; i++)
    {
        auto &v = tv::ran_point::dbl_vectors[i];
        ran_jacobian a, r;
        h_load(&a, v.a);
        ran_dbl(&r, &a);
        unsigned char out[32];
        ran_tobytes(out, &r);
        check_bytes((std::string("tv(C): ran point dbl ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::ran_point::negate_count; i++)
    {
        auto &v = tv::ran_point::negate_vectors[i];
        ran_jacobian a, r;
        h_load(&a, v.a);
        ran_neg(&r, &a);
        unsigned char out[32];
        ran_tobytes(out, &r);
        check_bytes((std::string("tv(C): ran point neg ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::ran_point::scalar_mul_count; i++)
    {
        auto &v = tv::ran_point::scalar_mul_vectors[i];
        ran_jacobian p, r;
        h_load(&p, v.point);
        ran_scalarmult(&r, v.scalar, &p);
        unsigned char out[32];
        ran_tobytes(out, &r);
        check_bytes((std::string("tv(C): ran point scalarmult ") + v.label).c_str(), v.result, out, 32);
    }

    /* MSM (ran) */
    {
        auto run_msm = [&](const char *name,
                           const uint8_t(*scalars)[32],
                           const uint8_t(*points)[32],
                           const uint8_t expected[32],
                           size_t n)
        {
            std::vector<ran_jacobian> pts(n);
            for (size_t j = 0; j < n; j++)
                h_load(&pts[j], points[j]);
            ran_jacobian r;
            ran_msm_vartime(&r, scalars[0], pts.data(), n);
            unsigned char out[32];
            ran_tobytes(out, &r);
            check_bytes(name, expected, out, 32);
        };
        namespace hp = tv::ran_point;
        run_msm("tv(C): ran msm n=1", hp::msm_n_1_scalars, hp::msm_n_1_points, hp::msm_n_1_result, 1);
        run_msm("tv(C): ran msm n=2", hp::msm_n_2_scalars, hp::msm_n_2_points, hp::msm_n_2_result, 2);
        run_msm("tv(C): ran msm n=4", hp::msm_n_4_scalars, hp::msm_n_4_points, hp::msm_n_4_result, 4);
        run_msm("tv(C): ran msm n=16", hp::msm_n_16_scalars, hp::msm_n_16_points, hp::msm_n_16_result, 16);
        run_msm(
            "tv(C): ran msm n=32",
            hp::msm_n_32_straus_scalars,
            hp::msm_n_32_straus_points,
            hp::msm_n_32_straus_result,
            32);
        run_msm(
            "tv(C): ran msm n=33",
            hp::msm_n_33_pippenger_scalars,
            hp::msm_n_33_pippenger_points,
            hp::msm_n_33_pippenger_result,
            33);
        run_msm(
            "tv(C): ran msm n=64",
            hp::msm_n_64_pippenger_scalars,
            hp::msm_n_64_pippenger_points,
            hp::msm_n_64_pippenger_result,
            64);
    }

    /* Pedersen (ran) */
    {
        auto run_ped = [&](const char *name,
                           const uint8_t blinding[32],
                           const uint8_t H_bytes[32],
                           const uint8_t(*values)[32],
                           const uint8_t(*generators)[32],
                           const uint8_t expected[32],
                           size_t n)
        {
            ran_jacobian H_pt;
            h_load(&H_pt, H_bytes);
            std::vector<ran_jacobian> gens(n);
            for (size_t j = 0; j < n; j++)
                h_load(&gens[j], generators[j]);
            ran_jacobian r;
            ran_pedersen_commit(&r, blinding, &H_pt, values[0], gens.data(), n);
            unsigned char out[32];
            ran_tobytes(out, &r);
            check_bytes(name, expected, out, 32);
        };
        namespace hp = tv::ran_point;
        run_ped(
            "tv(C): ran pedersen n=1",
            hp::pedersen_n_1_blinding,
            hp::pedersen_n_1_H,
            hp::pedersen_n_1_values,
            hp::pedersen_n_1_generators,
            hp::pedersen_n_1_result,
            1);
        run_ped(
            "tv(C): ran pedersen n=4",
            hp::pedersen_n_4_blinding,
            hp::pedersen_n_4_H,
            hp::pedersen_n_4_values,
            hp::pedersen_n_4_generators,
            hp::pedersen_n_4_result,
            4);
        run_ped(
            "tv(C): ran pedersen blind=0",
            hp::pedersen_blinding_zero_blinding,
            hp::pedersen_blinding_zero_H,
            hp::pedersen_blinding_zero_values,
            hp::pedersen_blinding_zero_generators,
            hp::pedersen_blinding_zero_result,
            1);
    }

    /* Map-to-curve (ran) */
    for (size_t i = 0; i < tv::ran_point::map_to_curve_single_count; i++)
    {
        auto &v = tv::ran_point::map_to_curve_single_vectors[i];
        ran_jacobian r;
        ran_map_to_curve(&r, v.u);
        unsigned char out[32];
        ran_tobytes(out, &r);
        check_bytes((std::string("tv(C): ran map_to_curve ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::ran_point::map_to_curve_double_count; i++)
    {
        auto &v = tv::ran_point::map_to_curve_double_vectors[i];
        ran_jacobian r;
        ran_map_to_curve2(&r, v.u0, v.u1);
        unsigned char out[32];
        ran_tobytes(out, &r);
        check_bytes((std::string("tv(C): ran map_to_curve2 ") + v.label).c_str(), v.result, out, 32);
    }

    /* ==== Shaw Point (C primitives) ==== */
    std::cout << "  --- Shaw Point (C) ---" << std::endl;
    for (size_t i = 0; i < tv::shaw_point::from_bytes_count; i++)
    {
        auto &v = tv::shaw_point::from_bytes_vectors[i];
        shaw_jacobian p;
        int ok = shaw_frombytes(&p, v.input);
        std::string name = std::string("tv(C): shaw point from_bytes ") + v.label;
        if (v.valid)
        {
            check_int((name + " valid").c_str(), 0, ok);
            if (ok == 0)
            {
                unsigned char out[32];
                shaw_tobytes(out, &p);
                check_bytes((name + " value").c_str(), v.result, out, 32);
            }
        }
        else
        {
            check_int((name + " invalid").c_str(), 1, (ok != 0) ? 1 : 0);
        }
    }
    for (size_t i = 0; i < tv::shaw_point::add_count; i++)
    {
        auto &v = tv::shaw_point::add_vectors[i];
        shaw_jacobian a, b, r;
        s_load(&a, v.a);
        s_load(&b, v.b);
        shaw_add(&r, &a, &b);
        unsigned char out[32];
        shaw_tobytes(out, &r);
        check_bytes((std::string("tv(C): shaw point add ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::shaw_point::dbl_count; i++)
    {
        auto &v = tv::shaw_point::dbl_vectors[i];
        shaw_jacobian a, r;
        s_load(&a, v.a);
        shaw_dbl(&r, &a);
        unsigned char out[32];
        shaw_tobytes(out, &r);
        check_bytes((std::string("tv(C): shaw point dbl ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::shaw_point::negate_count; i++)
    {
        auto &v = tv::shaw_point::negate_vectors[i];
        shaw_jacobian a, r;
        s_load(&a, v.a);
        shaw_neg(&r, &a);
        unsigned char out[32];
        shaw_tobytes(out, &r);
        check_bytes((std::string("tv(C): shaw point neg ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::shaw_point::scalar_mul_count; i++)
    {
        auto &v = tv::shaw_point::scalar_mul_vectors[i];
        shaw_jacobian p, r;
        s_load(&p, v.point);
        shaw_scalarmult(&r, v.scalar, &p);
        unsigned char out[32];
        shaw_tobytes(out, &r);
        check_bytes((std::string("tv(C): shaw point scalarmult ") + v.label).c_str(), v.result, out, 32);
    }

    /* MSM (shaw) */
    {
        auto run_msm = [&](const char *name,
                           const uint8_t(*scalars)[32],
                           const uint8_t(*points)[32],
                           const uint8_t expected[32],
                           size_t n)
        {
            std::vector<shaw_jacobian> pts(n);
            for (size_t j = 0; j < n; j++)
                s_load(&pts[j], points[j]);
            shaw_jacobian r;
            shaw_msm_vartime(&r, scalars[0], pts.data(), n);
            unsigned char out[32];
            shaw_tobytes(out, &r);
            check_bytes(name, expected, out, 32);
        };
        namespace sp = tv::shaw_point;
        run_msm("tv(C): shaw msm n=1", sp::msm_n_1_scalars, sp::msm_n_1_points, sp::msm_n_1_result, 1);
        run_msm("tv(C): shaw msm n=2", sp::msm_n_2_scalars, sp::msm_n_2_points, sp::msm_n_2_result, 2);
        run_msm("tv(C): shaw msm n=4", sp::msm_n_4_scalars, sp::msm_n_4_points, sp::msm_n_4_result, 4);
        run_msm("tv(C): shaw msm n=16", sp::msm_n_16_scalars, sp::msm_n_16_points, sp::msm_n_16_result, 16);
        run_msm(
            "tv(C): shaw msm n=32",
            sp::msm_n_32_straus_scalars,
            sp::msm_n_32_straus_points,
            sp::msm_n_32_straus_result,
            32);
        run_msm(
            "tv(C): shaw msm n=33",
            sp::msm_n_33_pippenger_scalars,
            sp::msm_n_33_pippenger_points,
            sp::msm_n_33_pippenger_result,
            33);
        run_msm(
            "tv(C): shaw msm n=64",
            sp::msm_n_64_pippenger_scalars,
            sp::msm_n_64_pippenger_points,
            sp::msm_n_64_pippenger_result,
            64);
    }

    /* Pedersen (shaw) */
    {
        auto run_ped = [&](const char *name,
                           const uint8_t blinding[32],
                           const uint8_t H_bytes[32],
                           const uint8_t(*values)[32],
                           const uint8_t(*generators)[32],
                           const uint8_t expected[32],
                           size_t n)
        {
            shaw_jacobian H_pt;
            s_load(&H_pt, H_bytes);
            std::vector<shaw_jacobian> gens(n);
            for (size_t j = 0; j < n; j++)
                s_load(&gens[j], generators[j]);
            shaw_jacobian r;
            shaw_pedersen_commit(&r, blinding, &H_pt, values[0], gens.data(), n);
            unsigned char out[32];
            shaw_tobytes(out, &r);
            check_bytes(name, expected, out, 32);
        };
        namespace sp = tv::shaw_point;
        run_ped(
            "tv(C): shaw pedersen n=1",
            sp::pedersen_n_1_blinding,
            sp::pedersen_n_1_H,
            sp::pedersen_n_1_values,
            sp::pedersen_n_1_generators,
            sp::pedersen_n_1_result,
            1);
        run_ped(
            "tv(C): shaw pedersen n=4",
            sp::pedersen_n_4_blinding,
            sp::pedersen_n_4_H,
            sp::pedersen_n_4_values,
            sp::pedersen_n_4_generators,
            sp::pedersen_n_4_result,
            4);
        run_ped(
            "tv(C): shaw pedersen blind=0",
            sp::pedersen_blinding_zero_blinding,
            sp::pedersen_blinding_zero_H,
            sp::pedersen_blinding_zero_values,
            sp::pedersen_blinding_zero_generators,
            sp::pedersen_blinding_zero_result,
            1);
    }

    /* Map-to-curve (shaw) */
    for (size_t i = 0; i < tv::shaw_point::map_to_curve_single_count; i++)
    {
        auto &v = tv::shaw_point::map_to_curve_single_vectors[i];
        shaw_jacobian r;
        shaw_map_to_curve(&r, v.u);
        unsigned char out[32];
        shaw_tobytes(out, &r);
        check_bytes((std::string("tv(C): shaw map_to_curve ") + v.label).c_str(), v.result, out, 32);
    }
    for (size_t i = 0; i < tv::shaw_point::map_to_curve_double_count; i++)
    {
        auto &v = tv::shaw_point::map_to_curve_double_vectors[i];
        shaw_jacobian r;
        shaw_map_to_curve2(&r, v.u0, v.u1);
        unsigned char out[32];
        shaw_tobytes(out, &r);
        check_bytes((std::string("tv(C): shaw map_to_curve2 ") + v.label).c_str(), v.result, out, 32);
    }

    /* ==== Fp Polynomial (C primitives) ==== */
    std::cout << "  --- Fp Polynomial (C) ---" << std::endl;
    {
        namespace fp = tv::fp_polynomial;

        /* Helper: build fp_poly from test vector coefficient bytes */
        auto make_fp_poly = [](const uint8_t(*coeffs)[32], size_t n) -> fp_poly
        {
            fp_poly p;
            p.coeffs.resize(n);
            for (size_t i = 0; i < n; i++)
                fp_frombytes(p.coeffs[i].v, coeffs[i]);
            return p;
        };

        /* from_roots */
        {
            fp_fe roots[1];
            fp_frombytes(roots[0], fp::from_roots_one_root_roots[0]);
            fp_poly p;
            fp_poly_from_roots(&p, roots, 1);
            size_t n = sizeof(fp::from_roots_one_root_coefficients) / sizeof(fp::from_roots_one_root_coefficients[0]);
            for (size_t i = 0; i < n && i < p.coeffs.size(); i++)
            {
                uint8_t c[32];
                fp_tobytes(c, p.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): fp poly from_roots 1 coeff[") + std::to_string(i) + "]").c_str(),
                    fp::from_roots_one_root_coefficients[i],
                    c,
                    32);
            }
        }
        {
            fp_fe roots[4];
            for (int j = 0; j < 4; j++)
                fp_frombytes(roots[j], fp::from_roots_four_roots_roots[j]);
            fp_poly p;
            fp_poly_from_roots(&p, roots, 4);
            size_t n =
                sizeof(fp::from_roots_four_roots_coefficients) / sizeof(fp::from_roots_four_roots_coefficients[0]);
            for (size_t i = 0; i < n && i < p.coeffs.size(); i++)
            {
                uint8_t c[32];
                fp_tobytes(c, p.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): fp poly from_roots 4 coeff[") + std::to_string(i) + "]").c_str(),
                    fp::from_roots_four_roots_coefficients[i],
                    c,
                    32);
            }
        }

        /* eval */
        {
            size_t nc = sizeof(fp::eval_quadratic_at_7_coefficients) / sizeof(fp::eval_quadratic_at_7_coefficients[0]);
            auto p = make_fp_poly(fp::eval_quadratic_at_7_coefficients, nc);
            fp_fe x, result;
            fp_frombytes(x, fp::eval_quadratic_at_7_x);
            fp_poly_eval(result, &p, x);
            uint8_t out[32];
            fp_tobytes(out, result);
            check_bytes("tv(C): fp poly eval quadratic_at_7", fp::eval_quadratic_at_7_result, out, 32);
        }

        /* mul: deg1*deg1 */
        {
            size_t na = sizeof(fp::mul_deg1_times_deg1_a) / sizeof(fp::mul_deg1_times_deg1_a[0]);
            size_t nb = sizeof(fp::mul_deg1_times_deg1_b) / sizeof(fp::mul_deg1_times_deg1_b[0]);
            size_t nr = sizeof(fp::mul_deg1_times_deg1_result) / sizeof(fp::mul_deg1_times_deg1_result[0]);
            auto a = make_fp_poly(fp::mul_deg1_times_deg1_a, na);
            auto b = make_fp_poly(fp::mul_deg1_times_deg1_b, nb);
            fp_poly r;
            fp_poly_mul(&r, &a, &b);
            for (size_t i = 0; i < nr && i < r.coeffs.size(); i++)
            {
                uint8_t c[32];
                fp_tobytes(c, r.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): fp poly mul deg1*deg1 coeff[") + std::to_string(i) + "]").c_str(),
                    fp::mul_deg1_times_deg1_result[i],
                    c,
                    32);
            }
        }

        /* mul: deg16*deg16 (karatsuba) */
        {
            size_t na =
                sizeof(fp::mul_deg16_times_deg16_karatsuba_a) / sizeof(fp::mul_deg16_times_deg16_karatsuba_a[0]);
            size_t nb =
                sizeof(fp::mul_deg16_times_deg16_karatsuba_b) / sizeof(fp::mul_deg16_times_deg16_karatsuba_b[0]);
            size_t nr = sizeof(fp::mul_deg16_times_deg16_karatsuba_result)
                        / sizeof(fp::mul_deg16_times_deg16_karatsuba_result[0]);
            auto a = make_fp_poly(fp::mul_deg16_times_deg16_karatsuba_a, na);
            auto b = make_fp_poly(fp::mul_deg16_times_deg16_karatsuba_b, nb);
            fp_poly r;
            fp_poly_mul(&r, &a, &b);
            for (size_t i = 0; i < nr && i < r.coeffs.size(); i++)
            {
                uint8_t c[32];
                fp_tobytes(c, r.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): fp poly mul deg16*deg16 coeff[") + std::to_string(i) + "]").c_str(),
                    fp::mul_deg16_times_deg16_karatsuba_result[i],
                    c,
                    32);
            }
        }

        /* divmod: exact division */
        {
            size_t nn = sizeof(fp::divmod_exact_division_numerator) / sizeof(fp::divmod_exact_division_numerator[0]);
            size_t nd =
                sizeof(fp::divmod_exact_division_denominator) / sizeof(fp::divmod_exact_division_denominator[0]);
            size_t nq = sizeof(fp::divmod_exact_division_quotient) / sizeof(fp::divmod_exact_division_quotient[0]);
            size_t nrem = sizeof(fp::divmod_exact_division_remainder) / sizeof(fp::divmod_exact_division_remainder[0]);
            auto num = make_fp_poly(fp::divmod_exact_division_numerator, nn);
            auto den = make_fp_poly(fp::divmod_exact_division_denominator, nd);
            fp_poly q, rem;
            fp_poly_divmod(&q, &rem, &num, &den);
            for (size_t i = 0; i < nq && i < q.coeffs.size(); i++)
            {
                uint8_t c[32];
                fp_tobytes(c, q.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): fp poly divmod exact q[") + std::to_string(i) + "]").c_str(),
                    fp::divmod_exact_division_quotient[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nrem && i < rem.coeffs.size(); i++)
            {
                uint8_t c[32];
                fp_tobytes(c, rem.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): fp poly divmod exact r[") + std::to_string(i) + "]").c_str(),
                    fp::divmod_exact_division_remainder[i],
                    c,
                    32);
            }
        }

        /* interpolate: three points */
        {
            fp_fe xs[3], ys[3];
            for (int j = 0; j < 3; j++)
            {
                fp_frombytes(xs[j], fp::interp_three_points_xs[j]);
                fp_frombytes(ys[j], fp::interp_three_points_ys[j]);
            }
            fp_poly p;
            fp_poly_interpolate(&p, xs, ys, 3);
            size_t nc = sizeof(fp::interp_three_points_coefficients) / sizeof(fp::interp_three_points_coefficients[0]);
            for (size_t i = 0; i < nc && i < p.coeffs.size(); i++)
            {
                uint8_t c[32];
                fp_tobytes(c, p.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): fp poly interp 3pt coeff[") + std::to_string(i) + "]").c_str(),
                    fp::interp_three_points_coefficients[i],
                    c,
                    32);
            }
        }
    }

    /* ==== Fq Polynomial (C primitives) ==== */
    std::cout << "  --- Fq Polynomial (C) ---" << std::endl;
    {
        namespace fqn = tv::fq_polynomial;

        auto make_fq_poly = [](const uint8_t(*coeffs)[32], size_t n) -> fq_poly
        {
            fq_poly p;
            p.coeffs.resize(n);
            for (size_t i = 0; i < n; i++)
                fq_frombytes(p.coeffs[i].v, coeffs[i]);
            return p;
        };

        /* from_roots */
        {
            fq_fe roots[4];
            for (int j = 0; j < 4; j++)
                fq_frombytes(roots[j], fqn::from_roots_four_roots_roots[j]);
            fq_poly p;
            fq_poly_from_roots(&p, roots, 4);
            size_t n =
                sizeof(fqn::from_roots_four_roots_coefficients) / sizeof(fqn::from_roots_four_roots_coefficients[0]);
            for (size_t i = 0; i < n && i < p.coeffs.size(); i++)
            {
                uint8_t c[32];
                fq_tobytes(c, p.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): fq poly from_roots 4 coeff[") + std::to_string(i) + "]").c_str(),
                    fqn::from_roots_four_roots_coefficients[i],
                    c,
                    32);
            }
        }

        /* eval */
        {
            size_t nc =
                sizeof(fqn::eval_quadratic_at_7_coefficients) / sizeof(fqn::eval_quadratic_at_7_coefficients[0]);
            auto p = make_fq_poly(fqn::eval_quadratic_at_7_coefficients, nc);
            fq_fe x, result;
            fq_frombytes(x, fqn::eval_quadratic_at_7_x);
            fq_poly_eval(result, &p, x);
            uint8_t out[32];
            fq_tobytes(out, result);
            check_bytes("tv(C): fq poly eval quadratic_at_7", fqn::eval_quadratic_at_7_result, out, 32);
        }

        /* mul: deg16*deg16 (karatsuba) */
        {
            size_t na =
                sizeof(fqn::mul_deg16_times_deg16_karatsuba_a) / sizeof(fqn::mul_deg16_times_deg16_karatsuba_a[0]);
            size_t nb =
                sizeof(fqn::mul_deg16_times_deg16_karatsuba_b) / sizeof(fqn::mul_deg16_times_deg16_karatsuba_b[0]);
            size_t nr = sizeof(fqn::mul_deg16_times_deg16_karatsuba_result)
                        / sizeof(fqn::mul_deg16_times_deg16_karatsuba_result[0]);
            auto a = make_fq_poly(fqn::mul_deg16_times_deg16_karatsuba_a, na);
            auto b = make_fq_poly(fqn::mul_deg16_times_deg16_karatsuba_b, nb);
            fq_poly r;
            fq_poly_mul(&r, &a, &b);
            for (size_t i = 0; i < nr && i < r.coeffs.size(); i++)
            {
                uint8_t c[32];
                fq_tobytes(c, r.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): fq poly mul deg16*deg16 coeff[") + std::to_string(i) + "]").c_str(),
                    fqn::mul_deg16_times_deg16_karatsuba_result[i],
                    c,
                    32);
            }
        }

        /* divmod: exact division */
        {
            size_t nn = sizeof(fqn::divmod_exact_division_numerator) / sizeof(fqn::divmod_exact_division_numerator[0]);
            size_t nd =
                sizeof(fqn::divmod_exact_division_denominator) / sizeof(fqn::divmod_exact_division_denominator[0]);
            size_t nq = sizeof(fqn::divmod_exact_division_quotient) / sizeof(fqn::divmod_exact_division_quotient[0]);
            size_t nrem =
                sizeof(fqn::divmod_exact_division_remainder) / sizeof(fqn::divmod_exact_division_remainder[0]);
            auto num = make_fq_poly(fqn::divmod_exact_division_numerator, nn);
            auto den = make_fq_poly(fqn::divmod_exact_division_denominator, nd);
            fq_poly q, rem;
            fq_poly_divmod(&q, &rem, &num, &den);
            for (size_t i = 0; i < nq && i < q.coeffs.size(); i++)
            {
                uint8_t c[32];
                fq_tobytes(c, q.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): fq poly divmod exact q[") + std::to_string(i) + "]").c_str(),
                    fqn::divmod_exact_division_quotient[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nrem && i < rem.coeffs.size(); i++)
            {
                uint8_t c[32];
                fq_tobytes(c, rem.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): fq poly divmod exact r[") + std::to_string(i) + "]").c_str(),
                    fqn::divmod_exact_division_remainder[i],
                    c,
                    32);
            }
        }

        /* interpolate: three points */
        {
            fq_fe xs[3], ys[3];
            for (int j = 0; j < 3; j++)
            {
                fq_frombytes(xs[j], fqn::interp_three_points_xs[j]);
                fq_frombytes(ys[j], fqn::interp_three_points_ys[j]);
            }
            fq_poly p;
            fq_poly_interpolate(&p, xs, ys, 3);
            size_t nc =
                sizeof(fqn::interp_three_points_coefficients) / sizeof(fqn::interp_three_points_coefficients[0]);
            for (size_t i = 0; i < nc && i < p.coeffs.size(); i++)
            {
                uint8_t c[32];
                fq_tobytes(c, p.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): fq poly interp 3pt coeff[") + std::to_string(i) + "]").c_str(),
                    fqn::interp_three_points_coefficients[i],
                    c,
                    32);
            }
        }
    }

    /* ==== Ran Divisor (C primitives) ==== */
    std::cout << "  --- Ran Divisor (C) ---" << std::endl;
    {
        namespace hd = tv::ran_divisor;

        auto run_divisor = [&](const char *label,
                               const uint8_t(*pt_bytes)[32],
                               size_t n,
                               const uint8_t(*a_coeffs)[32],
                               size_t na,
                               const uint8_t(*b_coeffs)[32],
                               size_t nb,
                               const uint8_t eval_x[32],
                               const uint8_t eval_y[32],
                               const uint8_t eval_expected[32])
        {
            /* Load affine points */
            std::vector<ran_affine> pts(n);
            for (size_t j = 0; j < n; j++)
            {
                ran_jacobian jac;
                ran_frombytes(&jac, pt_bytes[j]);
                ran_to_affine(&pts[j], &jac);
            }
            ran_divisor d;
            ran_compute_divisor(&d, pts.data(), n);
            for (size_t i = 0; i < na && i < d.a.coeffs.size(); i++)
            {
                uint8_t c[32];
                fp_tobytes(c, d.a.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): ran div ") + label + " a[" + std::to_string(i) + "]").c_str(),
                    a_coeffs[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nb && i < d.b.coeffs.size(); i++)
            {
                uint8_t c[32];
                fp_tobytes(c, d.b.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): ran div ") + label + " b[" + std::to_string(i) + "]").c_str(),
                    b_coeffs[i],
                    c,
                    32);
            }
            fp_fe ex, ey, ev;
            fp_frombytes(ex, eval_x);
            fp_frombytes(ey, eval_y);
            ran_evaluate_divisor(ev, &d, ex, ey);
            uint8_t out[32];
            fp_tobytes(out, ev);
            check_bytes((std::string("tv(C): ran div ") + label + " eval").c_str(), eval_expected, out, 32);
        };

        run_divisor(
            "n=2",
            hd::n_2_points,
            2,
            hd::n_2_a_coefficients,
            sizeof(hd::n_2_a_coefficients) / sizeof(hd::n_2_a_coefficients[0]),
            hd::n_2_b_coefficients,
            sizeof(hd::n_2_b_coefficients) / sizeof(hd::n_2_b_coefficients[0]),
            hd::n_2_eval_point_x,
            hd::n_2_eval_point_y,
            hd::n_2_eval_result);
        run_divisor(
            "n=4",
            hd::n_4_points,
            4,
            hd::n_4_a_coefficients,
            sizeof(hd::n_4_a_coefficients) / sizeof(hd::n_4_a_coefficients[0]),
            hd::n_4_b_coefficients,
            sizeof(hd::n_4_b_coefficients) / sizeof(hd::n_4_b_coefficients[0]),
            hd::n_4_eval_point_x,
            hd::n_4_eval_point_y,
            hd::n_4_eval_result);
        run_divisor(
            "n=8",
            hd::n_8_points,
            8,
            hd::n_8_a_coefficients,
            sizeof(hd::n_8_a_coefficients) / sizeof(hd::n_8_a_coefficients[0]),
            hd::n_8_b_coefficients,
            sizeof(hd::n_8_b_coefficients) / sizeof(hd::n_8_b_coefficients[0]),
            hd::n_8_eval_point_x,
            hd::n_8_eval_point_y,
            hd::n_8_eval_result);
    }

    /* ==== Shaw Divisor (C primitives) ==== */
    std::cout << "  --- Shaw Divisor (C) ---" << std::endl;
    {
        namespace sd = tv::shaw_divisor;

        auto run_divisor = [&](const char *label,
                               const uint8_t(*pt_bytes)[32],
                               size_t n,
                               const uint8_t(*a_coeffs)[32],
                               size_t na,
                               const uint8_t(*b_coeffs)[32],
                               size_t nb,
                               const uint8_t eval_x[32],
                               const uint8_t eval_y[32],
                               const uint8_t eval_expected[32])
        {
            std::vector<shaw_affine> pts(n);
            for (size_t j = 0; j < n; j++)
            {
                shaw_jacobian jac;
                shaw_frombytes(&jac, pt_bytes[j]);
                shaw_to_affine(&pts[j], &jac);
            }
            shaw_divisor d;
            shaw_compute_divisor(&d, pts.data(), n);
            for (size_t i = 0; i < na && i < d.a.coeffs.size(); i++)
            {
                uint8_t c[32];
                fq_tobytes(c, d.a.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): shaw div ") + label + " a[" + std::to_string(i) + "]").c_str(),
                    a_coeffs[i],
                    c,
                    32);
            }
            for (size_t i = 0; i < nb && i < d.b.coeffs.size(); i++)
            {
                uint8_t c[32];
                fq_tobytes(c, d.b.coeffs[i].v);
                check_bytes(
                    (std::string("tv(C): shaw div ") + label + " b[" + std::to_string(i) + "]").c_str(),
                    b_coeffs[i],
                    c,
                    32);
            }
            fq_fe ex, ey, ev;
            fq_frombytes(ex, eval_x);
            fq_frombytes(ey, eval_y);
            shaw_evaluate_divisor(ev, &d, ex, ey);
            uint8_t out[32];
            fq_tobytes(out, ev);
            check_bytes((std::string("tv(C): shaw div ") + label + " eval").c_str(), eval_expected, out, 32);
        };

        run_divisor(
            "n=2",
            sd::n_2_points,
            2,
            sd::n_2_a_coefficients,
            sizeof(sd::n_2_a_coefficients) / sizeof(sd::n_2_a_coefficients[0]),
            sd::n_2_b_coefficients,
            sizeof(sd::n_2_b_coefficients) / sizeof(sd::n_2_b_coefficients[0]),
            sd::n_2_eval_point_x,
            sd::n_2_eval_point_y,
            sd::n_2_eval_result);
        run_divisor(
            "n=4",
            sd::n_4_points,
            4,
            sd::n_4_a_coefficients,
            sizeof(sd::n_4_a_coefficients) / sizeof(sd::n_4_a_coefficients[0]),
            sd::n_4_b_coefficients,
            sizeof(sd::n_4_b_coefficients) / sizeof(sd::n_4_b_coefficients[0]),
            sd::n_4_eval_point_x,
            sd::n_4_eval_point_y,
            sd::n_4_eval_result);
        run_divisor(
            "n=8",
            sd::n_8_points,
            8,
            sd::n_8_a_coefficients,
            sizeof(sd::n_8_a_coefficients) / sizeof(sd::n_8_a_coefficients[0]),
            sd::n_8_b_coefficients,
            sizeof(sd::n_8_b_coefficients) / sizeof(sd::n_8_b_coefficients[0]),
            sd::n_8_eval_point_x,
            sd::n_8_eval_point_y,
            sd::n_8_eval_result);
    }
}
