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

#include "divisor.h"

#include "fp_invert.h"
#include "fp_mul.h"
#include "fp_ops.h"
#include "fp_sq.h"
#include "fq_invert.h"
#include "fq_mul.h"
#include "fq_ops.h"
#include "fq_sq.h"
#include "poly.h"

/* ---- Helpers: copy fp_fe in/out of storage ---- */

static inline void fp_fe_store(fp_fe_storage *dst, const fp_fe src)
{
    std::memcpy(dst->v, src, sizeof(fp_fe));
}

static inline void fp_fe_load(fp_fe dst, const fp_fe_storage *src)
{
    std::memcpy(dst, src->v, sizeof(fp_fe));
}

static inline void fq_fe_store(fq_fe_storage *dst, const fq_fe src)
{
    std::memcpy(dst->v, src, sizeof(fq_fe));
}

static inline void fq_fe_load(fq_fe dst, const fq_fe_storage *src)
{
    std::memcpy(dst, src->v, sizeof(fq_fe));
}

/* ================================================================
 * Helios (F_p) divisor operations
 * ================================================================ */

/*
 * Compute divisor witness D(x,y) = a(x) - y*b(x) for a set of affine points.
 *
 * Construction via Lagrange interpolation:
 *   1. v(x) = prod(x - x_i) for all i (degree n, vanishing polynomial)
 *   2. For each point i, compute Lagrange basis L_i(x) = v(x)/(x - x_i) / w_i
 *      where w_i = prod_{j!=i}(x_i - x_j)
 *   3. b(x) = sum_i(y_i * L_i(x))       -- interpolates y-coordinates
 *   4. a(x) = sum_i(y_i^2 * L_i(x))     -- interpolates y_i^2 values
 *
 * Then D(x_i, y_i) = a(x_i) - y_i * b(x_i) = y_i^2 - y_i * y_i = 0.
 */
void helios_compute_divisor(helios_divisor *d, const helios_affine *points, size_t n)
{
    if (n == 0)
    {
        /* Degenerate: return zero divisor */
        d->a.coeffs.resize(1);
        fp_0(d->a.coeffs[0].v);
        d->b.coeffs.resize(1);
        fp_0(d->b.coeffs[0].v);
        return;
    }

    /*
     * Build flat array of x-coordinates for fp_poly_from_roots.
     * fp_fe_storage has the same layout as fp_fe (no padding),
     * so reinterpret_cast to const fp_fe* is valid.
     */
    std::vector<fp_fe_storage> x_roots(n);
    for (size_t i = 0; i < n; i++)
    {
        std::memcpy(x_roots[i].v, points[i].x, sizeof(fp_fe));
    }

    /* Build vanishing polynomial v(x) = prod(x - x_i) */
    fp_poly v;
    fp_poly_from_roots(&v, reinterpret_cast<const fp_fe *>(x_roots.data()), n);

    /* Initialize accumulators for a(x) and b(x) with degree n-1 */
    size_t deg = (n > 1) ? n : 1; /* number of coefficients for degree n-1 */
    d->a.coeffs.resize(deg);
    d->b.coeffs.resize(deg);
    for (size_t k = 0; k < deg; k++)
    {
        fp_0(d->a.coeffs[k].v);
        fp_0(d->b.coeffs[k].v);
    }

    /* For each point, compute Lagrange basis L_i(x) and accumulate */
    for (size_t i = 0; i < n; i++)
    {
        /* L_num_i(x) = v(x) / (x - x_i) via polynomial division */
        fp_poly lin;
        lin.coeffs.resize(2);
        fp_neg(lin.coeffs[0].v, points[i].x);
        fp_1(lin.coeffs[1].v);

        fp_poly L_num, remainder;
        fp_poly_divmod(&L_num, &remainder, &v, &lin);

        /* w_i = prod_{j!=i}(x_i - x_j) */
        fp_fe wi;
        fp_1(wi);
        for (size_t j = 0; j < n; j++)
        {
            if (j == i)
                continue;
            fp_fe diff, tmp;
            fp_sub(diff, points[i].x, points[j].x);
            fp_mul(tmp, wi, diff);
            fp_copy(wi, tmp);
        }

        /* Invert w_i */
        fp_fe wi_inv;
        fp_invert(wi_inv, wi);

        /* Compute y_i and y_i^2 */
        fp_fe yi, yi_sq;
        fp_copy(yi, points[i].y);
        fp_sq(yi_sq, yi);

        /* Scale factors: y_i / w_i for b(x), y_i^2 / w_i for a(x) */
        fp_fe b_scale, a_scale;
        fp_mul(b_scale, yi, wi_inv);
        fp_mul(a_scale, yi_sq, wi_inv);

        /* Accumulate: b(x) += b_scale * L_num(x), a(x) += a_scale * L_num(x) */
        for (size_t k = 0; k < L_num.coeffs.size() && k < deg; k++)
        {
            fp_fe lk, prod_b, prod_a, cur_b, cur_a;
            fp_fe_load(lk, &L_num.coeffs[k]);

            fp_mul(prod_b, b_scale, lk);
            fp_fe_load(cur_b, &d->b.coeffs[k]);
            fp_add(cur_b, cur_b, prod_b);
            fp_fe_store(&d->b.coeffs[k], cur_b);

            fp_mul(prod_a, a_scale, lk);
            fp_fe_load(cur_a, &d->a.coeffs[k]);
            fp_add(cur_a, cur_a, prod_a);
            fp_fe_store(&d->a.coeffs[k], cur_a);
        }
    }
}

void helios_evaluate_divisor(fp_fe result, const helios_divisor *d, const fp_fe x, const fp_fe y)
{
    fp_fe ax, bx, ybx;
    fp_poly_eval(ax, &d->a, x);
    fp_poly_eval(bx, &d->b, x);
    fp_mul(ybx, y, bx);
    fp_sub(result, ax, ybx);
}

/* ================================================================
 * Selene (F_q) divisor operations
 * ================================================================ */

void selene_compute_divisor(selene_divisor *d, const selene_affine *points, size_t n)
{
    if (n == 0)
    {
        d->a.coeffs.resize(1);
        fq_0(d->a.coeffs[0].v);
        d->b.coeffs.resize(1);
        fq_0(d->b.coeffs[0].v);
        return;
    }

    std::vector<fq_fe_storage> x_roots(n);
    for (size_t i = 0; i < n; i++)
    {
        std::memcpy(x_roots[i].v, points[i].x, sizeof(fq_fe));
    }

    /* Build vanishing polynomial v(x) = prod(x - x_i) */
    fq_poly v;
    fq_poly_from_roots(&v, reinterpret_cast<const fq_fe *>(x_roots.data()), n);

    /* Initialize accumulators for a(x) and b(x) */
    size_t deg = (n > 1) ? n : 1;
    d->a.coeffs.resize(deg);
    d->b.coeffs.resize(deg);
    for (size_t k = 0; k < deg; k++)
    {
        fq_0(d->a.coeffs[k].v);
        fq_0(d->b.coeffs[k].v);
    }

    for (size_t i = 0; i < n; i++)
    {
        /* L_num_i(x) = v(x) / (x - x_i) */
        fq_poly lin;
        lin.coeffs.resize(2);
        fq_neg(lin.coeffs[0].v, points[i].x);
        fq_1(lin.coeffs[1].v);

        fq_poly L_num, remainder;
        fq_poly_divmod(&L_num, &remainder, &v, &lin);

        /* w_i = prod_{j!=i}(x_i - x_j) */
        fq_fe wi;
        fq_1(wi);
        for (size_t j = 0; j < n; j++)
        {
            if (j == i)
                continue;
            fq_fe diff, tmp;
            fq_sub(diff, points[i].x, points[j].x);
            fq_mul(tmp, wi, diff);
            fq_copy(wi, tmp);
        }

        fq_fe wi_inv;
        fq_invert(wi_inv, wi);

        fq_fe yi, yi_sq;
        fq_copy(yi, points[i].y);
        fq_sq(yi_sq, yi);

        fq_fe b_scale, a_scale;
        fq_mul(b_scale, yi, wi_inv);
        fq_mul(a_scale, yi_sq, wi_inv);

        for (size_t k = 0; k < L_num.coeffs.size() && k < deg; k++)
        {
            fq_fe lk, prod_b, prod_a, cur_b, cur_a;
            fq_fe_load(lk, &L_num.coeffs[k]);

            fq_mul(prod_b, b_scale, lk);
            fq_fe_load(cur_b, &d->b.coeffs[k]);
            fq_add(cur_b, cur_b, prod_b);
            fq_fe_store(&d->b.coeffs[k], cur_b);

            fq_mul(prod_a, a_scale, lk);
            fq_fe_load(cur_a, &d->a.coeffs[k]);
            fq_add(cur_a, cur_a, prod_a);
            fq_fe_store(&d->a.coeffs[k], cur_a);
        }
    }
}

void selene_evaluate_divisor(fq_fe result, const selene_divisor *d, const fq_fe x, const fq_fe y)
{
    fq_fe ax, bx, ybx;
    fq_poly_eval(ax, &d->a, x);
    fq_poly_eval(bx, &d->b, x);
    fq_mul(ybx, y, bx);
    fq_sub(result, ax, ybx);
}
