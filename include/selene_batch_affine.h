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

#ifndef HELIOSELENE_SELENE_BATCH_AFFINE_H
#define HELIOSELENE_SELENE_BATCH_AFFINE_H

/**
 * @file selene_batch_affine.h
 * @brief Batch Jacobian-to-affine conversion for Selene using Montgomery's trick.
 *
 * Converts n Jacobian points to affine using 1 inversion + 3(n-1) multiplications,
 * instead of n separate inversions.
 */

#include "fq_invert.h"
#include "fq_mul.h"
#include "fq_sq.h"
#include "selene.h"
#include "selene_ops.h"

#include <vector>

/**
 * Convert n Jacobian points to affine using Montgomery's trick.
 * Identity points (Z==0) are mapped to (0,0).
 *
 * @param out   Output array of n affine points
 * @param points Input array of n Jacobian points
 * @param n     Number of points
 */
static inline void selene_batch_to_affine(selene_affine *out, const selene_jacobian *points, size_t n)
{
    if (n == 0)
        return;
    if (n == 1)
    {
        if (selene_is_identity(&points[0]))
        {
            fq_0(out[0].x);
            fq_0(out[0].y);
        }
        else
        {
            selene_to_affine(&out[0], &points[0]);
        }
        return;
    }

    /* Accumulate products of Z coordinates */
    std::vector<fq_fe> acc(n);
    fq_copy(acc[0], points[0].Z);
    for (size_t i = 1; i < n; i++)
        fq_mul(acc[i], acc[i - 1], points[i].Z);

    /* Single inversion of the product of all Z values */
    fq_fe inv;
    fq_invert(inv, acc[n - 1]);

    /* Work backwards to recover individual Z inverses */
    for (size_t i = n - 1; i > 0; i--)
    {
        fq_fe zi;
        fq_mul(zi, inv, acc[i - 1]); /* zi = Z_i^{-1} */
        fq_mul(inv, inv, points[i].Z); /* update running inverse */

        if (!fq_isnonzero(points[i].Z))
        {
            fq_0(out[i].x);
            fq_0(out[i].y);
        }
        else
        {
            fq_fe zi2, zi3;
            fq_sq(zi2, zi);
            fq_mul(zi3, zi2, zi);
            fq_mul(out[i].x, points[i].X, zi2);
            fq_mul(out[i].y, points[i].Y, zi3);
        }
    }

    /* First element: inv now holds Z_0^{-1} */
    if (!fq_isnonzero(points[0].Z))
    {
        fq_0(out[0].x);
        fq_0(out[0].y);
    }
    else
    {
        fq_fe zi2, zi3;
        fq_sq(zi2, inv);
        fq_mul(zi3, zi2, inv);
        fq_mul(out[0].x, points[0].X, zi2);
        fq_mul(out[0].y, points[0].Y, zi3);
    }
}

#endif // HELIOSELENE_SELENE_BATCH_AFFINE_H
