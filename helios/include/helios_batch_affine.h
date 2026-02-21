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

#ifndef HELIOSELENE_HELIOS_BATCH_AFFINE_H
#define HELIOSELENE_HELIOS_BATCH_AFFINE_H

/**
 * @file helios_batch_affine.h
 * @brief Batch Jacobian-to-affine conversion for Helios using Montgomery's trick.
 *
 * Converts n Jacobian points to affine using 1 inversion + 3(n-1) multiplications,
 * instead of n separate inversions.
 */

#include "fp_batch_invert.h"
#include "fp_mul.h"
#include "fp_sq.h"
#include "helios.h"
#include "helios_ops.h"

#include <vector>

/**
 * Convert n Jacobian points to affine using Montgomery's trick.
 * Identity points (Z==0) are mapped to (0,0).
 *
 * @param out   Output array of n affine points
 * @param points Input array of n Jacobian points
 * @param n     Number of points
 */
static inline void helios_batch_to_affine(helios_affine *out, const helios_jacobian *points, size_t n)
{
    if (n == 0)
        return;
    if (n == 1)
    {
        if (helios_is_identity(&points[0]))
        {
            fp_0(out[0].x);
            fp_0(out[0].y);
        }
        else
        {
            helios_to_affine(&out[0], &points[0]);
        }
        return;
    }

    /* Extract Z coordinates and batch-invert */
    struct fp_fe_s
    {
        fp_fe v;
    };
    std::vector<fp_fe_s> zs(n);
    std::vector<fp_fe_s> zinvs(n);
    for (size_t i = 0; i < n; i++)
        fp_copy(zs[i].v, points[i].Z);

    fp_batch_invert(&zinvs[0].v, &zs[0].v, n);

    /* Convert each point using its Z inverse */
    for (size_t i = 0; i < n; i++)
    {
        if (!fp_isnonzero(points[i].Z))
        {
            fp_0(out[i].x);
            fp_0(out[i].y);
        }
        else
        {
            fp_fe zi2, zi3;
            fp_sq(zi2, zinvs[i].v);
            fp_mul(zi3, zi2, zinvs[i].v);
            fp_mul(out[i].x, points[i].X, zi2);
            fp_mul(out[i].y, points[i].Y, zi3);
        }
    }
}

#endif // HELIOSELENE_HELIOS_BATCH_AFFINE_H
