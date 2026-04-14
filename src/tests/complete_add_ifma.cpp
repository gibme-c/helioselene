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

// AVX-512 IFMA complete-add and CT-MSM cross-checks. This TU is compiled with
// -mavx512f/-mavx512ifma (see CMakeLists.txt), so it must contain ONLY code
// reached after a runtime ranshaw_has_avx512ifma() check; otherwise the
// compiler may emit AVX-512 instructions that fault on CPUs without the
// feature. The unconditional scalar complete-add tests live in
// complete_add.cpp, which is built for the baseline ISA.
#if defined(__AVX512F__) && defined(__AVX512IFMA__) && !defined(_MSC_VER)
#include "x64/ifma/ran_complete_add_ifma.h"
#include "x64/ifma/ran_msm_ct_ifma.h"
#include "x64/ifma/ran_projective_8x.h"
#include "x64/ifma/shaw_complete_add_ifma.h"
#include "x64/ifma/shaw_msm_ct_ifma.h"
#include "x64/ifma/shaw_projective_8x.h"

namespace
{

    /* Deterministic 64-bit LCG for reproducible fuzz (mirrors complete_add.cpp). */
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

    void run_ran_complete_add_ifma_vs_scalar()
    {
        std::cout << std::endl << "=== Ran complete_add IFMA 8x vs scalar (byte-equal) ===" << std::endl;

        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        /* Prepare 8 distinct scalar projective points for the left operand, and
         * 8 more for the right. Compare scalar ran_complete_add(p_k, q_k) to the
         * k-th lane of ran_complete_add_ifma_8x(packed_p, packed_q). */
        uint64_t rng = 0xC0DE5C0FFEEC0DE5ULL;

        ran_projective p_scalar[8], q_scalar[8];
        for (int k = 0; k < 8; k++)
        {
            unsigned char sa[32], sb[32];
            lcg_fill(sa, 32, &rng);
            lcg_fill(sb, 32, &rng);
            sa[31] &= 0x7F;
            sb[31] &= 0x7F;
            ran_jacobian P, Q;
            ran_scalarmult_vartime(&P, sa, &G);
            ran_scalarmult_vartime(&Q, sb, &G);
            ran_jac_to_proj(&p_scalar[k], &P);
            ran_jac_to_proj(&q_scalar[k], &Q);
        }

        ran_projective_8x packed_p, packed_q;
        ran_pack_proj_8x(
            &packed_p,
            &p_scalar[0],
            &p_scalar[1],
            &p_scalar[2],
            &p_scalar[3],
            &p_scalar[4],
            &p_scalar[5],
            &p_scalar[6],
            &p_scalar[7]);
        ran_pack_proj_8x(
            &packed_q,
            &q_scalar[0],
            &q_scalar[1],
            &q_scalar[2],
            &q_scalar[3],
            &q_scalar[4],
            &q_scalar[5],
            &q_scalar[6],
            &q_scalar[7]);

        ran_projective_8x packed_r;
        ran_complete_add_ifma_8x(&packed_r, &packed_p, &packed_q);

        ran_projective r_ifma[8];
        ran_unpack_proj_8x(
            &r_ifma[0], &r_ifma[1], &r_ifma[2], &r_ifma[3], &r_ifma[4], &r_ifma[5], &r_ifma[6], &r_ifma[7], &packed_r);

        int mismatches = 0;
        for (int k = 0; k < 8; k++)
        {
            ran_projective r_scalar;
            ran_complete_add(&r_scalar, &p_scalar[k], &q_scalar[k]);

            ran_jacobian j_scalar, j_ifma;
            ran_proj_to_jac(&j_scalar, &r_scalar);
            ran_proj_to_jac(&j_ifma, &r_ifma[k]);

            bool s_id = ran_is_identity(&j_scalar);
            bool i_id = ran_is_identity(&j_ifma);
            if (s_id != i_id)
            {
                mismatches++;
                continue;
            }
            if (s_id)
            {
                continue;
            }

            unsigned char s_bytes[32], i_bytes[32];
            ran_tobytes(s_bytes, &j_scalar);
            ran_tobytes(i_bytes, &j_ifma);
            if (std::memcmp(s_bytes, i_bytes, 32) != 0)
            {
                mismatches++;
            }
        }
        check_int("Ran complete_add_ifma_8x == 8 scalar ran_complete_add", 0, mismatches);
    }

    void run_ran_ifma_8x_invariants()
    {
        std::cout << std::endl << "=== Ran IFMA 8x invariants ===" << std::endl;

        ran_projective_8x id_pk;
        ran_proj_identity_8x(&id_pk);

        /* First: does unpacking an identity-initialized 8x give 8 identity projectives? */
        {
            ran_projective direct_parts[8];
            ran_unpack_proj_8x(
                &direct_parts[0],
                &direct_parts[1],
                &direct_parts[2],
                &direct_parts[3],
                &direct_parts[4],
                &direct_parts[5],
                &direct_parts[6],
                &direct_parts[7],
                &id_pk);
            int direct_failed = 0;
            for (int k = 0; k < 8; k++)
            {
                if (!ran_proj_is_identity(&direct_parts[k]))
                    direct_failed++;
            }
            check_int("unpack of ran_proj_identity_8x gives 8 identities", 0, direct_failed);
        }

        ran_projective_8x doubled;
        ran_complete_add_ifma_8x(&doubled, &id_pk, &id_pk);

        ran_projective parts[8];
        ran_unpack_proj_8x(
            &parts[0], &parts[1], &parts[2], &parts[3], &parts[4], &parts[5], &parts[6], &parts[7], &doubled);

        int failed = 0;
        for (int k = 0; k < 8; k++)
        {
            if (!ran_proj_is_identity(&parts[k]))
                failed++;
        }
        check_int("doubling of identity-in-all-8-lanes stays identity", 0, failed);

        /* Chained RCB doublings with DISTINCT per-lane inputs: compare 3 chained
         * IFMA doublings of (P0..P7) with different scalar multiples of G
         * against 8 scalar chained doublings. */
        {
            ran_jacobian Gj;
            fp_copy(Gj.X, RAN_GX);
            fp_copy(Gj.Y, RAN_GY);
            fp_1(Gj.Z);
            ran_projective lane_in[8];
            for (int k = 0; k < 8; k++)
            {
                unsigned char sk[32] = {0};
                sk[0] = (unsigned char)(k + 1);
                ran_jacobian mk;
                ran_scalarmult_vartime(&mk, sk, &Gj);
                ran_jac_to_proj(&lane_in[k], &mk);
            }
            ran_projective_8x packed;
            ran_pack_proj_8x(
                &packed,
                &lane_in[0],
                &lane_in[1],
                &lane_in[2],
                &lane_in[3],
                &lane_in[4],
                &lane_in[5],
                &lane_in[6],
                &lane_in[7]);
            ran_projective_8x A, B;
            ran_complete_add_ifma_8x(&A, &packed, &packed); /* 2*P_k per lane */
            ran_complete_add_ifma_8x(&B, &A, &A); /* 4*P_k */
            ran_complete_add_ifma_8x(&A, &B, &B); /* 8*P_k */

            ran_projective out_lanes[8];
            ran_unpack_proj_8x(
                &out_lanes[0],
                &out_lanes[1],
                &out_lanes[2],
                &out_lanes[3],
                &out_lanes[4],
                &out_lanes[5],
                &out_lanes[6],
                &out_lanes[7],
                &A);

            int fail = 0;
            for (int k = 0; k < 8; k++)
            {
                ran_projective s2, s4, s8;
                ran_complete_add(&s2, &lane_in[k], &lane_in[k]);
                ran_complete_add(&s4, &s2, &s2);
                ran_complete_add(&s8, &s4, &s4);
                ran_jacobian ja, jb;
                ran_proj_to_jac(&ja, &out_lanes[k]);
                ran_proj_to_jac(&jb, &s8);
                unsigned char ba[32], bb[32];
                ran_tobytes(ba, &ja);
                ran_tobytes(bb, &jb);
                if (std::memcmp(ba, bb, 32) != 0)
                    fail++;
            }
            check_int("3 chained IFMA doublings with distinct-per-lane inputs", 0, fail);
        }

        /* RCB with one side = identity in all lanes, other side = distinct-per-lane.
         * Regression: the MSM driver's add-to-accum at the first non-zero window
         * hits this shape and must produce the distinct-per-lane on the other
         * side. (void fallthrough for the outer lambda.) */
        {
            ran_jacobian Gj;
            fp_copy(Gj.X, RAN_GX);
            fp_copy(Gj.Y, RAN_GY);
            fp_1(Gj.Z);
            ran_projective Gp;
            ran_jac_to_proj(&Gp, &Gj);

            ran_projective mults[8];
            for (int k = 0; k < 8; k++)
            {
                if (k < 4)
                {
                    /* lanes 0..3: 5G, 7G, 3G, 2G */
                    unsigned char sk[32] = {0};
                    int factors[4] = {5, 7, 3, 2};
                    sk[0] = (unsigned char)factors[k];
                    ran_jacobian mj;
                    ran_scalarmult_vartime(&mj, sk, &Gj);
                    ran_jac_to_proj(&mults[k], &mj);
                }
                else
                {
                    ran_proj_identity(&mults[k]);
                }
            }

            ran_projective_8x id_8x, mix_8x;
            ran_proj_identity_8x(&id_8x);
            ran_pack_proj_8x(
                &mix_8x, &mults[0], &mults[1], &mults[2], &mults[3], &mults[4], &mults[5], &mults[6], &mults[7]);

            ran_projective_8x sum_8x;
            ran_complete_add_ifma_8x(&sum_8x, &id_8x, &mix_8x);

            ran_projective out[8];
            ran_unpack_proj_8x(&out[0], &out[1], &out[2], &out[3], &out[4], &out[5], &out[6], &out[7], &sum_8x);

            int fail = 0;
            for (int k = 0; k < 8; k++)
            {
                ran_jacobian ja, jb;
                ran_proj_to_jac(&ja, &out[k]);
                ran_proj_to_jac(&jb, &mults[k]);

                if (ran_is_identity(&ja) != ran_is_identity(&jb))
                {
                    fail++;
                    continue;
                }
                if (ran_is_identity(&ja))
                    continue;

                unsigned char ba[32], bb[32];
                ran_tobytes(ba, &ja);
                ran_tobytes(bb, &jb);
                if (std::memcmp(ba, bb, 32) != 0)
                    fail++;
            }
            check_int("RCB(id_8x, distinct-per-lane) == distinct-per-lane", 0, fail);
        }

        /* Chained RCB doublings on non-identity input: compare 3 chained IFMA
         * doublings of G against 3 chained scalar doublings. This catches bound
         * issues that only fire when inputs have non-canonical upper bits. */
        {
            ran_jacobian Gj;
            fp_copy(Gj.X, RAN_GX);
            fp_copy(Gj.Y, RAN_GY);
            fp_1(Gj.Z);
            ran_projective Gp;
            ran_jac_to_proj(&Gp, &Gj);

            ran_projective_8x packed_G;
            ran_pack_proj_8x(&packed_G, &Gp, &Gp, &Gp, &Gp, &Gp, &Gp, &Gp, &Gp);
            ran_projective_8x A, B;
            ran_complete_add_ifma_8x(&A, &packed_G, &packed_G); /* 2G */
            ran_complete_add_ifma_8x(&B, &A, &A); /* 4G */
            ran_complete_add_ifma_8x(&A, &B, &B); /* 8G */

            ran_projective lanes[8];
            ran_unpack_proj_8x(
                &lanes[0], &lanes[1], &lanes[2], &lanes[3], &lanes[4], &lanes[5], &lanes[6], &lanes[7], &A);

            /* Scalar reference: 8G via three scalar RCB doublings. */
            ran_projective s2, s4, s8;
            ran_complete_add(&s2, &Gp, &Gp);
            ran_complete_add(&s4, &s2, &s2);
            ran_complete_add(&s8, &s4, &s4);

            int chain_failed = 0;
            for (int k = 0; k < 8; k++)
            {
                ran_jacobian ja, jb;
                ran_proj_to_jac(&ja, &lanes[k]);
                ran_proj_to_jac(&jb, &s8);
                unsigned char ba[32], bb[32];
                ran_tobytes(ba, &ja);
                ran_tobytes(bb, &jb);
                if (std::memcmp(ba, bb, 32) != 0)
                    chain_failed++;
            }
            check_int("3 chained IFMA doublings of G == 8G", 0, chain_failed);
        }

        /* Regression: the IFMA RCB must handle scalar-fed input where limbs may
         * already be at ~52 bits (fp output can represent values up to ~2p).
         * Chained through to another RCB those would hit 53 bits and be truncated
         * by vpmadd52. The RCB's defensive input normalize_weak guards against
         * this; this test catches a regression if someone removes it. */
        {
            std::vector<ran_projective_8x> vec_accum(1);
            ran_proj_identity_8x(&vec_accum[0]);
            ran_projective_8x tmp;
            ran_complete_add_ifma_8x(&tmp, &vec_accum[0], &vec_accum[0]);
            ran_complete_add_ifma_8x(&vec_accum[0], &tmp, &tmp);
            ran_projective vparts[8];
            ran_unpack_proj_8x(
                &vparts[0],
                &vparts[1],
                &vparts[2],
                &vparts[3],
                &vparts[4],
                &vparts[5],
                &vparts[6],
                &vparts[7],
                &vec_accum[0]);
            int vec_failed = 0;
            for (int k = 0; k < 8; k++)
            {
                if (!ran_proj_is_identity(&vparts[k]))
                    vec_failed++;
            }
            check_int("vector-backed chained doublings stay identity", 0, vec_failed);
        }
    }

    void run_ran_msm_ct_ifma_trivial()
    {
        std::cout << std::endl << "=== Ran CT MSM IFMA trivial cases ===" << std::endl;

        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        /* n = 4, all-zero scalars, all-G points. Expected: identity. */
        {
            unsigned char zero_scalars[32 * 4] = {0};
            ran_jacobian pts[4];
            for (int i = 0; i < 4; i++)
                ran_copy(&pts[i], &G);
            ran_jacobian r;
            ran_msm_ct_ifma(&r, zero_scalars, pts, 4);
            check_nonzero("n=4 all-zero scalars == identity", ran_is_identity(&r));
        }

        /* n = 4, scalar[0] = 1, rest = 0, all points = G. Expected: G. */
        {
            unsigned char scalars[32 * 4] = {0};
            scalars[0] = 1;
            ran_jacobian pts[4];
            for (int i = 0; i < 4; i++)
                ran_copy(&pts[i], &G);
            ran_jacobian r;
            ran_msm_ct_ifma(&r, scalars, pts, 4);
            unsigned char buf[32];
            ran_tobytes(buf, &r);
            check_bytes("n=4 scalars=(1,0,0,0) pts=(G,G,G,G) == G", tv::compressed_points::ran_g, buf, 32);
        }

        /* n = 4, scalars all = 1, all points = G. Expected: 4G. Compare with scalar CT. */
        {
            unsigned char scalars[32 * 4] = {0};
            for (int i = 0; i < 4; i++)
                scalars[32 * i] = 1;
            ran_jacobian pts[4];
            for (int i = 0; i < 4; i++)
                ran_copy(&pts[i], &G);
            ran_jacobian r_ifma, r_scalar;
            ran_msm_ct_ifma(&r_ifma, scalars, pts, 4);
            ran_msm_ct_scalar(&r_scalar, scalars, pts, 4);
            unsigned char buf_ifma[32], buf_scalar[32];
            ran_tobytes(buf_ifma, &r_ifma);
            ran_tobytes(buf_scalar, &r_scalar);
            check_bytes("n=4 scalars=(1,1,1,1) pts=(G*4) IFMA == scalar CT", buf_scalar, buf_ifma, 32);
        }

        /* n = 4, scalars = small random byte pattern, points = G. */
        {
            unsigned char scalars[32 * 4] = {0};
            scalars[32 * 0] = 5;
            scalars[32 * 1] = 7;
            scalars[32 * 2] = 3;
            scalars[32 * 3] = 2;
            ran_jacobian pts[4];
            for (int i = 0; i < 4; i++)
                ran_copy(&pts[i], &G);
            ran_jacobian r_ifma, r_scalar;
            ran_msm_ct_ifma(&r_ifma, scalars, pts, 4);
            ran_msm_ct_scalar(&r_scalar, scalars, pts, 4);
            unsigned char buf_ifma[32], buf_scalar[32];
            ran_tobytes(buf_ifma, &r_ifma);
            ran_tobytes(buf_scalar, &r_scalar);
            check_bytes("n=4 small-scalars (5,7,3,2) pts=(G*4) IFMA == scalar CT", buf_scalar, buf_ifma, 32);
        }

        /* n = 4, scalars = distinct small, points = distinct multiples of G. */
        {
            unsigned char scalars[32 * 4] = {0};
            scalars[32 * 0] = 3;
            scalars[32 * 1] = 5;
            scalars[32 * 2] = 11;
            scalars[32 * 3] = 13;
            ran_jacobian pts[4];
            unsigned char seed[32] = {0};
            for (int i = 0; i < 4; i++)
            {
                seed[0] = (unsigned char)(0x10 + i);
                ran_scalarmult_vartime(&pts[i], seed, &G);
            }
            ran_jacobian r_ifma, r_scalar;
            ran_msm_ct_ifma(&r_ifma, scalars, pts, 4);
            ran_msm_ct_scalar(&r_scalar, scalars, pts, 4);
            unsigned char buf_ifma[32], buf_scalar[32];
            ran_tobytes(buf_ifma, &r_ifma);
            ran_tobytes(buf_scalar, &r_scalar);
            check_bytes("n=4 small-scalars distinct-pts IFMA == scalar CT", buf_scalar, buf_ifma, 32);
        }
    }

    void run_ran_msm_ct_ifma_vs_scalar()
    {
        std::cout << std::endl << "=== Ran CT MSM IFMA vs scalar CT MSM (byte-equal) ===" << std::endl;

        ran_jacobian G;
        fp_copy(G.X, RAN_GX);
        fp_copy(G.Y, RAN_GY);
        fp_1(G.Z);

        uint64_t rng = 0x11FA0C1FC71FA00DULL;
        int mismatches = 0;

        /* n ranges: drive the driver across its <4 fallback, crossover, and
         * full 8-lane + padded + multi-group regimes. */
        const size_t n_values[] = {1, 3, 4, 7, 8, 9, 15, 16, 17, 24, 32};
        for (size_t n : n_values)
        {
            for (int trial = 0; trial < 32; trial++)
            {
                std::vector<unsigned char> scalars(32 * n);
                std::vector<ran_jacobian> points(n);
                for (size_t i = 0; i < n; i++)
                {
                    unsigned char seed[32];
                    lcg_fill(seed, 32, &rng);
                    seed[31] &= 0x7F;
                    ran_scalarmult_vartime(&points[i], seed, &G);
                    lcg_fill(&scalars[32 * i], 32, &rng);
                    scalars[32 * i + 31] &= 0x7F;
                }

                ran_jacobian r_scalar, r_ifma;
                ran_msm_ct_scalar(&r_scalar, scalars.data(), points.data(), n);
                ran_msm_ct_ifma(&r_ifma, scalars.data(), points.data(), n);

                bool s_id = ran_is_identity(&r_scalar);
                bool i_id = ran_is_identity(&r_ifma);
                if (s_id != i_id)
                {
                    mismatches++;
                    continue;
                }
                if (s_id)
                {
                    continue;
                }

                unsigned char s_bytes[32], i_bytes[32];
                ran_tobytes(s_bytes, &r_scalar);
                ran_tobytes(i_bytes, &r_ifma);
                if (std::memcmp(s_bytes, i_bytes, 32) != 0)
                {
                    mismatches++;
                }
            }
        }
        check_int("Ran msm_ct_ifma == msm_ct across 11 sizes x 32 trials", 0, mismatches);
    }

    void run_shaw_msm_ct_ifma_vs_scalar()
    {
        std::cout << std::endl << "=== Shaw CT MSM IFMA vs scalar CT MSM (byte-equal) ===" << std::endl;

        shaw_jacobian G;
        fq_copy(G.X, SHAW_GX);
        fq_copy(G.Y, SHAW_GY);
        fq_1(G.Z);

        uint64_t rng = 0xF1FA57AA15551FAULL;
        int mismatches = 0;

        const size_t n_values[] = {1, 3, 4, 7, 8, 9, 15, 16, 17, 24, 32};
        for (size_t n : n_values)
        {
            for (int trial = 0; trial < 32; trial++)
            {
                std::vector<unsigned char> scalars(32 * n);
                std::vector<shaw_jacobian> points(n);
                for (size_t i = 0; i < n; i++)
                {
                    unsigned char seed[32];
                    lcg_fill(seed, 32, &rng);
                    seed[31] &= 0x7F;
                    shaw_scalarmult_vartime(&points[i], seed, &G);
                    lcg_fill(&scalars[32 * i], 32, &rng);
                    scalars[32 * i + 31] &= 0x7F;
                }

                shaw_jacobian r_scalar, r_ifma;
                shaw_msm_ct_scalar(&r_scalar, scalars.data(), points.data(), n);
                shaw_msm_ct_ifma(&r_ifma, scalars.data(), points.data(), n);

                bool s_id = shaw_is_identity(&r_scalar);
                bool i_id = shaw_is_identity(&r_ifma);
                if (s_id != i_id)
                {
                    mismatches++;
                    continue;
                }
                if (s_id)
                {
                    continue;
                }

                unsigned char s_bytes[32], i_bytes[32];
                shaw_tobytes(s_bytes, &r_scalar);
                shaw_tobytes(i_bytes, &r_ifma);
                if (std::memcmp(s_bytes, i_bytes, 32) != 0)
                {
                    mismatches++;
                }
            }
        }
        check_int("Shaw msm_ct_ifma == msm_ct across 11 sizes x 32 trials", 0, mismatches);
    }

} // namespace

// AVX-512 worker body. Called only from run_complete_add_ifma_suite() in
// complete_add.cpp, and only after a baseline-ISA ranshaw_has_avx512ifma()
// guard has passed. The guard cannot live in this TU: it is compiled with
// -mavx512f/-mavx512ifma, so a guard here would emit AVX-512 in its own
// prologue and fault on a baseline CPU before the check ran. See the entry in
// complete_add.cpp for the full note.
void ranshaw_run_complete_add_ifma_impl()
{
    run_ran_complete_add_ifma_vs_scalar();
    run_ran_ifma_8x_invariants();
    run_ran_msm_ct_ifma_trivial();
    run_ran_msm_ct_ifma_vs_scalar();
    run_shaw_msm_ct_ifma_vs_scalar();
}

#else // IFMA backend not compiled in this build

void ranshaw_run_complete_add_ifma_impl() {}

#endif
