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

#include "ran_complete_add.h"
#include "ran_dbl.h"
#include "ran_msm_ct.h"
#include "ran_msm_vartime.h"
#include "ran_projective.h"
#include "ranshaw_cpuid.h"
#include "shaw_complete_add.h"
#include "shaw_dbl.h"
#include "shaw_msm_ct.h"
#include "shaw_msm_vartime.h"
#include "shaw_projective.h"
#include "tests/common.h"
#include "tests/registry.h"

#include <vector>

namespace
{

    /* Deterministic 64-bit LCG for reproducible fuzz. */
    inline uint64_t lcg_next(uint64_t *state)
    {
        *state = (*state) * 6364136223846793005ULL + 1442695040888963407ULL;
        return *state;
    }

    inline void lcg_fill(unsigned char *out, size_t n, uint64_t *state)
    {
        for (size_t i = 0; i < n; i++)
        {
            out[i] = (unsigned char)(lcg_next(state) >> 56);
        }
    }

    void run_ran_complete_add()
    {
        std::cout << std::endl << "=== Ran complete-add (RCB 2016 Alg 4) ===" << std::endl;

        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        ran_projective G_proj;
        ran_jac_to_proj(&G_proj, &G);

        /* 1. identity + G == G */
        {
            ran_projective id_proj, sum_proj;
            ran_proj_identity(&id_proj);
            ran_complete_add(&sum_proj, &id_proj, &G_proj);
            ran_jacobian sum_jac;
            ran_proj_to_jac(&sum_jac, &sum_proj);
            unsigned char buf[32];
            ran_tobytes(buf, &sum_jac);
            check_bytes("identity + G == G", tv::compressed_points::ran_g, buf, 32);
        }

        /* 2. G + identity == G */
        {
            ran_projective id_proj, sum_proj;
            ran_proj_identity(&id_proj);
            ran_complete_add(&sum_proj, &G_proj, &id_proj);
            ran_jacobian sum_jac;
            ran_proj_to_jac(&sum_jac, &sum_proj);
            unsigned char buf[32];
            ran_tobytes(buf, &sum_jac);
            check_bytes("G + identity == G", tv::compressed_points::ran_g, buf, 32);
        }

        /* 3. G + (-G) == identity */
        {
            ran_jacobian negG_jac;
            ran_neg(&negG_jac, &G);
            ran_projective negG_proj, sum_proj;
            ran_jac_to_proj(&negG_proj, &negG_jac);
            ran_complete_add(&sum_proj, &G_proj, &negG_proj);
            check_nonzero("G + (-G) == identity", ran_proj_is_identity(&sum_proj));
        }

        /* 4. G + G == 2G */
        {
            ran_jacobian two_g_jac;
            ran_dbl(&two_g_jac, &G);
            ran_projective sum_proj;
            ran_complete_add(&sum_proj, &G_proj, &G_proj);
            ran_jacobian sum_jac;
            ran_proj_to_jac(&sum_jac, &sum_proj);
            unsigned char ref_bytes[32], ct_bytes[32];
            ran_tobytes(ref_bytes, &two_g_jac);
            ran_tobytes(ct_bytes, &sum_jac);
            check_bytes("G + G == 2G", ref_bytes, ct_bytes, 32);
        }

        /* 5. 10k random fuzz: CT complete-add == incomplete Jacobian add (after
         *    normalization via ran_tobytes). Uses ran_scalarmult_vartime to build
         *    random Jacobian points from deterministic 32-byte scalars. */
        {
            uint64_t rng = 0xDEADBEEFCAFE1234ULL;
            int mismatches = 0;
            const int iters = 10000;
            for (int i = 0; i < iters; i++)
            {
                unsigned char sa[32], sb[32];
                lcg_fill(sa, 32, &rng);
                lcg_fill(sb, 32, &rng);
                /* Mask top bit to avoid the tiny-probability rejection window on
                 * scalars >= group order; ran_scalarmult_vartime handles unreduced
                 * scalars but normalization churns test time. */
                sa[31] &= 0x7F;
                sb[31] &= 0x7F;

                ran_jacobian P, Q;
                ran_scalarmult_vartime(&P, sa, &G);
                ran_scalarmult_vartime(&Q, sb, &G);

                ran_jacobian ref_sum;
                ran_add(&ref_sum, &P, &Q);

                ran_projective P_proj, Q_proj, ct_sum_proj;
                ran_jac_to_proj(&P_proj, &P);
                ran_jac_to_proj(&Q_proj, &Q);
                ran_complete_add(&ct_sum_proj, &P_proj, &Q_proj);
                ran_jacobian ct_sum;
                ran_proj_to_jac(&ct_sum, &ct_sum_proj);

                /* Both sums normalize to the same affine point if RCB is correct.
                 * ran_tobytes goes through ran_to_affine (fp_invert on Z), so the
                 * test captures affine equivalence, not raw Jacobian coordinates. */
                bool ref_id = ran_is_identity(&ref_sum);
                bool ct_id = ran_is_identity(&ct_sum);
                if (ref_id != ct_id)
                {
                    mismatches++;
                    continue;
                }
                if (ref_id)
                {
                    continue; /* both identity, nothing more to compare */
                }

                unsigned char ref_bytes[32], ct_bytes[32];
                ran_tobytes(ref_bytes, &ref_sum);
                ran_tobytes(ct_bytes, &ct_sum);
                if (std::memcmp(ref_bytes, ct_bytes, 32) != 0)
                {
                    mismatches++;
                }
            }
            check_int("10k random CT complete-add == ran_add", 0, mismatches);
        }
    }

    void run_shaw_complete_add()
    {
        std::cout << std::endl << "=== Shaw complete-add (RCB 2016 Alg 4) ===" << std::endl;

        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        shaw_projective G_proj;
        shaw_jac_to_proj(&G_proj, &G);

        /* 1. identity + G == G */
        {
            shaw_projective id_proj, sum_proj;
            shaw_proj_identity(&id_proj);
            shaw_complete_add(&sum_proj, &id_proj, &G_proj);
            shaw_jacobian sum_jac;
            shaw_proj_to_jac(&sum_jac, &sum_proj);
            unsigned char buf[32];
            shaw_tobytes(buf, &sum_jac);
            check_bytes("identity + G == G", tv::compressed_points::shaw_g, buf, 32);
        }

        /* 2. G + identity == G */
        {
            shaw_projective id_proj, sum_proj;
            shaw_proj_identity(&id_proj);
            shaw_complete_add(&sum_proj, &G_proj, &id_proj);
            shaw_jacobian sum_jac;
            shaw_proj_to_jac(&sum_jac, &sum_proj);
            unsigned char buf[32];
            shaw_tobytes(buf, &sum_jac);
            check_bytes("G + identity == G", tv::compressed_points::shaw_g, buf, 32);
        }

        /* 3. G + (-G) == identity */
        {
            shaw_jacobian negG_jac;
            shaw_neg(&negG_jac, &G);
            shaw_projective negG_proj, sum_proj;
            shaw_jac_to_proj(&negG_proj, &negG_jac);
            shaw_complete_add(&sum_proj, &G_proj, &negG_proj);
            check_nonzero("G + (-G) == identity", shaw_proj_is_identity(&sum_proj));
        }

        /* 4. G + G == 2G */
        {
            shaw_jacobian two_g_jac;
            shaw_dbl(&two_g_jac, &G);
            shaw_projective sum_proj;
            shaw_complete_add(&sum_proj, &G_proj, &G_proj);
            shaw_jacobian sum_jac;
            shaw_proj_to_jac(&sum_jac, &sum_proj);
            unsigned char ref_bytes[32], ct_bytes[32];
            shaw_tobytes(ref_bytes, &two_g_jac);
            shaw_tobytes(ct_bytes, &sum_jac);
            check_bytes("G + G == 2G", ref_bytes, ct_bytes, 32);
        }

        /* 5. 10k random fuzz: CT complete-add == incomplete Jacobian add. */
        {
            uint64_t rng = 0xCAFEBABEFEEDFACEULL;
            int mismatches = 0;
            const int iters = 10000;
            for (int i = 0; i < iters; i++)
            {
                unsigned char sa[32], sb[32];
                lcg_fill(sa, 32, &rng);
                lcg_fill(sb, 32, &rng);
                sa[31] &= 0x7F;
                sb[31] &= 0x7F;

                shaw_jacobian P, Q;
                shaw_scalarmult_vartime(&P, sa, &G);
                shaw_scalarmult_vartime(&Q, sb, &G);

                shaw_jacobian ref_sum;
                shaw_add(&ref_sum, &P, &Q);

                shaw_projective P_proj, Q_proj, ct_sum_proj;
                shaw_jac_to_proj(&P_proj, &P);
                shaw_jac_to_proj(&Q_proj, &Q);
                shaw_complete_add(&ct_sum_proj, &P_proj, &Q_proj);
                shaw_jacobian ct_sum;
                shaw_proj_to_jac(&ct_sum, &ct_sum_proj);

                bool ref_id = shaw_is_identity(&ref_sum);
                bool ct_id = shaw_is_identity(&ct_sum);
                if (ref_id != ct_id)
                {
                    mismatches++;
                    continue;
                }
                if (ref_id)
                {
                    continue;
                }

                unsigned char ref_bytes[32], ct_bytes[32];
                shaw_tobytes(ref_bytes, &ref_sum);
                shaw_tobytes(ct_bytes, &ct_sum);
                if (std::memcmp(ref_bytes, ct_bytes, 32) != 0)
                {
                    mismatches++;
                }
            }
            check_int("10k random CT complete-add == shaw_add", 0, mismatches);
        }
    }

    void run_ran_msm_ct_vs_vt()
    {
        std::cout << std::endl << "=== Ran CT MSM vs vartime MSM (byte-equal) ===" << std::endl;

        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        uint64_t rng = 0x0C0FFEE1DEADBEEFULL;
        int mismatches = 0;
        int cases = 0;

        /* n = 0 edge case. */
        {
            ran_jacobian ct_r, vt_r;
            ran_msm_ct(&ct_r, nullptr, nullptr, 0);
            ran_msm_vartime(&vt_r, nullptr, nullptr, 0);
            if (!(ran_is_identity(&ct_r) && ran_is_identity(&vt_r)))
            {
                mismatches++;
            }
            cases++;
        }

        /* Random: n in {1..32}, 512 cases with varied scalar/point shapes. */
        for (int trial = 0; trial < 512; trial++)
        {
            size_t n = 1 + (size_t)(lcg_next(&rng) % 32);
            std::vector<unsigned char> scalars(32 * n);
            std::vector<ran_jacobian> points(n);
            for (size_t i = 0; i < n; i++)
            {
                unsigned char point_seed[32];
                lcg_fill(point_seed, 32, &rng);
                point_seed[31] &= 0x7F;
                ran_scalarmult_vartime(&points[i], point_seed, &G);

                lcg_fill(&scalars[32 * i], 32, &rng);
                scalars[32 * i + 31] &= 0x7F;
            }

            ran_jacobian ct_r, vt_r;
            ran_msm_ct(&ct_r, scalars.data(), points.data(), n);
            ran_msm_vartime(&vt_r, scalars.data(), points.data(), n);

            bool ct_id = ran_is_identity(&ct_r);
            bool vt_id = ran_is_identity(&vt_r);
            if (ct_id != vt_id)
            {
                mismatches++;
                cases++;
                continue;
            }
            if (ct_id)
            {
                cases++;
                continue;
            }

            unsigned char ct_bytes[32], vt_bytes[32];
            ran_tobytes(ct_bytes, &ct_r);
            ran_tobytes(vt_bytes, &vt_r);
            if (std::memcmp(ct_bytes, vt_bytes, 32) != 0)
            {
                mismatches++;
            }
            cases++;
        }

        check_int("Ran msm_ct == msm_vartime across 513 cases", 0, mismatches);
        (void)cases;
    }

    void run_shaw_msm_ct_vs_vt()
    {
        std::cout << std::endl << "=== Shaw CT MSM vs vartime MSM (byte-equal) ===" << std::endl;

        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        uint64_t rng = 0xA110CA7EDB00BEEFULL;
        int mismatches = 0;

        {
            shaw_jacobian ct_r, vt_r;
            shaw_msm_ct(&ct_r, nullptr, nullptr, 0);
            shaw_msm_vartime(&vt_r, nullptr, nullptr, 0);
            if (!(shaw_is_identity(&ct_r) && shaw_is_identity(&vt_r)))
            {
                mismatches++;
            }
        }

        for (int trial = 0; trial < 512; trial++)
        {
            size_t n = 1 + (size_t)(lcg_next(&rng) % 32);
            std::vector<unsigned char> scalars(32 * n);
            std::vector<shaw_jacobian> points(n);
            for (size_t i = 0; i < n; i++)
            {
                unsigned char point_seed[32];
                lcg_fill(point_seed, 32, &rng);
                point_seed[31] &= 0x7F;
                shaw_scalarmult_vartime(&points[i], point_seed, &G);

                lcg_fill(&scalars[32 * i], 32, &rng);
                scalars[32 * i + 31] &= 0x7F;
            }

            shaw_jacobian ct_r, vt_r;
            shaw_msm_ct(&ct_r, scalars.data(), points.data(), n);
            shaw_msm_vartime(&vt_r, scalars.data(), points.data(), n);

            bool ct_id = shaw_is_identity(&ct_r);
            bool vt_id = shaw_is_identity(&vt_r);
            if (ct_id != vt_id)
            {
                mismatches++;
                continue;
            }
            if (ct_id)
            {
                continue;
            }

            unsigned char ct_bytes[32], vt_bytes[32];
            shaw_tobytes(ct_bytes, &ct_r);
            shaw_tobytes(vt_bytes, &vt_r);
            if (std::memcmp(ct_bytes, vt_bytes, 32) != 0)
            {
                mismatches++;
            }
        }

        check_int("Shaw msm_ct == msm_vartime across 513 cases", 0, mismatches);
    }

} // namespace

// AVX-512 IFMA worker, defined in complete_add_ifma.cpp (a separate TU built
// with -mavx512f/-mavx512ifma). A no-op when the IFMA backend is not compiled
// in. The runtime guard below MUST live here in the baseline-ISA TU: a guard
// compiled into the AVX-512 TU emits AVX-512 in its own prologue (clang+ASan
// zeroes the enlarged stack frame with zmm stores) that faults with SIGILL on a
// baseline CPU before the check runs. Calling across the TU boundary is safe —
// only the callee's body uses AVX-512, and only after the check passes.
extern void ranshaw_run_complete_add_ifma_impl();

// Runtime-gated entry, called unconditionally from test_complete_add(). Self-
// skips on hosts without AVX-512 IFMA so an IFMA-enabled binary still runs
// correctly on baseline CPUs. RANSHAW_NO_AVX512_FN is the second safeguard
// layer (this TU is already baseline ISA).
RANSHAW_NO_AVX512_FN void run_complete_add_ifma_suite()
{
#if RANSHAW_SIMD && !defined(RANSHAW_NO_AVX512)
    if (!ranshaw_has_avx512ifma())
    {
        std::cout << std::endl
                  << "=== Ran/Shaw IFMA complete-add + CT MSM ===" << std::endl
                  << "Host CPU lacks AVX512IFMA — skipping IFMA complete-add tests." << std::endl;
        return;
    }
    ranshaw_run_complete_add_ifma_impl();
#endif
}

void test_complete_add()
{
    run_ran_complete_add();
    run_shaw_complete_add();
    run_ran_msm_ct_vs_vt();
    run_shaw_msm_ct_vs_vt();
    run_complete_add_ifma_suite();
}
