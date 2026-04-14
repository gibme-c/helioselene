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

#include "fuzz_tests/common.h"
#include "fuzz_tests/registry.h"

/*
 * Differential fuzz: fq_is_qr must agree with fq_sqrt's Legendre post-check
 * for every random Fq element. fq_sqrt is the authoritative oracle because
 * it computes z^((q+1)/4) and verifies acc^2 == z — if acc^2 != z, z is NR.
 *
 * 1000 iterations of random 32-byte canonical Fq input per run. Any divergence
 * between the two is a correctness failure in the Jacobi-divsteps tracking.
 */
void fuzz_is_qr()
{
    std::cout << std::endl << "=== Fuzz: fq_is_qr ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 22);

    /* Zero input (QR by convention) */
    {
        fq_fe zero;
        fq_0(zero);
        int iq = fq_is_qr(zero);
        check_true("is_qr(0) == 1", iq == 1);
    }

    /* 1000 random canonical Fq elements */
    for (int i = 0; i < 1000; i++)
    {
        std::string label = "is_qr_diff[" + std::to_string(i) + "]";

        uint8_t bytes[32];
        rng.fill_bytes(bytes, 32);
        bytes[31] &= 0x7F; /* keep canonical in [0, 2^255); frombytes folds further */

        fq_fe z;
        fq_frombytes(z, bytes);

        int iq = fq_is_qr(z);

        fq_fe tmp;
        int rc = fq_sqrt(tmp, z);
        int iq_ref = (rc == 0) ? 1 : 0;

        check_true(label.c_str(), iq == iq_ref);
    }

    /* Structural cases: for any nonzero a, (a*a) is always QR and the product
     * of a NR and (a*a) is a NR. Use -1 (NR when q ≡ 3 mod 4) as the witness. */
    {
        fq_fe neg_one;
        {
            fq_fe one;
            fq_1(one);
            fq_fe zero_fe;
            fq_0(zero_fe);
            fq_sub(neg_one, zero_fe, one);
        }
        check_true("is_qr(-1) == 0", fq_is_qr(neg_one) == 0);

        for (int i = 0; i < 100; i++)
        {
            std::string label = "is_qr_struct[" + std::to_string(i) + "]";

            uint8_t b[32];
            rng.fill_bytes(b, 32);
            b[31] &= 0x7F;
            fq_fe a;
            fq_frombytes(a, b);

            fq_fe a_sq, neg_a_sq;
            fq_sq(a_sq, a);
            fq_mul(neg_a_sq, a_sq, neg_one);

            /* If a is nonzero then a^2 is a nonzero QR and -a^2 is NR. If a is
             * zero, both are zero (QR by convention). Either way is_qr(a^2)=1
             * and is_qr(-a^2) in {0, 1} depending on whether a^2 == 0. */
            int zero_a = 1;
            uint8_t ab[32];
            fq_tobytes(ab, a_sq);
            for (int j = 0; j < 32; j++)
                if (ab[j])
                {
                    zero_a = 0;
                    break;
                }

            check_true((label + "_sq").c_str(), fq_is_qr(a_sq) == 1);
            int expected_neg = zero_a ? 1 : 0;
            check_true((label + "_neg_sq").c_str(), fq_is_qr(neg_a_sq) == expected_neg);
        }
    }
}
