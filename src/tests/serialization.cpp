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

void test_serialization_edges()
{
    std::cout << std::endl << "=== Serialization edges ===" << std::endl;
    unsigned char buf[32];

    /* Fp: round-trip 0, 1, p-1 */
    {
        fp_fe fe;
        fp_0(fe);
        fp_tobytes(buf, fe);
        fp_fe fe2;
        fp_frombytes(fe2, buf);
        unsigned char buf2[32];
        fp_tobytes(buf2, fe2);
        check_bytes("fp round-trip 0", buf, buf2, 32);
    }
    {
        fp_fe fe;
        fp_1(fe);
        fp_tobytes(buf, fe);
        fp_fe fe2;
        fp_frombytes(fe2, buf);
        unsigned char buf2[32];
        fp_tobytes(buf2, fe2);
        check_bytes("fp round-trip 1", buf, buf2, 32);
    }
    {
        unsigned char pm1_bytes[32] = {0xec, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                       0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
        fp_fe fe;
        fp_frombytes(fe, pm1_bytes);
        fp_tobytes(buf, fe);
        check_bytes("fp round-trip p-1", pm1_bytes, buf, 32);
    }

    /* Fq: round-trip 0, 1, q-1 */
    {
        fq_fe fe;
        fq_0(fe);
        fq_tobytes(buf, fe);
        fq_fe fe2;
        fq_frombytes(fe2, buf);
        unsigned char buf2[32];
        fq_tobytes(buf2, fe2);
        check_bytes("fq round-trip 0", buf, buf2, 32);
    }
    {
        fq_fe fe;
        fq_1(fe);
        fq_tobytes(buf, fe);
        fq_fe fe2;
        fq_frombytes(fe2, buf);
        unsigned char buf2[32];
        fq_tobytes(buf2, fe2);
        check_bytes("fq round-trip 1", buf, buf2, 32);
    }
    {
        unsigned char qm1_bytes[32];
        std::memcpy(qm1_bytes, RAN_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (qm1_bytes[i] > 0)
            {
                qm1_bytes[i]--;
                break;
            }
            qm1_bytes[i] = 0xff;
        }
        fq_fe fe;
        fq_frombytes(fe, qm1_bytes);
        fq_tobytes(buf, fe);
        check_bytes("fq round-trip q-1", qm1_bytes, buf, 32);
    }

    /* Fp: value with high bits near 255 */
    {
        unsigned char high_bytes[32] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40};
        fp_fe fe;
        fp_frombytes(fe, high_bytes);
        fp_tobytes(buf, fe);
        check_bytes("fp round-trip high bit value", high_bytes, buf, 32);
    }
}


void test_serialization_roundtrip()
{
    using namespace ranshaw;

    std::cout << std::endl << "=== Serialization round-trip ===" << std::endl;

    /* Helper: round-trip a point through to_bytes/from_bytes */
    auto ran_point_rt = [](const char *label, const RanPoint &p)
    {
        auto bytes = p.to_bytes();
        auto p2 = RanPoint::from_bytes(bytes.data());
        check_int(label, 1, p2.has_value() ? 1 : 0);
        if (p2)
        {
            auto bytes2 = p2->to_bytes();
            check_bytes(label, bytes.data(), bytes2.data(), 32);
        }
    };
    auto shaw_point_rt = [](const char *label, const ShawPoint &p)
    {
        auto bytes = p.to_bytes();
        auto p2 = ShawPoint::from_bytes(bytes.data());
        check_int(label, 1, p2.has_value() ? 1 : 0);
        if (p2)
        {
            auto bytes2 = p2->to_bytes();
            check_bytes(label, bytes.data(), bytes2.data(), 32);
        }
    };
    auto ran_scalar_rt = [](const char *label, const RanScalar &s)
    {
        auto bytes = s.to_bytes();
        auto s2 = RanScalar::from_bytes(bytes.data());
        check_int(label, 1, s2.has_value() ? 1 : 0);
        if (s2)
        {
            auto bytes2 = s2->to_bytes();
            check_bytes(label, bytes.data(), bytes2.data(), 32);
        }
    };
    auto shaw_scalar_rt = [](const char *label, const ShawScalar &s)
    {
        auto bytes = s.to_bytes();
        auto s2 = ShawScalar::from_bytes(bytes.data());
        check_int(label, 1, s2.has_value() ? 1 : 0);
        if (s2)
        {
            auto bytes2 = s2->to_bytes();
            check_bytes(label, bytes.data(), bytes2.data(), 32);
        }
    };

    /* ---- Ran point round-trips ---- */
    {
        auto G = RanPoint::generator();
        auto one = RanScalar::one();
        auto two = one + one;
        auto three = two + one;
        auto a = RanScalar::from_bytes(test_a_bytes).value();
        auto b = RanScalar::from_bytes(test_b_bytes).value();

        ran_point_rt("rt: ran G", G);

        /* Identity + P == P (operator+ must handle identity inputs) */
        {
            auto I = RanPoint::identity();
            auto sum = I + G;
            check_int("rt: ran identity+G not identity", 0, sum.is_identity() ? 1 : 0);
            auto Gb = G.to_bytes();
            auto sb = sum.to_bytes();
            check_bytes("rt: ran identity+G == G", Gb.data(), sb.data(), 32);

            auto sum2 = G + I;
            check_int("rt: ran G+identity not identity", 0, sum2.is_identity() ? 1 : 0);
            auto sb2 = sum2.to_bytes();
            check_bytes("rt: ran G+identity == G", Gb.data(), sb2.data(), 32);

            /* Accumulation pattern: identity + P1 + P2 */
            auto P2 = G.dbl();
            auto accum = I + G + P2;
            auto direct = G + P2;
            check_bytes("rt: ran accum I+G+2G", direct.to_bytes().data(), accum.to_bytes().data(), 32);
        }

        /* P + P == dbl(P) (operator+ must handle equal inputs) */
        {
            auto sum = G + G;
            auto dbl_G = G.dbl();
            check_bytes("rt: ran G+G == dbl(G)", dbl_G.to_bytes().data(), sum.to_bytes().data(), 32);

            /* Also with non-affine Z (computed point) */
            auto P = G.scalar_mul(a);
            auto sum2 = P + P;
            auto dbl_P = P.dbl();
            check_bytes("rt: ran P+P == dbl(P)", dbl_P.to_bytes().data(), sum2.to_bytes().data(), 32);
        }

        /* P + (-P) == identity */
        {
            auto negG = -G;
            auto sum = G + negG;
            check_int("rt: ran G+(-G) is identity", 1, sum.is_identity() ? 1 : 0);

            auto P = G.scalar_mul(a);
            auto negP = -P;
            auto sum2 = P + negP;
            check_int("rt: ran P+(-P) is identity", 1, sum2.is_identity() ? 1 : 0);
        }

        ran_point_rt("rt: ran 2G (dbl)", G.dbl());
        ran_point_rt("rt: ran 3G (add)", G.dbl() + G);
        ran_point_rt("rt: ran -G (neg)", -G);
        ran_point_rt("rt: ran G*1", G.scalar_mul(one));
        ran_point_rt("rt: ran G*2", G.scalar_mul(two));
        ran_point_rt("rt: ran G*3", G.scalar_mul(three));
        ran_point_rt("rt: ran G*a", G.scalar_mul(a));
        ran_point_rt("rt: ran G*b", G.scalar_mul(b));
        ran_point_rt("rt: ran G*a + G*b", G.scalar_mul(a) + G.scalar_mul(b));
        ran_point_rt("rt: ran map_to_curve(a)", RanPoint::map_to_curve(test_a_bytes));
        ran_point_rt("rt: ran map_to_curve(a,b)", RanPoint::map_to_curve(test_a_bytes, test_b_bytes));

        /* Iterated doubling: 2^k * G for k=1..10 */
        auto P = G;
        for (int k = 1; k <= 10; k++)
        {
            P = P.dbl();
            std::string name = "rt: ran 2^" + std::to_string(k) + "*G";
            ran_point_rt(name.c_str(), P);
        }
    }

    /* ---- Shaw point round-trips ---- */
    {
        auto G = ShawPoint::generator();
        auto one = ShawScalar::one();
        auto two = one + one;
        auto three = two + one;
        auto a = ShawScalar::from_bytes(test_a_bytes).value();
        auto b = ShawScalar::from_bytes(test_b_bytes).value();

        shaw_point_rt("rt: shaw G", G);

        /* Identity + P == P */
        {
            auto I = ShawPoint::identity();
            auto sum = I + G;
            check_int("rt: shaw identity+G not identity", 0, sum.is_identity() ? 1 : 0);
            auto Gb = G.to_bytes();
            auto sb = sum.to_bytes();
            check_bytes("rt: shaw identity+G == G", Gb.data(), sb.data(), 32);

            auto sum2 = G + I;
            check_int("rt: shaw G+identity not identity", 0, sum2.is_identity() ? 1 : 0);
            auto sb2 = sum2.to_bytes();
            check_bytes("rt: shaw G+identity == G", Gb.data(), sb2.data(), 32);

            auto P2 = G.dbl();
            auto accum = I + G + P2;
            auto direct = G + P2;
            check_bytes("rt: shaw accum I+G+2G", direct.to_bytes().data(), accum.to_bytes().data(), 32);
        }

        /* P + P == dbl(P) */
        {
            auto sum = G + G;
            auto dbl_G = G.dbl();
            check_bytes("rt: shaw G+G == dbl(G)", dbl_G.to_bytes().data(), sum.to_bytes().data(), 32);

            auto P = G.scalar_mul(a);
            auto sum2 = P + P;
            auto dbl_P = P.dbl();
            check_bytes("rt: shaw P+P == dbl(P)", dbl_P.to_bytes().data(), sum2.to_bytes().data(), 32);
        }

        /* P + (-P) == identity */
        {
            auto negG = -G;
            auto sum = G + negG;
            check_int("rt: shaw G+(-G) is identity", 1, sum.is_identity() ? 1 : 0);

            auto P = G.scalar_mul(a);
            auto negP = -P;
            auto sum2 = P + negP;
            check_int("rt: shaw P+(-P) is identity", 1, sum2.is_identity() ? 1 : 0);
        }

        shaw_point_rt("rt: shaw 2G (dbl)", G.dbl());
        shaw_point_rt("rt: shaw 3G (add)", G.dbl() + G);
        shaw_point_rt("rt: shaw -G (neg)", -G);
        shaw_point_rt("rt: shaw G*1", G.scalar_mul(one));
        shaw_point_rt("rt: shaw G*2", G.scalar_mul(two));
        shaw_point_rt("rt: shaw G*3", G.scalar_mul(three));
        shaw_point_rt("rt: shaw G*a", G.scalar_mul(a));
        shaw_point_rt("rt: shaw G*b", G.scalar_mul(b));
        shaw_point_rt("rt: shaw G*a + G*b", G.scalar_mul(a) + G.scalar_mul(b));
        shaw_point_rt("rt: shaw map_to_curve(a)", ShawPoint::map_to_curve(test_a_bytes));
        shaw_point_rt("rt: shaw map_to_curve(a,b)", ShawPoint::map_to_curve(test_a_bytes, test_b_bytes));

        auto P = G;
        for (int k = 1; k <= 10; k++)
        {
            P = P.dbl();
            std::string name = "rt: shaw 2^" + std::to_string(k) + "*G";
            shaw_point_rt(name.c_str(), P);
        }
    }

    /* ---- MSM vs scalar_mul+add consistency ---- */
    {
        auto G = RanPoint::generator();
        auto a = RanScalar::from_bytes(test_a_bytes).value();
        auto b = RanScalar::from_bytes(test_b_bytes).value();

        /* n=2: MSM(a,b; G,2G) == a*G + b*2G */
        {
            auto G2 = G.dbl();
            RanScalar s[2] = {a, b};
            RanPoint p[2] = {G, G2};
            auto msm = RanPoint::multi_scalar_mul(s, p, 2);
            auto manual = G.scalar_mul(a) + G2.scalar_mul(b);
            check_int("rt: ran msm n=2 not identity", 0, msm.is_identity() ? 1 : 0);
            check_bytes("rt: ran msm n=2 == manual", manual.to_bytes().data(), msm.to_bytes().data(), 32);
        }

        /* n=2 with map_to_curve points (non-trivial Z after scalarmul) */
        {
            auto P0 = RanPoint::map_to_curve(test_a_bytes);
            auto P1 = RanPoint::map_to_curve(test_b_bytes);
            RanScalar s[2] = {a, b};
            RanPoint p[2] = {P0, P1};
            auto msm = RanPoint::multi_scalar_mul(s, p, 2);
            auto manual = P0.scalar_mul(a) + P1.scalar_mul(b);
            check_int("rt: ran msm n=2 h2c not identity", 0, msm.is_identity() ? 1 : 0);
            check_bytes("rt: ran msm n=2 h2c == manual", manual.to_bytes().data(), msm.to_bytes().data(), 32);
        }

        /* n=1: MSM(a; G) == a*G */
        {
            RanScalar s[1] = {a};
            RanPoint p[1] = {G};
            auto msm = RanPoint::multi_scalar_mul(s, p, 1);
            auto manual = G.scalar_mul(a);
            check_bytes("rt: ran msm n=1 == manual", manual.to_bytes().data(), msm.to_bytes().data(), 32);
        }

        /* n=3 */
        {
            auto G2 = G.dbl();
            auto G3 = G2 + G;
            auto one = RanScalar::one();
            RanScalar s[3] = {a, b, one};
            RanPoint p[3] = {G, G2, G3};
            auto msm = RanPoint::multi_scalar_mul(s, p, 3);
            auto manual = G.scalar_mul(a) + G2.scalar_mul(b) + G3.scalar_mul(one);
            check_bytes("rt: ran msm n=3 == manual", manual.to_bytes().data(), msm.to_bytes().data(), 32);
        }
    }
    {
        auto G = ShawPoint::generator();
        auto a = ShawScalar::from_bytes(test_a_bytes).value();
        auto b = ShawScalar::from_bytes(test_b_bytes).value();

        {
            auto G2 = G.dbl();
            ShawScalar s[2] = {a, b};
            ShawPoint p[2] = {G, G2};
            auto msm = ShawPoint::multi_scalar_mul(s, p, 2);
            auto manual = G.scalar_mul(a) + G2.scalar_mul(b);
            check_int("rt: shaw msm n=2 not identity", 0, msm.is_identity() ? 1 : 0);
            check_bytes("rt: shaw msm n=2 == manual", manual.to_bytes().data(), msm.to_bytes().data(), 32);
        }

        {
            auto P0 = ShawPoint::map_to_curve(test_a_bytes);
            auto P1 = ShawPoint::map_to_curve(test_b_bytes);
            ShawScalar s[2] = {a, b};
            ShawPoint p[2] = {P0, P1};
            auto msm = ShawPoint::multi_scalar_mul(s, p, 2);
            auto manual = P0.scalar_mul(a) + P1.scalar_mul(b);
            check_int("rt: shaw msm n=2 h2c not identity", 0, msm.is_identity() ? 1 : 0);
            check_bytes("rt: shaw msm n=2 h2c == manual", manual.to_bytes().data(), msm.to_bytes().data(), 32);
        }

        {
            ShawScalar s[1] = {a};
            ShawPoint p[1] = {G};
            auto msm = ShawPoint::multi_scalar_mul(s, p, 1);
            auto manual = G.scalar_mul(a);
            check_bytes("rt: shaw msm n=1 == manual", manual.to_bytes().data(), msm.to_bytes().data(), 32);
        }

        {
            auto G2 = G.dbl();
            auto G3 = G2 + G;
            auto one = ShawScalar::one();
            ShawScalar s[3] = {a, b, one};
            ShawPoint p[3] = {G, G2, G3};
            auto msm = ShawPoint::multi_scalar_mul(s, p, 3);
            auto manual = G.scalar_mul(a) + G2.scalar_mul(b) + G3.scalar_mul(one);
            check_bytes("rt: shaw msm n=3 == manual", manual.to_bytes().data(), msm.to_bytes().data(), 32);
        }
    }

    /* ---- Ran scalar round-trips ---- */
    {
        auto a = RanScalar::from_bytes(test_a_bytes).value();
        auto b = RanScalar::from_bytes(test_b_bytes).value();
        auto one = RanScalar::one();

        ran_scalar_rt("rt: ran scalar zero", RanScalar::zero());
        ran_scalar_rt("rt: ran scalar one", one);
        ran_scalar_rt("rt: ran scalar a", a);
        ran_scalar_rt("rt: ran scalar a+b", a + b);
        ran_scalar_rt("rt: ran scalar a*b", a * b);
        ran_scalar_rt("rt: ran scalar a-b", a - b);
        ran_scalar_rt("rt: ran scalar -a", -a);
        ran_scalar_rt("rt: ran scalar a^2", a.sq());
        ran_scalar_rt("rt: ran scalar inv(a)", a.invert().value());
    }

    /* ---- Shaw scalar round-trips ---- */
    {
        auto a = ShawScalar::from_bytes(test_a_bytes).value();
        auto b = ShawScalar::from_bytes(test_b_bytes).value();
        auto one = ShawScalar::one();

        shaw_scalar_rt("rt: shaw scalar zero", ShawScalar::zero());
        shaw_scalar_rt("rt: shaw scalar one", one);
        shaw_scalar_rt("rt: shaw scalar a", a);
        shaw_scalar_rt("rt: shaw scalar a+b", a + b);
        shaw_scalar_rt("rt: shaw scalar a*b", a * b);
        shaw_scalar_rt("rt: shaw scalar a-b", a - b);
        shaw_scalar_rt("rt: shaw scalar -a", -a);
        shaw_scalar_rt("rt: shaw scalar a^2", a.sq());
        shaw_scalar_rt("rt: shaw scalar inv(a)", a.invert().value());
    }
}
