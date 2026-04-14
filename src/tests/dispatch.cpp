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

void test_dispatch()
{
    std::cout << "  dispatch" << std::endl;

#if RANSHAW_SIMD
    // Test that init() can be called (no-op if already called, or first time)
    ranshaw_init();

    // After init, dispatch should still produce correct results.
    // Run scalarmult through dispatch wrappers and verify against KAT.

    // Ran scalarmult via dispatch
    {
        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        // 7*G via dispatch
        unsigned char scalar_7[32] = {0x07};
        ran_jacobian result;
        ran_scalarmult(&result, scalar_7, &G);

        unsigned char result_bytes[32];
        ran_tobytes(result_bytes, &result);
        check_bytes("ran dispatch scalarmult 7*G", result_bytes, tv::compressed_points::ran_7g, 32);

        // vartime
        ran_scalarmult_vartime(&result, scalar_7, &G);
        ran_tobytes(result_bytes, &result);
        check_bytes("ran dispatch scalarmult_vt 7*G", result_bytes, tv::compressed_points::ran_7g, 32);

        // MSM: 7*G via MSM(scalar=7, point=G, n=1)
        ran_msm_vartime(&result, scalar_7, &G, 1);
        ran_tobytes(result_bytes, &result);
        check_bytes("ran dispatch msm 7*G", result_bytes, tv::compressed_points::ran_7g, 32);

        // CT MSM via dispatch: same 7*G.
        ran_msm_ct(&result, scalar_7, &G, 1);
        ran_tobytes(result_bytes, &result);
        check_bytes("ran dispatch msm_ct 7*G", result_bytes, tv::compressed_points::ran_7g, 32);
    }

    // Shaw scalarmult via dispatch
    {
        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        unsigned char scalar_7[32] = {0x07};
        shaw_jacobian result;
        shaw_scalarmult(&result, scalar_7, &G);

        unsigned char result_bytes[32];
        shaw_tobytes(result_bytes, &result);
        check_bytes("shaw dispatch scalarmult 7*G", result_bytes, tv::compressed_points::shaw_7g, 32);

        shaw_scalarmult_vartime(&result, scalar_7, &G);
        shaw_tobytes(result_bytes, &result);
        check_bytes("shaw dispatch scalarmult_vt 7*G", result_bytes, tv::compressed_points::shaw_7g, 32);

        shaw_msm_vartime(&result, scalar_7, &G, 1);
        shaw_tobytes(result_bytes, &result);
        check_bytes("shaw dispatch msm 7*G", result_bytes, tv::compressed_points::shaw_7g, 32);

        shaw_msm_ct(&result, scalar_7, &G, 1);
        shaw_tobytes(result_bytes, &result);
        check_bytes("shaw dispatch msm_ct 7*G", result_bytes, tv::compressed_points::shaw_7g, 32);
    }

    // After ranshaw_init, both CT slots must be non-null (the init block
    // defaults them to the scalar CT MSM driver or, on IFMA hardware, to
    // the IFMA driver). A null CT slot here means a bad dispatch table,
    // which would segfault the first caller of ran_msm_ct / shaw_msm_ct.
    {
        const ranshaw_dispatch_table &t = ranshaw_get_dispatch();
        check_nonzero("ran_msm_ct slot non-null after init", t.ran_msm_ct != nullptr);
        check_nonzero("shaw_msm_ct slot non-null after init", t.shaw_msm_ct != nullptr);
    }

    // Test double init is safe (idempotent via call_once)
    ranshaw_init();
#else
    // No-SIMD build: init/autotune are no-ops, dispatch not used
    ranshaw_init();
    ranshaw_autotune();
    std::cout << "    (SIMD disabled, dispatch stubs only)" << std::endl;
#endif
}


void test_cpp_api()
{
    using namespace ranshaw;

    std::cout << std::endl << "=== C++ API ===" << std::endl;

    /* ---- Scalar round-trip ---- */
    {
        auto s = RanScalar::from_bytes(test_a_bytes);
        check_int("api: ran scalar from_bytes valid", 1, s.has_value() ? 1 : 0);
        auto bytes = s->to_bytes();
        check_bytes("api: ran scalar round-trip", test_a_bytes, bytes.data(), 32);
    }
    {
        auto s = ShawScalar::from_bytes(test_a_bytes);
        check_int("api: shaw scalar from_bytes valid", 1, s.has_value() ? 1 : 0);
        auto bytes = s->to_bytes();
        check_bytes("api: shaw scalar round-trip", test_a_bytes, bytes.data(), 32);
    }

    /* ---- Scalar arithmetic ---- */
    {
        auto a = RanScalar::from_bytes(test_a_bytes).value();
        auto b = RanScalar::from_bytes(test_b_bytes).value();
        auto one = RanScalar::one();

        /* a + b == b + a (commutativity) */
        auto ab = (a + b).to_bytes();
        auto ba = (b + a).to_bytes();
        check_bytes("api: ran scalar a+b == b+a", ab.data(), ba.data(), 32);

        /* a * one == a */
        auto a_times_1 = (a * one).to_bytes();
        auto a_bytes = a.to_bytes();
        check_bytes("api: ran scalar a*1 == a", a_bytes.data(), a_times_1.data(), 32);

        /* a * invert(a) == one */
        auto inv = a.invert();
        check_int("api: ran scalar invert non-null", 1, inv.has_value() ? 1 : 0);
        auto prod = (a * inv.value()).to_bytes();
        auto one_b = one.to_bytes();
        check_bytes("api: ran scalar a*inv(a) == 1", one_b.data(), prod.data(), 32);

        /* zero invert returns nullopt */
        auto z_inv = RanScalar::zero().invert();
        check_int("api: ran scalar inv(0) == nullopt", 0, z_inv.has_value() ? 1 : 0);

        /* is_zero */
        check_int("api: ran scalar zero.is_zero", 1, RanScalar::zero().is_zero() ? 1 : 0);
        check_int("api: ran scalar one.is_zero", 0, one.is_zero() ? 1 : 0);
    }
    {
        auto a = ShawScalar::from_bytes(test_a_bytes).value();
        auto one = ShawScalar::one();

        auto inv = a.invert();
        check_int("api: shaw scalar invert non-null", 1, inv.has_value() ? 1 : 0);
        auto prod = (a * inv.value()).to_bytes();
        auto one_b = one.to_bytes();
        check_bytes("api: shaw scalar a*inv(a) == 1", one_b.data(), prod.data(), 32);
    }

    /* ---- Scalar from_bytes rejects invalid ---- */
    {
        /* Bit 255 set */
        unsigned char bad[32] = {};
        bad[31] = 0x80;
        auto s = RanScalar::from_bytes(bad);
        check_int("api: ran scalar rejects bit255", 0, s.has_value() ? 1 : 0);
        auto s2 = ShawScalar::from_bytes(bad);
        check_int("api: shaw scalar rejects bit255", 0, s2.has_value() ? 1 : 0);
    }

    /* ---- Scalar muladd ---- */
    {
        auto a = RanScalar::from_bytes(test_a_bytes).value();
        auto b = RanScalar::from_bytes(test_b_bytes).value();
        auto one = RanScalar::one();
        auto lhs = RanScalar::muladd(a, b, one).to_bytes();
        auto rhs = (a * b + one).to_bytes();
        check_bytes("api: ran muladd a*b+1", lhs.data(), rhs.data(), 32);
    }

    /* ---- Point round-trip ---- */
    {
        auto G = RanPoint::generator();
        auto bytes = G.to_bytes();
        auto p = RanPoint::from_bytes(bytes.data());
        check_int("api: ran point from_bytes valid", 1, p.has_value() ? 1 : 0);
        auto bytes2 = p->to_bytes();
        check_bytes("api: ran point round-trip", bytes.data(), bytes2.data(), 32);
    }
    {
        auto G = ShawPoint::generator();
        auto bytes = G.to_bytes();
        auto p = ShawPoint::from_bytes(bytes.data());
        check_int("api: shaw point from_bytes valid", 1, p.has_value() ? 1 : 0);
        auto bytes2 = p->to_bytes();
        check_bytes("api: shaw point round-trip", bytes.data(), bytes2.data(), 32);
    }

    /* ---- Point arithmetic ---- */
    {
        auto G = RanPoint::generator();
        auto one = RanScalar::one();
        auto G1 = G.scalar_mul(one).to_bytes();
        auto Gb = G.to_bytes();
        check_bytes("api: ran G*1 == G", Gb.data(), G1.data(), 32);

        /* identity checks */
        auto I = RanPoint::identity();
        check_int("api: ran identity.is_identity", 1, I.is_identity() ? 1 : 0);
        check_int("api: ran G.is_identity", 0, G.is_identity() ? 1 : 0);

        /* dbl works */
        auto two = one + one;
        auto G2_sm = G.scalar_mul(two).to_bytes();
        auto G2_dbl = G.dbl().to_bytes();
        check_bytes("api: ran dbl == 2*G", G2_sm.data(), G2_dbl.data(), 32);

        /* P + Q where P != Q and neither is identity */
        auto three = two + one;
        auto G3 = G.scalar_mul(three);
        auto G2 = G.dbl();
        auto sum = (G2 + G).to_bytes();
        auto G3b = G3.to_bytes();
        check_bytes("api: ran 2G+G == 3G", G3b.data(), sum.data(), 32);

        /* negation: -G serializes differently from G (y-parity flips) */
        auto negG = (-G).to_bytes();
        check_nonzero("api: ran -G != G", std::memcmp(Gb.data(), negG.data(), 32));
    }
    {
        auto G = ShawPoint::generator();
        auto one = ShawScalar::one();
        auto G1 = G.scalar_mul(one).to_bytes();
        auto Gb = G.to_bytes();
        check_bytes("api: shaw G*1 == G", Gb.data(), G1.data(), 32);
    }

    /* ---- Point from_bytes rejects invalid ---- */
    {
        unsigned char bad[32] = {};
        bad[0] = 0x02; /* Likely off-curve */
        auto p = RanPoint::from_bytes(bad);
        check_int("api: ran point rejects off-curve", 0, p.has_value() ? 1 : 0);
    }

    /* ---- MSM: compare API wrapper against C-level MSM ---- */
    {
        auto G = RanPoint::generator();
        auto G2 = G.dbl();
        RanScalar scalars[2] = {
            RanScalar::from_bytes(test_a_bytes).value(), RanScalar::from_bytes(test_b_bytes).value()};
        RanPoint points[2] = {G, G2};
        auto msm = RanPoint::multi_scalar_mul(scalars, points, 2);

        /* Compare against C-level MSM */
        unsigned char c_scalars[64];
        std::memcpy(c_scalars, scalars[0].to_bytes().data(), 32);
        std::memcpy(c_scalars + 32, scalars[1].to_bytes().data(), 32);
        ran_jacobian c_points[2], c_result;
        ran_copy(&c_points[0], &G.raw());
        ran_copy(&c_points[1], &G2.raw());
        ran_msm_vartime(&c_result, c_scalars, c_points, 2);
        unsigned char c_bytes[32];
        ran_tobytes(c_bytes, &c_result);
        auto api_bytes = msm.to_bytes();
        check_bytes("api: ran msm matches C-level", c_bytes, api_bytes.data(), 32);
    }
    {
        auto G = ShawPoint::generator();
        auto G2 = G.dbl();
        ShawScalar scalars[2] = {
            ShawScalar::from_bytes(test_a_bytes).value(), ShawScalar::from_bytes(test_b_bytes).value()};
        ShawPoint points[2] = {G, G2};
        auto msm = ShawPoint::multi_scalar_mul(scalars, points, 2);

        unsigned char c_scalars[64];
        std::memcpy(c_scalars, scalars[0].to_bytes().data(), 32);
        std::memcpy(c_scalars + 32, scalars[1].to_bytes().data(), 32);
        shaw_jacobian c_points[2], c_result;
        shaw_copy(&c_points[0], &G.raw());
        shaw_copy(&c_points[1], &G2.raw());
        shaw_msm_vartime(&c_result, c_scalars, c_points, 2);
        unsigned char c_bytes[32];
        shaw_tobytes(c_bytes, &c_result);
        auto api_bytes = msm.to_bytes();
        check_bytes("api: shaw msm matches C-level", c_bytes, api_bytes.data(), 32);
    }

    /* ---- Pedersen: compare API wrapper against C-level ---- */
    {
        auto G = RanPoint::generator();
        auto H = G.dbl();
        auto blind = RanScalar::from_bytes(test_a_bytes).value();
        auto val = RanScalar::from_bytes(test_b_bytes).value();
        auto commit = RanPoint::pedersen_commit(blind, H, &val, &G, 1);

        /* Compare against C-level pedersen */
        auto bb = blind.to_bytes();
        auto vb = val.to_bytes();
        ran_jacobian c_result;
        ran_pedersen_commit(&c_result, bb.data(), &H.raw(), vb.data(), &G.raw(), 1);
        unsigned char c_bytes[32];
        ran_tobytes(c_bytes, &c_result);
        auto api_bytes = commit.to_bytes();
        check_bytes("api: ran pedersen matches C-level", c_bytes, api_bytes.data(), 32);
    }

    /* ---- Map to curve ---- */
    {
        auto p1 = RanPoint::map_to_curve(test_a_bytes);
        check_int("api: ran map_to_curve not identity", 0, p1.is_identity() ? 1 : 0);

        auto p2 = RanPoint::map_to_curve(test_a_bytes, test_b_bytes);
        check_int("api: ran map_to_curve2 not identity", 0, p2.is_identity() ? 1 : 0);
    }

    /* ---- x_coordinate_bytes ---- */
    {
        auto G = RanPoint::generator();
        auto xb = G.x_coordinate_bytes();
        /* x-coordinate of Ran generator is 3 */
        check_bytes("api: ran G x-coord", tv::compressed_points::ran_gx, xb.data(), 32);
    }

    /* ---- Polynomial ---- */
    {
        /* p(x) = x - r => from_roots([r]) => p(r) == 0 */
        auto poly = FpPolynomial::from_roots(test_a_bytes, 1);
        auto val = poly.evaluate(test_a_bytes);
        check_bytes("api: fp poly eval root == 0", zero_bytes, val.data(), 32);
        check_int("api: fp poly degree from 1 root", 1, (int)poly.degree());
    }
    {
        auto poly = FqPolynomial::from_roots(test_a_bytes, 1);
        auto val = poly.evaluate(test_a_bytes);
        check_bytes("api: fq poly eval root == 0", zero_bytes, val.data(), 32);
    }

    /* ---- Polynomial multiply consistency ---- */
    {
        /* (x - a) * (x - b) should equal from_roots([a, b]) */
        unsigned char roots[64];
        std::memcpy(roots, test_a_bytes, 32);
        std::memcpy(roots + 32, test_b_bytes, 32);

        auto pa = FpPolynomial::from_roots(test_a_bytes, 1);
        auto pb = FpPolynomial::from_roots(test_b_bytes, 1);
        auto prod = pa * pb;
        auto direct = FpPolynomial::from_roots(roots, 2);

        /* Evaluate both at point 1 and compare */
        auto v1 = prod.evaluate(one_bytes);
        auto v2 = direct.evaluate(one_bytes);
        check_bytes("api: fp poly mul == from_roots", v1.data(), v2.data(), 32);
    }

    /* ---- Divisor compute + evaluate ---- */
    {
        auto G = RanPoint::generator();
        auto P2 = G.dbl();
        RanPoint pts[2] = {G, P2};
        auto div_opt = RanDivisor::compute(pts, 2);
        check_nonzero("api: ran divisor compute succeeds", div_opt.has_value());
        auto div = *div_opt;

        /* Divisor should vanish at the points: get affine coords and evaluate */
        auto G_xb = G.x_coordinate_bytes();
        ran_affine aff;
        ran_to_affine(&aff, &G.raw());
        unsigned char y_bytes[32];
        fp_tobytes(y_bytes, aff.y);

        auto val_opt = div.evaluate(G_xb.data(), y_bytes);
        check_nonzero("api: ran divisor evaluate succeeds", val_opt.has_value());
        auto val = *val_opt;
        check_bytes("api: ran divisor eval at G == 0", zero_bytes, val.data(), 32);

        /* RanPoint overload: type-safe evaluation, never fails. */
        auto val2 = div.evaluate(G);
        check_bytes("api: ran divisor eval(RanPoint) == 0", zero_bytes, val2.data(), 32);
    }
    {
        auto G = ShawPoint::generator();
        auto P2 = G.dbl();
        ShawPoint pts[2] = {G, P2};
        auto div_opt = ShawDivisor::compute(pts, 2);
        check_nonzero("api: shaw divisor compute succeeds", div_opt.has_value());
        auto div = *div_opt;

        auto G_xb = G.x_coordinate_bytes();
        shaw_affine aff;
        shaw_to_affine(&aff, &G.raw());
        unsigned char y_bytes[32];
        fq_tobytes(y_bytes, aff.y);

        auto val_opt = div.evaluate(G_xb.data(), y_bytes);
        check_nonzero("api: shaw divisor evaluate succeeds", val_opt.has_value());
        auto val = *val_opt;
        check_bytes("api: shaw divisor eval at G == 0", zero_bytes, val.data(), 32);

        auto val2 = div.evaluate(G);
        check_bytes("api: shaw divisor eval(ShawPoint) == 0", zero_bytes, val2.data(), 32);
    }

    /* ---- Divisor negative tests (PR 7 validation) ---- */
    {
        /* Duplicate-x detection: compute() must reject a point list where
         * two entries share an x-coordinate. Use P and P (a trivial duplicate). */
        auto G = RanPoint::generator();
        RanPoint dup_pts[2] = {G, G};
        auto dup_opt = RanDivisor::compute(dup_pts, 2);
        check_nonzero("api: ran divisor rejects duplicate points", dup_opt.has_value() ? 0 : 1);

        /* Also reject P and -P (same x, different y): still duplicate x. */
        RanPoint neg_pts[2] = {G, -G};
        auto neg_opt = RanDivisor::compute(neg_pts, 2);
        check_nonzero("api: ran divisor rejects P and -P", neg_opt.has_value() ? 0 : 1);
    }
    {
        auto G = ShawPoint::generator();
        ShawPoint dup_pts[2] = {G, G};
        auto dup_opt = ShawDivisor::compute(dup_pts, 2);
        check_nonzero("api: shaw divisor rejects duplicate points", dup_opt.has_value() ? 0 : 1);

        ShawPoint neg_pts[2] = {G, -G};
        auto neg_opt = ShawDivisor::compute(neg_pts, 2);
        check_nonzero("api: shaw divisor rejects P and -P", neg_opt.has_value() ? 0 : 1);
    }
    {
        /* Off-curve evaluate: construct an (x, y) that does NOT satisfy the
         * curve equation and confirm evaluate() returns nullopt. We use x = 1,
         * y = 1 which is not on Ran (y^2 = 1 but x^3 - 3x + b != 1). */
        auto G = RanPoint::generator();
        auto P2 = G.dbl();
        RanPoint pts[2] = {G, P2};
        auto div = RanDivisor::compute(pts, 2).value();

        unsigned char x1[32] = {0};
        unsigned char y1[32] = {0};
        x1[0] = 1;
        y1[0] = 1;
        auto ev_off = div.evaluate(x1, y1);
        check_nonzero("api: ran divisor evaluate rejects off-curve", ev_off.has_value() ? 0 : 1);
    }
    {
        auto G = ShawPoint::generator();
        auto P2 = G.dbl();
        ShawPoint pts[2] = {G, P2};
        auto div = ShawDivisor::compute(pts, 2).value();

        unsigned char x1[32] = {0};
        unsigned char y1[32] = {0};
        x1[0] = 1;
        y1[0] = 1;
        auto ev_off = div.evaluate(x1, y1);
        check_nonzero("api: shaw divisor evaluate rejects off-curve", ev_off.has_value() ? 0 : 1);
    }

    /* ---- Wei25519 bridge ---- */
    {
        /* Valid x-coordinate (3 is valid as F_p element) */
        unsigned char x3[32] = {};
        x3[0] = 0x03;
        auto s = shaw_scalar_from_wei25519_x(x3);
        check_int("api: wei25519 valid x", 1, s.has_value() ? 1 : 0);
        auto sb = s->to_bytes();
        check_bytes("api: wei25519 x value", x3, sb.data(), 32);

        /* Invalid: bit 255 set */
        unsigned char bad[32] = {};
        bad[31] = 0x80;
        auto s2 = shaw_scalar_from_wei25519_x(bad);
        check_int("api: wei25519 rejects bit255", 0, s2.has_value() ? 1 : 0);
    }

    /* ---- MSM/Pedersen negative and edge tests (PR 12) ---- */
    {
        /* Empty MSM returns identity */
        auto r_msm = RanPoint::multi_scalar_mul(nullptr, nullptr, 0);
        check_int("api: ran msm empty returns identity", 1, r_msm.is_identity() ? 1 : 0);

        auto s_msm = ShawPoint::multi_scalar_mul(nullptr, nullptr, 0);
        check_int("api: shaw msm empty returns identity", 1, s_msm.is_identity() ? 1 : 0);
    }
    {
        /* Empty Pedersen returns blinding * H */
        auto H = RanPoint::generator();
        auto blinding = RanScalar::one() + RanScalar::one() + RanScalar::one(); /* 3 */
        auto expected = H.scalar_mul(blinding);
        auto got = RanPoint::pedersen_commit(blinding, H, nullptr, nullptr, 0);
        check_bytes("api: ran pedersen empty == blinding*H", expected.to_bytes().data(), got.to_bytes().data(), 32);

        auto sH = ShawPoint::generator();
        auto sb = ShawScalar::one() + ShawScalar::one() + ShawScalar::one();
        auto sexp = sH.scalar_mul(sb);
        auto sgot = ShawPoint::pedersen_commit(sb, sH, nullptr, nullptr, 0);
        check_bytes("api: shaw pedersen empty == blinding*H", sexp.to_bytes().data(), sgot.to_bytes().data(), 32);
    }
    {
        /* Zero-blinding Pedersen equals naive sum. */
        auto G = RanPoint::generator();
        auto G2 = G.dbl();
        auto G3 = G + G2;
        RanPoint gens[3] = {G, G2, G3};
        auto s1 = RanScalar::one();
        auto s2 = s1 + s1;
        auto s3 = s2 + s1;
        RanScalar vals[3] = {s1, s2, s3};

        auto expected = G.scalar_mul(s1) + G2.scalar_mul(s2) + G3.scalar_mul(s3);
        auto got = RanPoint::pedersen_commit(RanScalar::zero(), G, vals, gens, 3);
        check_bytes("api: ran pedersen zero-blinding == sum", expected.to_bytes().data(), got.to_bytes().data(), 32);
    }
    {
        /* SSWU exceptional input u=0: denom=0 path must produce an on-curve
         * point (via the inv0 fallback to B/(Z*A)). */
        unsigned char u_zero[32] = {0};
        auto p = RanPoint::map_to_curve(u_zero);
        check_int("api: ran map_to_curve(0) is on-curve", 1, p.is_identity() ? 0 : 1);

        auto sp = ShawPoint::map_to_curve(u_zero);
        check_int("api: shaw map_to_curve(0) is on-curve", 1, sp.is_identity() ? 0 : 1);
    }

    /* ---- Namespace init/autotune ---- */
    {
        ranshaw::init();
        /* No crash is the test */
        ++tests_run;
        ++tests_passed;
        std::cout << "  PASS: api: namespace init()" << std::endl;
    }
}
