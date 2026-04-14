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

#if defined(__AVX512F__) && defined(__AVX512IFMA__) && !defined(_MSC_VER)
#include "fq_cmov.h"
#include "fq_frombytes.h"
#include "fq_mul.h"
#include "fq_ops.h"
#include "fq_sq.h"
#include "fq_tobytes.h"
#include "ranshaw_cpuid.h"
#include "shaw.h"
#include "shaw_dbl.h"
#include "shaw_madd.h"
#include "x64/fq51.h"
#include "x64/ifma/fq51x2_ifma.h"
#include "x64/ifma/shaw_point_ops_ifma.h"

#include <cstdint>
#include <cstring>
#endif

namespace
{

#if defined(__AVX512F__) && defined(__AVX512IFMA__) && !defined(_MSC_VER)

    // Simple xorshift64 PRNG for reproducible random inputs — separate state
    // per test so order doesn't matter.
    struct xs64
    {
        uint64_t s;
    };
    static uint64_t xs_next(xs64 &r)
    {
        uint64_t x = r.s;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        r.s = x ? x : 0x9E3779B97F4A7C15ULL;
        return r.s;
    }

    static void rand_bytes(xs64 &r, unsigned char *buf, size_t n)
    {
        for (size_t i = 0; i < n; i++)
            buf[i] = (unsigned char)xs_next(r);
    }

    static void rand_fq(xs64 &r, fq_fe out)
    {
        unsigned char buf[32];
        rand_bytes(r, buf, 32);
        buf[31] &= 0x7F; // clear top bit, frombytes normalizes anyway
        fq_frombytes(out, buf);
    }

    static bool fq_equal_canonical(const fq_fe a, const fq_fe b)
    {
        unsigned char ba[32], bb[32];
        fq_tobytes(ba, a);
        fq_tobytes(bb, b);
        return std::memcmp(ba, bb, 32) == 0;
    }

    static void check_zero_pad(const char *name, const fq51x2_t v)
    {
        alignas(64) uint64_t buf[8];
        for (int i = 0; i < 5; i++)
        {
            _mm512_store_si512((__m512i *)buf, v[i]);
            for (int lane = 2; lane < 8; lane++)
            {
                if (buf[lane] != 0)
                {
                    check_true((std::string(name) + " zero-pad lane " + std::to_string(lane)).c_str(), false);
                    return;
                }
            }
        }
        check_true((std::string(name) + " zero-pad").c_str(), true);
    }

    static constexpr int N = 1000;

    static void test_mul(xs64 &r)
    {
        bool all_ok = true;
        for (int i = 0; i < N; i++)
        {
            fq_fe a, b, c, d;
            rand_fq(r, a);
            rand_fq(r, b);
            rand_fq(r, c);
            rand_fq(r, d);

            fq51x2_t pa, pb, pr;
            fq51x2_from_pair(pa, a, c);
            fq51x2_from_pair(pb, b, d);
            fq51x2_mul(pr, pa, pb);

            fq_fe lane0, lane1;
            fq51x2_to_pair(lane0, lane1, pr);

            fq_fe ref_ac, ref_bd;
            fq_mul(ref_ac, a, b);
            fq_mul(ref_bd, c, d);

            if (!fq_equal_canonical(lane0, ref_ac) || !fq_equal_canonical(lane1, ref_bd))
            {
                all_ok = false;
                break;
            }
        }
        check_true("fq51x2_mul matches scalar fq_mul (1000 random pairs, 2 lanes)", all_ok);
    }

    static void test_sq(xs64 &r)
    {
        bool all_ok = true;
        bool matches_mul = true;
        for (int i = 0; i < N; i++)
        {
            fq_fe a, b;
            rand_fq(r, a);
            rand_fq(r, b);

            fq51x2_t pa, psq, pmul;
            fq51x2_from_pair(pa, a, b);
            fq51x2_sq(psq, pa);
            fq51x2_mul(pmul, pa, pa);

            fq_fe s0, s1, m0, m1;
            fq51x2_to_pair(s0, s1, psq);
            fq51x2_to_pair(m0, m1, pmul);

            fq_fe ref_a, ref_b;
            fq_sq(ref_a, a);
            fq_sq(ref_b, b);

            if (!fq_equal_canonical(s0, ref_a) || !fq_equal_canonical(s1, ref_b))
                all_ok = false;
            if (!fq_equal_canonical(s0, m0) || !fq_equal_canonical(s1, m1))
                matches_mul = false;
        }
        check_true("fq51x2_sq matches scalar fq_sq", all_ok);
        check_true("fq51x2_sq matches fq51x2_mul(a,a)", matches_mul);
    }

    static void test_add(xs64 &r)
    {
        bool all_ok = true;
        for (int i = 0; i < N; i++)
        {
            fq_fe a, b, c, d;
            rand_fq(r, a);
            rand_fq(r, b);
            rand_fq(r, c);
            rand_fq(r, d);

            fq51x2_t pa, pb, pr;
            fq51x2_from_pair(pa, a, c);
            fq51x2_from_pair(pb, b, d);
            fq51x2_add(pr, pa, pb);

            // After lazy add, multiply result by 1 to canonicalise then compare.
            fq_fe one;
            fq_1(one);
            fq51x2_t pone;
            fq51x2_broadcast(pone, one);
            fq51x2_t pn;
            fq51x2_mul(pn, pr, pone);

            fq_fe lane0, lane1;
            fq51x2_to_pair(lane0, lane1, pn);

            fq_fe ref_ab, ref_cd, refs0, refs1;
            fq_add(ref_ab, a, b);
            fq_add(ref_cd, c, d);
            fq_mul(refs0, ref_ab, one);
            fq_mul(refs1, ref_cd, one);

            if (!fq_equal_canonical(lane0, refs0) || !fq_equal_canonical(lane1, refs1))
            {
                all_ok = false;
                break;
            }
        }
        check_true("fq51x2_add matches scalar fq_add (post-canonicalised)", all_ok);
    }

    static void test_sub(xs64 &r)
    {
        bool all_ok = true;
        for (int i = 0; i < N; i++)
        {
            fq_fe a, b, c, d;
            rand_fq(r, a);
            rand_fq(r, b);
            rand_fq(r, c);
            rand_fq(r, d);

            fq51x2_t pa, pb, pr;
            fq51x2_from_pair(pa, a, c);
            fq51x2_from_pair(pb, b, d);
            fq51x2_sub(pr, pa, pb);

            fq_fe lane0, lane1;
            fq51x2_to_pair(lane0, lane1, pr);

            fq_fe ref_ab, ref_cd;
            fq_sub(ref_ab, a, b);
            fq_sub(ref_cd, c, d);

            if (!fq_equal_canonical(lane0, ref_ab) || !fq_equal_canonical(lane1, ref_cd))
            {
                all_ok = false;
                break;
            }
        }
        check_true("fq51x2_sub matches scalar fq_sub", all_ok);
    }

    static void test_neg(xs64 &r)
    {
        bool all_ok = true;
        for (int i = 0; i < N; i++)
        {
            fq_fe a, b;
            rand_fq(r, a);
            rand_fq(r, b);

            fq51x2_t pa, pr;
            fq51x2_from_pair(pa, a, b);
            fq51x2_neg(pr, pa);

            fq_fe lane0, lane1;
            fq51x2_to_pair(lane0, lane1, pr);

            fq_fe ref_a, ref_b;
            fq_neg(ref_a, a);
            fq_neg(ref_b, b);

            if (!fq_equal_canonical(lane0, ref_a) || !fq_equal_canonical(lane1, ref_b))
            {
                all_ok = false;
                break;
            }
        }
        check_true("fq51x2_neg matches scalar fq_neg", all_ok);
    }

    static void test_cmov(xs64 &r)
    {
        bool ok0 = true, ok1 = true;
        for (int i = 0; i < N; i++)
        {
            fq_fe a, b, c, d;
            rand_fq(r, a);
            rand_fq(r, b);
            rand_fq(r, c);
            rand_fq(r, d);

            fq51x2_t pr, ps;
            fq51x2_from_pair(pr, a, c);
            fq51x2_from_pair(ps, b, d);

            fq51x2_t copy0;
            fq51x2_copy(copy0, pr);
            fq51x2_cmov(copy0, ps, 0);
            fq_fe l0a, l0b;
            fq51x2_to_pair(l0a, l0b, copy0);
            if (!fq_equal_canonical(l0a, a) || !fq_equal_canonical(l0b, c))
                ok0 = false;

            fq51x2_t copy1;
            fq51x2_copy(copy1, pr);
            fq51x2_cmov(copy1, ps, 1);
            fq_fe l1a, l1b;
            fq51x2_to_pair(l1a, l1b, copy1);
            if (!fq_equal_canonical(l1a, b) || !fq_equal_canonical(l1b, d))
                ok1 = false;
        }
        check_true("fq51x2_cmov b=0 preserves r", ok0);
        check_true("fq51x2_cmov b=1 selects s", ok1);
    }

    static void test_pack_roundtrip(xs64 &r)
    {
        bool all_ok = true;
        for (int i = 0; i < N; i++)
        {
            fq_fe a, b;
            rand_fq(r, a);
            rand_fq(r, b);

            fq51x2_t p;
            fq51x2_from_pair(p, a, b);
            fq_fe oa, ob;
            fq51x2_to_pair(oa, ob, p);

            if (!fq_equal_canonical(oa, a) || !fq_equal_canonical(ob, b))
            {
                all_ok = false;
                break;
            }
        }
        check_true("fq51x2 pack/unpack roundtrip", all_ok);
    }

    static void test_zero_pad(xs64 &r)
    {
        fq_fe a, b, c, d;
        rand_fq(r, a);
        rand_fq(r, b);
        rand_fq(r, c);
        rand_fq(r, d);
        fq51x2_t pa, pb, pr;
        fq51x2_from_pair(pa, a, c);
        fq51x2_from_pair(pb, b, d);

        fq51x2_mul(pr, pa, pb);
        check_zero_pad("mul", pr);
        fq51x2_sq(pr, pa);
        check_zero_pad("sq", pr);
        fq51x2_add(pr, pa, pb);
        check_zero_pad("add", pr);
        fq51x2_sub(pr, pa, pb);
        check_zero_pad("sub", pr);
        fq51x2_neg(pr, pa);
        check_zero_pad("neg", pr);
        fq51x2_cmov(pr, pb, 1);
        check_zero_pad("cmov", pr);
        fq51x2_scale_small(pr, pa, 3);
        check_zero_pad("scale_small", pr);
    }

    static void test_scale_small(xs64 &r)
    {
        bool all_ok = true;
        const unsigned int ks[] = {2, 3, 4, 8};
        for (unsigned int k : ks)
        {
            for (int i = 0; i < 200; i++)
            {
                fq_fe a, b;
                rand_fq(r, a);
                rand_fq(r, b);

                fq51x2_t pa, pr;
                fq51x2_from_pair(pa, a, b);
                fq51x2_scale_small(pr, pa, k);

                // Compare against chained scalar adds. With the native 4x64 fq_fe,
                // fq51x2_to_pair canonicalizes its output while chained fq64_add
                // leaves a < 2^256 (non-canonical) representative, so raw limb
                // equality no longer holds across representations; compare the
                // canonical value (mod q) instead.
                fq_fe lane0, lane1;
                fq51x2_to_pair(lane0, lane1, pr);

                fq_fe ka;
                fq_copy(ka, a);
                for (unsigned int t = 1; t < k; t++)
                    fq_add(ka, ka, a);
                fq_fe kb;
                fq_copy(kb, b);
                for (unsigned int t = 1; t < k; t++)
                    fq_add(kb, kb, b);

                if (!fq_equal_canonical(lane0, ka) || !fq_equal_canonical(lane1, kb))
                {
                    all_ok = false;
                    break;
                }
            }
            if (!all_ok)
                break;
        }
        check_true("fq51x2_scale_small (k in {2,3,4,8}) matches chained scalar adds", all_ok);
    }

    static bool jacobian_equal_canonical(const shaw_jacobian *a, const shaw_jacobian *b)
    {
        // Jacobian points are projectively equal if X_a*Z_b² == X_b*Z_a² and
        // Y_a*Z_b³ == Y_b*Z_a³. Check via cross-multiplication.
        fq_fe Za2, Zb2, Za3, Zb3, lx, rx, ly, ry;
        fq_sq(Za2, a->Z);
        fq_sq(Zb2, b->Z);
        fq_mul(Za3, Za2, a->Z);
        fq_mul(Zb3, Zb2, b->Z);
        fq_mul(lx, a->X, Zb2);
        fq_mul(rx, b->X, Za2);
        fq_mul(ly, a->Y, Zb3);
        fq_mul(ry, b->Y, Za3);
        return fq_equal_canonical(lx, rx) && fq_equal_canonical(ly, ry);
    }

    static void test_point_ops_ifma()
    {
        // Seed with the curve generator G, then repeatedly double/mixed-add to
        // generate a sequence of Jacobian points. Compare each step's ifma vs
        // x64 output.
        shaw_jacobian p;
        fq_copy(p.X, SHAW_GX);
        fq_copy(p.Y, SHAW_GY);
        fq_1(p.Z);

        shaw_affine qa;
        fq_copy(qa.x, SHAW_GX);
        fq_copy(qa.y, SHAW_GY);

        bool dbl_ok = true;
        bool madd_ok = true;
        for (int i = 0; i < 1000; i++)
        {
            shaw_jacobian r_ref, r_ifma;
            shaw_dbl_x64(&r_ref, &p);
            shaw_dbl_ifma(&r_ifma, &p);
            if (!jacobian_equal_canonical(&r_ref, &r_ifma))
            {
                dbl_ok = false;
                break;
            }

            shaw_jacobian m_ref, m_ifma;
            shaw_madd_x64(&m_ref, &r_ref, &qa);
            shaw_madd_ifma(&m_ifma, &r_ref, &qa);
            if (!jacobian_equal_canonical(&m_ref, &m_ifma))
            {
                madd_ok = false;
                break;
            }

            // Advance p for the next iteration.
            p = m_ref;
        }
        check_true("shaw_dbl_ifma matches shaw_dbl_x64 (1000 iters)", dbl_ok);
        check_true("shaw_madd_ifma matches shaw_madd_x64 (1000 iters)", madd_ok);
    }

#endif // IFMA gates

} // namespace

// AVX-512 worker body. Called only from test_fq51x2_ifma() in field_fq.cpp,
// and only after a baseline-ISA ranshaw_has_avx512ifma() guard has passed. The
// guard cannot live in this TU: it is compiled with -mavx512f/-mavx512ifma, so
// a guard here would emit AVX-512 in its own prologue and fault on a baseline
// CPU before the check ran. See the entry in field_fq.cpp for the full note.
void ranshaw_run_fq51x2_ifma_tests()
{
#if defined(__AVX512F__) && defined(__AVX512IFMA__) && !defined(_MSC_VER)
    xs64 rng {0xDEADBEEFCAFEBABEULL};
    test_mul(rng);
    test_sq(rng);
    test_add(rng);
    test_sub(rng);
    test_neg(rng);
    test_cmov(rng);
    test_pack_roundtrip(rng);
    test_zero_pad(rng);
    test_scale_small(rng);
    test_point_ops_ifma();
#endif
}
