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

#include "ranshaw_cpuid.h"
#include "tests/common.h"
#include "tests/registry.h"

void test_fq()
{
    std::cout << std::endl << "=== F_q arithmetic ===" << std::endl;
    unsigned char buf[32];

    fq_fe a, b, c, d;
    fq_frombytes(a, test_a_bytes);
    fq_frombytes(b, test_b_bytes);

    fq_tobytes(buf, a);
    check_bytes("tobytes(frombytes(a)) == a", test_a_bytes, buf, 32);

    fq_fe zero;
    fq_0(zero);
    fq_tobytes(buf, zero);
    check_bytes("tobytes(0)", zero_bytes, buf, 32);

    fq_fe one;
    fq_1(one);
    fq_tobytes(buf, one);
    check_bytes("tobytes(1)", one_bytes, buf, 32);

    fq_add(c, a, zero);
    fq_tobytes(buf, c);
    check_bytes("a + 0 == a", test_a_bytes, buf, 32);

    fq_mul(c, a, b);
    fq_tobytes(buf, c);
    check_bytes("a * b", tv::fq_field::mul_ab, buf, 32);

    fq_mul(d, b, a);
    fq_tobytes(buf, d);
    check_bytes("b * a == a * b", tv::fq_field::mul_ab, buf, 32);

    fq_sq(c, a);
    fq_tobytes(buf, c);
    check_bytes("a^2", tv::fq_field::sq_a, buf, 32);

    fq_mul(d, a, a);
    fq_tobytes(buf, d);
    check_bytes("sq(a) == mul(a,a)", tv::fq_field::sq_a, buf, 32);

    fq_mul(c, a, one);
    fq_tobytes(buf, c);
    check_bytes("a * 1 == a", test_a_bytes, buf, 32);

    fq_fe inv_a;
    fq_invert(inv_a, a);
    fq_tobytes(buf, inv_a);
    check_bytes("inv(a)", tv::fq_field::inv_a, buf, 32);

    fq_mul(c, inv_a, a);
    fq_tobytes(buf, c);
    check_bytes("inv(a) * a == 1", one_bytes, buf, 32);

    /* Test invert with fully-populated input (exercises all limbs) */
    {
        fq_fe denom_fe, inv_denom, check_one;
        fq_frombytes(denom_fe, tv::fq_field::denom);
        fq_invert(inv_denom, denom_fe);
        fq_mul(check_one, inv_denom, denom_fe);
        fq_tobytes(buf, check_one);
        check_bytes("inv(full_denom) * full_denom == 1", one_bytes, buf, 32);
        /* Cross-check: known inverse should also give 1 when multiplied by denom */
        fq_fe x64_inv_fe;
        fq_frombytes(x64_inv_fe, tv::fq_field::denom_inv);
        fq_fe cross_check;
        fq_mul(cross_check, x64_inv_fe, denom_fe);
        fq_tobytes(buf, cross_check);
        check_bytes("x64_inv * denom == 1 (cross-check)", one_bytes, buf, 32);
#if !RANSHAW_PLATFORM_64BIT
        /* Verify GAMMA_25 matches GAMMA_51 via byte round-trip */
        {
            /* Construct gamma from GAMMA_25 limbs directly */
            fq_fe gamma_25_fe;
            gamma_25_fe[0] = GAMMA_25[0];
            gamma_25_fe[1] = GAMMA_25[1];
            gamma_25_fe[2] = GAMMA_25[2];
            gamma_25_fe[3] = GAMMA_25[3];
            gamma_25_fe[4] = GAMMA_25[4];
            gamma_25_fe[5] = 0;
            gamma_25_fe[6] = 0;
            gamma_25_fe[7] = 0;
            gamma_25_fe[8] = 0;
            gamma_25_fe[9] = 0;
            unsigned char gamma_25_bytes[32];
            fq_tobytes(gamma_25_bytes, gamma_25_fe);

            /* Construct gamma from GAMMA_51 via byte packing */
            uint64_t g51[5] = {0x7B9BA138F07A1ULL, 0x638D19E0B11D2ULL, 0x2D13853ULL, 0, 0};
            unsigned char gamma_51_bytes[32];
            /* Pack 51-bit limbs to bytes */
            uint64_t w0 = g51[0] | (g51[1] << 51);
            uint64_t w1 = (g51[1] >> 13) | (g51[2] << 38);
            uint64_t w2 = (g51[2] >> 26);
            uint64_t w3 = 0;
            for (int i = 0; i < 8; i++)
                gamma_51_bytes[i] = (unsigned char)(w0 >> (8 * i));
            for (int i = 0; i < 8; i++)
                gamma_51_bytes[8 + i] = (unsigned char)(w1 >> (8 * i));
            for (int i = 0; i < 8; i++)
                gamma_51_bytes[16 + i] = (unsigned char)(w2 >> (8 * i));
            for (int i = 0; i < 8; i++)
                gamma_51_bytes[24 + i] = (unsigned char)(w3 >> (8 * i));

            check_bytes("GAMMA_25 == GAMMA_51 (byte comparison)", gamma_51_bytes, gamma_25_bytes, 32);
        }
#endif // !RANSHAW_PLATFORM_64BIT
        /* Simple mul test: denom * 2 via add vs mul(denom, 2) */
        fq_fe two_fe;
        fq_1(two_fe);
        fq_add(two_fe, two_fe, two_fe);
        fq_fe denom_times_2_add, denom_times_2_mul;
        fq_add(denom_times_2_add, denom_fe, denom_fe);
        fq_mul(denom_times_2_mul, denom_fe, two_fe);
        unsigned char dadd[32], dmul[32];
        fq_tobytes(dadd, denom_times_2_add);
        fq_tobytes(dmul, denom_times_2_mul);
        check_bytes("denom*2 add vs mul", dadd, dmul, 32);
    }

    fq_sub(c, a, a);
    fq_tobytes(buf, c);
    check_bytes("a - a == 0", zero_bytes, buf, 32);

    fq_neg(d, a);
    fq_add(c, a, d);
    fq_tobytes(buf, c);
    check_bytes("a + (-a) == 0", zero_bytes, buf, 32);

    fq_fe four;
    fq_frombytes(four, four_bytes);
    fq_fe sqrt4;
    fq_sqrt(sqrt4, four);
    fq_tobytes(buf, sqrt4);
    check_bytes("sqrt(4) == 2", tv::fq_field::sqrt4, buf, 32);

    fq_sq(c, sqrt4);
    fq_tobytes(buf, c);
    check_bytes("sqrt(4)^2 == 4", four_bytes, buf, 32);
}


void test_fq_extended()
{
    std::cout << std::endl << "=== F_q extended ===" << std::endl;
    unsigned char buf[32];

    fq_fe a, one_fe, zero_fe;
    fq_frombytes(a, test_a_bytes);
    fq_1(one_fe);
    fq_0(zero_fe);

    /* 2*sq(a) via add == via mul */
    {
        fq_fe sq_a, sq2_via_add;
        fq_sq(sq_a, a);
        fq_add(sq2_via_add, sq_a, sq_a);
        unsigned char two_b[32] = {0x02};
        fq_fe two_fe, sq2_via_mul;
        fq_frombytes(two_fe, two_b);
        fq_mul(sq2_via_mul, sq_a, two_fe);
        unsigned char add_bytes[32], mul_bytes[32];
        fq_tobytes(add_bytes, sq2_via_add);
        fq_tobytes(mul_bytes, sq2_via_mul);
        check_bytes("2*sq(a) via add == via mul", mul_bytes, add_bytes, 32);
    }

    /* sq^5 chain equivalence */
    {
        fq_fe chain;
        fq_sq(chain, a);
        fq_sq(chain, chain);
        fq_sq(chain, chain);
        fq_sq(chain, chain);
        fq_sq(chain, chain);
        fq_fe power;
        fq_sq(power, a);
        fq_mul(power, power, power);
        fq_mul(power, power, power);
        fq_mul(power, power, power);
        fq_mul(power, power, power);
        unsigned char chain_bytes[32], power_bytes[32];
        fq_tobytes(chain_bytes, chain);
        fq_tobytes(power_bytes, power);
        check_bytes("sq^5(a) == a^32", power_bytes, chain_bytes, 32);
    }

    /* cmov */
    {
        fq_fe target;
        fq_copy(target, a);
        fq_cmov(target, one_fe, 0);
        fq_tobytes(buf, target);
        check_bytes("cmov(a, 1, 0) == a", test_a_bytes, buf, 32);
    }
    {
        fq_fe target;
        fq_copy(target, a);
        fq_cmov(target, one_fe, 1);
        fq_tobytes(buf, target);
        check_bytes("cmov(a, 1, 1) == 1", one_bytes, buf, 32);
    }

    /* fq_sqrt(0) == 0, returns 0 */
    {
        fq_fe result;
        int rc = fq_sqrt(result, zero_fe);
        check_int("sqrt(0) returns 0", 0, rc);
        fq_tobytes(buf, result);
        check_bytes("sqrt(0) == 0", zero_bytes, buf, 32);
    }

    /* fq_sqrt(1)^2 == 1, returns 0 */
    {
        fq_fe sqrt1, sq_check;
        int rc = fq_sqrt(sqrt1, one_fe);
        check_int("sqrt(1) returns 0", 0, rc);
        fq_sq(sq_check, sqrt1);
        fq_tobytes(buf, sq_check);
        check_bytes("sqrt(1)^2 == 1", one_bytes, buf, 32);
    }

    /* fq_sqrt(a^2)^2 == a^2, returns 0 */
    {
        fq_fe a_sq, sqrt_asq, sq_check;
        fq_sq(a_sq, a);
        int rc = fq_sqrt(sqrt_asq, a_sq);
        check_int("sqrt(a^2) returns 0", 0, rc);
        fq_sq(sq_check, sqrt_asq);
        unsigned char asq_bytes[32];
        fq_tobytes(asq_bytes, a_sq);
        fq_tobytes(buf, sq_check);
        check_bytes("sqrt(a^2)^2 == a^2", asq_bytes, buf, 32);
    }

    /* fq_sqrt on non-residue: returns -1, zeros out. For q ≡ 3 (mod 4),
     * −1 is a non-residue since (q−1)/2 is odd. Build q−1 = RAN_ORDER − 1. */
    {
        unsigned char neg_one_bytes[32];
        std::memcpy(neg_one_bytes, RAN_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (neg_one_bytes[i] > 0)
            {
                neg_one_bytes[i]--;
                break;
            }
            neg_one_bytes[i] = 0xff;
        }
        fq_fe neg_one, result;
        fq_frombytes(neg_one, neg_one_bytes);
        int rc = fq_sqrt(result, neg_one);
        check_int("sqrt(-1) returns -1 (non-residue)", -1, rc);
        fq_tobytes(buf, result);
        check_bytes("sqrt(-1) zeros out on non-residue", zero_bytes, buf, 32);
    }

    /* fq_sqrt: for any non-QR n, n * a^2 is also NR. Combine -1 with a^2
     * to build a different NR and re-verify. */
    {
        unsigned char neg_one_bytes[32];
        std::memcpy(neg_one_bytes, RAN_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (neg_one_bytes[i] > 0)
            {
                neg_one_bytes[i]--;
                break;
            }
            neg_one_bytes[i] = 0xff;
        }
        fq_fe neg_one, a_sq, nr, result;
        fq_frombytes(neg_one, neg_one_bytes);
        fq_sq(a_sq, a);
        fq_mul(nr, neg_one, a_sq);
        int rc = fq_sqrt(result, nr);
        check_int("sqrt(-a^2) returns -1 (non-residue)", -1, rc);
        fq_tobytes(buf, result);
        check_bytes("sqrt(-a^2) zeros out on non-residue", zero_bytes, buf, 32);
    }

    /* Edge: (q-1)*(q-1) == 1 */
    {
        /* q = 2^255 - gamma, q-1 in LE */
        /* RAN_ORDER is q in LE. q-1 = RAN_ORDER - 1 */
        unsigned char qm1_bytes[32];
        std::memcpy(qm1_bytes, RAN_ORDER, 32);
        /* Subtract 1 from little-endian */
        for (int i = 0; i < 32; i++)
        {
            if (qm1_bytes[i] > 0)
            {
                qm1_bytes[i]--;
                break;
            }
            qm1_bytes[i] = 0xff;
        }
        fq_fe qm1, qm1_sq;
        fq_frombytes(qm1, qm1_bytes);
        fq_mul(qm1_sq, qm1, qm1);
        fq_tobytes(buf, qm1_sq);
        check_bytes("(q-1)^2 == 1", one_bytes, buf, 32);
    }

    /* Edge: (q-1) + 1 wraps to 0 */
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
        fq_fe qm1, result;
        fq_frombytes(qm1, qm1_bytes);
        fq_add(result, qm1, one_fe);
        fq_tobytes(buf, result);
        check_bytes("(q-1) + 1 == 0", zero_bytes, buf, 32);
    }

    /* invert(1) == 1 */
    {
        fq_fe result;
        fq_invert(result, one_fe);
        fq_tobytes(buf, result);
        check_bytes("invert(1) == 1", one_bytes, buf, 32);
    }

    /* neg(0) == 0 */
    {
        fq_fe result;
        fq_neg(result, zero_fe);
        fq_tobytes(buf, result);
        check_bytes("neg(0) == 0", zero_bytes, buf, 32);
    }

    /* Serialization: frombytes(q_bytes) reduces to 0 */
    {
        fq_fe result;
        fq_frombytes(result, RAN_ORDER);
        fq_tobytes(buf, result);
        check_bytes("frombytes(q) == 0", zero_bytes, buf, 32);
    }

    /* Curve constant: SHAW_B3 == 3 * SHAW_B mod q (precomputed for RCB complete addition) */
    {
        fq_fe sum, diff;
        fq_add(sum, SHAW_B, SHAW_B);
        fq_add(sum, sum, SHAW_B);
        fq_sub(diff, sum, SHAW_B3);
        fq_tobytes(buf, diff);
        check_bytes("SHAW_B3 == 3 * SHAW_B mod q", zero_bytes, buf, 32);
    }
}


void test_fq_is_qr()
{
    std::cout << std::endl << "=== F_q is_qr (Legendre symbol) ===" << std::endl;

    fq_fe one_fe, zero_fe;
    fq_1(one_fe);
    fq_0(zero_fe);

    /* Edge cases */
    check_int("is_qr(0) == 1 (convention: sqrt(0)=0)", 1, fq_is_qr(zero_fe));
    check_int("is_qr(1) == 1", 1, fq_is_qr(one_fe));

    /* q - 1 is a non-residue for q ≡ 3 (mod 4) since (q-1)/2 is odd. */
    {
        unsigned char qm1[32];
        std::memcpy(qm1, RAN_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (qm1[i] > 0)
            {
                qm1[i]--;
                break;
            }
            qm1[i] = 0xff;
        }
        fq_fe neg_one;
        fq_frombytes(neg_one, qm1);
        check_int("is_qr(q-1) == 0 (-1 is NR when q ≡ 3 mod 4)", 0, fq_is_qr(neg_one));
    }

    /* For any a != 0, a^2 is a QR. For any NR n, n*a^2 is a NR. */
    {
        fq_fe a, a_sq, neg_a_sq;
        fq_frombytes(a, test_a_bytes);
        fq_sq(a_sq, a);
        check_int("is_qr(a^2) == 1", 1, fq_is_qr(a_sq));

        unsigned char qm1[32];
        std::memcpy(qm1, RAN_ORDER, 32);
        for (int i = 0; i < 32; i++)
        {
            if (qm1[i] > 0)
            {
                qm1[i]--;
                break;
            }
            qm1[i] = 0xff;
        }
        fq_fe neg_one;
        fq_frombytes(neg_one, qm1);
        fq_mul(neg_a_sq, a_sq, neg_one);
        check_int("is_qr(-a^2) == 0", 0, fq_is_qr(neg_a_sq));
    }

    /* Small-value scan 0..63: every small value's is_qr must agree with fq_sqrt. */
    {
        int small_diffs = 0;
        for (int v = 0; v < 64; v++)
        {
            unsigned char sb[32];
            std::memset(sb, 0, 32);
            sb[0] = (unsigned char)v;
            fq_fe z;
            fq_frombytes(z, sb);
            int iq = fq_is_qr(z);
            fq_fe tmp;
            int rc = fq_sqrt(tmp, z);
            int iq_ref = (rc == 0) ? 1 : 0;
            if (iq != iq_ref)
                small_diffs++;
        }
        check_int("is_qr small-value scan 0..63 mismatches", 0, small_diffs);
    }

    /* Differential test: for N random inputs, is_qr must match fq_sqrt's return. */
    {
        const int N = 256;
        int mismatches = 0;
        for (int k = 0; k < N; k++)
        {
            unsigned char seed[32];
            for (unsigned i = 0; i < 32; i++)
                seed[i] = (unsigned char)(((unsigned)k * 2654435761u + i * 97u) >> (i & 7u));
            seed[31] &= 0x7f; /* force into [0, 2^255) so frombytes is canonical */
            fq_fe z;
            fq_frombytes(z, seed);

            int iq = fq_is_qr(z);

            fq_fe sq_tmp;
            int rc = fq_sqrt(sq_tmp, z);
            int iq_ref = (rc == 0) ? 1 : 0;

            if (iq != iq_ref)
                mismatches++;
        }
        check_int("is_qr vs fq_sqrt: 0 mismatches across 256 random inputs", 0, mismatches);
    }
}

// Baseline-ISA entry for the AVX-512 IFMA fq51x2 tests. The worker
// (ranshaw_run_fq51x2_ifma_tests) lives in field_fq51x2_ifma.cpp, a TU compiled
// with -mavx512f/-mavx512ifma. The runtime guard MUST live here in a baseline
// TU: compiled into the AVX-512 TU, the guard's own prologue emits AVX-512
// (clang+ASan zeroes the enlarged stack frame with zmm stores) that faults with
// SIGILL on a baseline CPU before the check runs. Calling across the TU
// boundary is safe — only the callee's body uses AVX-512, and only after the
// check passes. RANSHAW_NO_AVX512_FN is the second safeguard layer.
RANSHAW_NO_AVX512_FN void test_fq51x2_ifma()
{
#if RANSHAW_SIMD && !defined(RANSHAW_NO_AVX512)
    extern void ranshaw_run_fq51x2_ifma_tests();

    std::cout << std::endl << "=== F_q 2-way IFMA primitives (fq51x2) ===" << std::endl;
    if (!ranshaw_has_avx512ifma())
    {
        std::cout << "Host CPU lacks AVX512IFMA — skipping fq51x2 tests." << std::endl;
        return;
    }
    ranshaw_run_fq51x2_ifma_tests();
#endif
}
