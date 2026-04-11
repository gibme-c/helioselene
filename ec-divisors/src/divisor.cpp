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

// divisor.cpp — EC-divisor witness computation via Lagrange interpolation.
// For a set of affine points {(x_i, y_i)}, builds D(x,y) = a(x) - y*b(x)
// where b interpolates the y-coordinates and a interpolates y^2 values.

#include "divisor.h"

#include "fp_mul.h"
#include "fp_ops.h"
#include "fp_sq.h"
#include "fp_tobytes.h"
#include "fq_mul.h"
#include "fq_ops.h"
#include "fq_sq.h"
#include "fq_tobytes.h"
#include "poly.h"
#include "ran_validate.h"
#include "shaw_validate.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace
{

    /* Duplicate-x detection helpers: distinct-x is required for Lagrange
     * interpolation. For small n we do O(n^2) compare; for larger n we
     * serialize each x to 32 bytes and sort + linear-scan. The threshold is
     * set so the O(n^2) path is fast enough for the typical FCMP++ divisor
     * sizes (n <= 32) and the sort path takes over for larger sets. */

    constexpr size_t DIVISOR_DUP_X_SCAN_THRESHOLD = 32;

    int ran_has_duplicate_x(const ran_affine *points, size_t n)
    {
        if (n < 2)
            return 0;

        if (n <= DIVISOR_DUP_X_SCAN_THRESHOLD)
        {
            for (size_t i = 0; i < n; i++)
            {
                for (size_t j = i + 1; j < n; j++)
                {
                    unsigned char a[32], b[32];
                    fp_tobytes(a, points[i].x);
                    fp_tobytes(b, points[j].x);
                    if (std::memcmp(a, b, 32) == 0)
                        return 1;
                }
            }
            return 0;
        }

        std::vector<std::array<unsigned char, 32>> xs(n);
        for (size_t i = 0; i < n; i++)
            fp_tobytes(xs[i].data(), points[i].x);
        std::sort(xs.begin(), xs.end());
        for (size_t i = 1; i < n; i++)
        {
            if (xs[i] == xs[i - 1])
                return 1;
        }
        return 0;
    }

    int shaw_has_duplicate_x(const shaw_affine *points, size_t n)
    {
        if (n < 2)
            return 0;

        if (n <= DIVISOR_DUP_X_SCAN_THRESHOLD)
        {
            for (size_t i = 0; i < n; i++)
            {
                for (size_t j = i + 1; j < n; j++)
                {
                    unsigned char a[32], b[32];
                    fq_tobytes(a, points[i].x);
                    fq_tobytes(b, points[j].x);
                    if (std::memcmp(a, b, 32) == 0)
                        return 1;
                }
            }
            return 0;
        }

        std::vector<std::array<unsigned char, 32>> xs(n);
        for (size_t i = 0; i < n; i++)
            fq_tobytes(xs[i].data(), points[i].x);
        std::sort(xs.begin(), xs.end());
        for (size_t i = 1; i < n; i++)
        {
            if (xs[i] == xs[i - 1])
                return 1;
        }
        return 0;
    }

} // namespace

/* ================================================================
 * Ran (F_p) divisor operations
 * ================================================================ */

/*
 * Compute divisor witness D(x,y) = a(x) - y*b(x) for a set of affine points.
 *
 * Construction via Lagrange interpolation:
 *   b(x) interpolates the y-coordinates through the x-coordinates
 *   a(x) interpolates the y^2 values through the x-coordinates
 *
 * Then D(x_i, y_i) = a(x_i) - y_i * b(x_i) = y_i^2 - y_i * y_i = 0.
 */
int ran_compute_divisor(ran_divisor *d, const ran_affine *points, size_t n)
{
    if (n == 0)
    {
        /* Degenerate: return zero divisor */
        d->a.coeffs.resize(1);
        fp_0(d->a.coeffs[0].v);
        d->b.coeffs.resize(1);
        fp_0(d->b.coeffs[0].v);
        return 0;
    }

    /* Validate: every point must be on the curve. */
    for (size_t i = 0; i < n; i++)
    {
        if (!ran_is_on_curve(&points[i]))
            return -1;
    }

    /* Validate: no two points may share an x-coordinate. Lagrange
     * interpolation is well-defined only for distinct x values; a
     * duplicate would produce a silently-wrong divisor. */
    if (ran_has_duplicate_x(points, n))
        return -1;

    /* Build flat arrays of x-coordinates, y-coordinates, and y^2 values */
    std::vector<fp_fe_storage> xs_store(n), ys_store(n), ysq_store(n);
    for (size_t i = 0; i < n; i++)
    {
        std::memcpy(xs_store[i].v, points[i].x, sizeof(fp_fe));
        std::memcpy(ys_store[i].v, points[i].y, sizeof(fp_fe));
        fp_sq(ysq_store[i].v, points[i].y);
    }

    const fp_fe *xs = reinterpret_cast<const fp_fe *>(xs_store.data());
    const fp_fe *ys = reinterpret_cast<const fp_fe *>(ys_store.data());
    const fp_fe *ysq = reinterpret_cast<const fp_fe *>(ysq_store.data());

    /* b(x) interpolates y-coordinates, a(x) interpolates y^2 values */
    fp_poly_interpolate(&d->b, xs, ys, n);
    fp_poly_interpolate(&d->a, xs, ysq, n);
    return 0;
}

int ran_evaluate_divisor(fp_fe result, const ran_divisor *d, const fp_fe x, const fp_fe y)
{
    /* Validate: (x, y) must satisfy the Ran curve equation. */
    ran_affine p;
    std::memcpy(p.x, x, sizeof(fp_fe));
    std::memcpy(p.y, y, sizeof(fp_fe));
    if (!ran_is_on_curve(&p))
        return -1;

    fp_fe ax, bx, ybx;
    fp_poly_eval(ax, &d->a, x);
    fp_poly_eval(bx, &d->b, x);
    fp_mul(ybx, y, bx);
    fp_sub(result, ax, ybx);
    return 0;
}

/* ================================================================
 * Shaw (F_q) divisor operations
 * ================================================================ */

int shaw_compute_divisor(shaw_divisor *d, const shaw_affine *points, size_t n)
{
    if (n == 0)
    {
        d->a.coeffs.resize(1);
        fq_0(d->a.coeffs[0].v);
        d->b.coeffs.resize(1);
        fq_0(d->b.coeffs[0].v);
        return 0;
    }

    for (size_t i = 0; i < n; i++)
    {
        if (!shaw_is_on_curve(&points[i]))
            return -1;
    }

    if (shaw_has_duplicate_x(points, n))
        return -1;

    std::vector<fq_fe_storage> xs_store(n), ys_store(n), ysq_store(n);
    for (size_t i = 0; i < n; i++)
    {
        std::memcpy(xs_store[i].v, points[i].x, sizeof(fq_fe));
        std::memcpy(ys_store[i].v, points[i].y, sizeof(fq_fe));
        fq_sq(ysq_store[i].v, points[i].y);
    }

    const fq_fe *xs = reinterpret_cast<const fq_fe *>(xs_store.data());
    const fq_fe *ys = reinterpret_cast<const fq_fe *>(ys_store.data());
    const fq_fe *ysq = reinterpret_cast<const fq_fe *>(ysq_store.data());

    fq_poly_interpolate(&d->b, xs, ys, n);
    fq_poly_interpolate(&d->a, xs, ysq, n);
    return 0;
}

int shaw_evaluate_divisor(fq_fe result, const shaw_divisor *d, const fq_fe x, const fq_fe y)
{
    shaw_affine p;
    std::memcpy(p.x, x, sizeof(fq_fe));
    std::memcpy(p.y, y, sizeof(fq_fe));
    if (!shaw_is_on_curve(&p))
        return -1;

    fq_fe ax, bx, ybx;
    fq_poly_eval(ax, &d->a, x);
    fq_poly_eval(bx, &d->b, x);
    fq_mul(ybx, y, bx);
    fq_sub(result, ax, ybx);
    return 0;
}
