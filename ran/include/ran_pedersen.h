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

#ifndef RANSHAW_RAN_PEDERSEN_H
#define RANSHAW_RAN_PEDERSEN_H

/**
 * @file ran_pedersen.h
 * @brief Pedersen vector commitment for Ran.
 *
 * Computes C = r*H + sum(a_i * G_i) using a single MSM call with n+1 pairs.
 *
 * Two entry points per the CT/vartime dual-path API policy:
 *   - ran_pedersen_commit — constant-time (CT) MSM. Safe when blinding or
 *     values are secret, e.g. FCMP++ prover with a secret blinding factor.
 *   - ran_pedersen_commit_vartime — variable-time MSM. Only safe when every
 *     scalar fed in (blinding included) is a public value.
 *
 * Both surfaces produce byte-identical commitments on any given input; they
 * differ only in timing characteristics. Callers must choose based on which
 * of their scalars are secret.
 */

#include "ran.h"
#include "ran_msm_ct.h"
#include "ran_msm_vartime.h"
#include "ran_ops.h"
#include "ranshaw_secure_erase.h"

#include <cstring>
#include <vector>

/* Shared body: pack [blinding, values...] and [H, generators...] into the
 * combined MSM arrays. The msm_fn parameter (ran_msm_ct or ran_msm_vartime)
 * is invoked through its inline dispatching wrapper, so both CT and VT
 * callers benefit from runtime backend selection. */
static inline void ran_pedersen_commit_impl_(
    ran_jacobian *result,
    const unsigned char *blinding,
    const ran_jacobian *H,
    const unsigned char *values,
    const ran_jacobian *generators,
    size_t n,
    void (*msm_fn)(ran_jacobian *, const unsigned char *, const ran_jacobian *, size_t))
{
    if (n > SIZE_MAX / 32 - 1)
    {
        ran_identity(result);
        return;
    }

    std::vector<unsigned char> all_scalars(32 * (n + 1));
    std::vector<ran_jacobian> all_points(n + 1);

    std::memcpy(all_scalars.data(), blinding, 32);
    ran_copy(&all_points[0], H);

    if (n > 0)
    {
        std::memcpy(all_scalars.data() + 32, values, 32 * n);
        for (size_t i = 0; i < n; i++)
            ran_copy(&all_points[i + 1], &generators[i]);
    }

    msm_fn(result, all_scalars.data(), all_points.data(), n + 1);
    ranshaw_secure_erase(all_scalars.data(), all_scalars.size());
}

/* Constant-time default. Secret blinding / value scalars are safe here. */
static inline void ran_pedersen_commit(
    ran_jacobian *result,
    const unsigned char *blinding,
    const ran_jacobian *H,
    const unsigned char *values,
    const ran_jacobian *generators,
    size_t n)
{
    /* ran_msm_ct is a static-inline dispatcher, not directly function-pointer
     * compatible; wrap it in a local trampoline so we can pass it through. */
    auto ct_trampoline = [](ran_jacobian *r, const unsigned char *s, const ran_jacobian *p, size_t m)
    { ran_msm_ct(r, s, p, m); };
    ran_pedersen_commit_impl_(result, blinding, H, values, generators, n, ct_trampoline);
}

/* Variable-time variant for public-input callers. */
static inline void ran_pedersen_commit_vartime(
    ran_jacobian *result,
    const unsigned char *blinding,
    const ran_jacobian *H,
    const unsigned char *values,
    const ran_jacobian *generators,
    size_t n)
{
    auto vt_trampoline = [](ran_jacobian *r, const unsigned char *s, const ran_jacobian *p, size_t m)
    { ran_msm_vartime(r, s, p, m); };
    ran_pedersen_commit_impl_(result, blinding, H, values, generators, n, vt_trampoline);
}

#endif // RANSHAW_RAN_PEDERSEN_H
