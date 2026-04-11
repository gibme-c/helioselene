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

void fuzz_batch_affine()
{
    std::cout << std::endl << "=== Fuzz: Batch Affine ===" << std::endl;
    xoshiro256ss rng;
    rng.seed(global_seed + 13);

    const int sizes[] = {1, 2, 4, 8, 16, 32};

    /* Ran */
    for (int si = 0; si < 6; si++)
    {
        size_t n = (size_t)sizes[si];
        for (int trial = 0; trial < 8; trial++)
        {
            std::string label = "ran_batch_aff[n=" + std::to_string(n) + ",t=" + std::to_string(trial) + "]";

            std::vector<ran_jacobian> jac(n);
            for (size_t j = 0; j < n; j++)
            {
                auto P = random_ran_point(rng);
                ran_copy(&jac[j], &P.raw());
            }

            std::vector<ran_affine> batch(n);
            ran_batch_to_affine(batch.data(), jac.data(), n);

            bool all_ok = true;
            for (size_t j = 0; j < n; j++)
            {
                ran_affine single;
                if (ran_is_identity(&jac[j]))
                {
                    fp_0(single.x);
                    fp_0(single.y);
                }
                else
                {
                    ran_to_affine(&single, &jac[j]);
                }

                unsigned char batch_x[32], single_x[32], batch_y[32], single_y[32];
                fp_tobytes(batch_x, batch[j].x);
                fp_tobytes(single_x, single.x);
                fp_tobytes(batch_y, batch[j].y);
                fp_tobytes(single_y, single.y);

                if (std::memcmp(batch_x, single_x, 32) != 0 || std::memcmp(batch_y, single_y, 32) != 0)
                    all_ok = false;
            }
            check_true(label.c_str(), all_ok);
        }
    }

    /* Shaw */
    for (int si = 0; si < 6; si++)
    {
        size_t n = (size_t)sizes[si];
        for (int trial = 0; trial < 8; trial++)
        {
            std::string label = "shaw_batch_aff[n=" + std::to_string(n) + ",t=" + std::to_string(trial) + "]";

            std::vector<shaw_jacobian> jac(n);
            for (size_t j = 0; j < n; j++)
            {
                auto P = random_shaw_point(rng);
                shaw_copy(&jac[j], &P.raw());
            }

            std::vector<shaw_affine> batch(n);
            shaw_batch_to_affine(batch.data(), jac.data(), n);

            bool all_ok = true;
            for (size_t j = 0; j < n; j++)
            {
                shaw_affine single;
                if (shaw_is_identity(&jac[j]))
                {
                    fq_0(single.x);
                    fq_0(single.y);
                }
                else
                {
                    shaw_to_affine(&single, &jac[j]);
                }

                unsigned char batch_x[32], single_x[32], batch_y[32], single_y[32];
                fq_tobytes(batch_x, batch[j].x);
                fq_tobytes(single_x, single.x);
                fq_tobytes(batch_y, batch[j].y);
                fq_tobytes(single_y, single.y);

                if (std::memcmp(batch_x, single_x, 32) != 0 || std::memcmp(batch_y, single_y, 32) != 0)
                    all_ok = false;
            }
            check_true(label.c_str(), all_ok);
        }
    }
}

/* ======================================================================
 * 14. fuzz_polynomial — ~1,500 checks
 * ====================================================================== */
