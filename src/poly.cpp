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

#include "poly.h"

#include "fp_frombytes.h"
#include "fp_invert.h"
#include "fp_mul.h"
#include "fp_ops.h"
#include "fp_sq.h"
#include "fp_tobytes.h"
#include "fq_frombytes.h"
#include "fq_invert.h"
#include "fq_mul.h"
#include "fq_ops.h"
#include "fq_sq.h"
#include "fq_tobytes.h"

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

/* ---- Helpers: check if an fp_fe is zero (via byte comparison) ---- */

static int fp_fe_is_zero(const fp_fe f)
{
    unsigned char s[32];
    fp_tobytes(s, f);
    unsigned char d = 0;
    for (int i = 0; i < 32; i++)
        d |= s[i];
    return d == 0;
}

static int fq_fe_is_zero(const fq_fe f)
{
    unsigned char s[32];
    fq_tobytes(s, f);
    unsigned char d = 0;
    for (int i = 0; i < 32; i++)
        d |= s[i];
    return d == 0;
}

/* ---- Strip trailing zero coefficients ---- */

static void fp_poly_strip(fp_poly *p)
{
    while (p->coeffs.size() > 1)
    {
        fp_fe tmp;
        fp_fe_load(tmp, &p->coeffs.back());
        if (!fp_fe_is_zero(tmp))
            break;
        p->coeffs.pop_back();
    }
}

static void fq_poly_strip(fq_poly *p)
{
    while (p->coeffs.size() > 1)
    {
        fq_fe tmp;
        fq_fe_load(tmp, &p->coeffs.back());
        if (!fq_fe_is_zero(tmp))
            break;
        p->coeffs.pop_back();
    }
}

/* ================================================================
 * F_p polynomial operations
 * ================================================================ */

void fp_poly_mul(fp_poly *r, const fp_poly *a, const fp_poly *b)
{
    size_t na = a->coeffs.size();
    size_t nb = b->coeffs.size();

    if (na == 0 || nb == 0)
    {
        r->coeffs.clear();
        fp_fe_storage z;
        fp_0(z.v);
        r->coeffs.push_back(z);
        return;
    }

    size_t nr = na + nb - 1;
    r->coeffs.resize(nr);

    /* Zero-initialize all output coefficients */
    for (size_t k = 0; k < nr; k++)
    {
        fp_0(r->coeffs[k].v);
    }

    /* Schoolbook multiplication: r[k] = sum_{i} a[i] * b[k-i] */
    for (size_t i = 0; i < na; i++)
    {
        fp_fe ai;
        fp_fe_load(ai, &a->coeffs[i]);
        for (size_t j = 0; j < nb; j++)
        {
            fp_fe bj, prod, sum;
            fp_fe_load(bj, &b->coeffs[j]);
            fp_mul(prod, ai, bj);
            fp_fe_load(sum, &r->coeffs[i + j]);
            fp_add(sum, sum, prod);
            fp_fe_store(&r->coeffs[i + j], sum);
        }
    }

    fp_poly_strip(r);
}

void fp_poly_eval(fp_fe result, const fp_poly *p, const fp_fe x)
{
    size_t n = p->coeffs.size();
    if (n == 0)
    {
        fp_0(result);
        return;
    }

    /* Horner's method: start from highest coefficient */
    fp_fe_load(result, &p->coeffs[n - 1]);
    for (size_t i = n - 1; i > 0; i--)
    {
        fp_fe tmp, ci;
        fp_mul(tmp, result, x);
        fp_fe_load(ci, &p->coeffs[i - 1]);
        fp_add(result, tmp, ci);
    }
}

void fp_poly_from_roots(fp_poly *r, const fp_fe *roots, size_t n)
{
    if (n == 0)
    {
        /* Return the constant polynomial 1 */
        r->coeffs.resize(1);
        fp_1(r->coeffs[0].v);
        return;
    }

    /* Start with (x - roots[0]) = [-roots[0], 1] */
    r->coeffs.resize(2);
    fp_neg(r->coeffs[0].v, roots[0]);
    fp_1(r->coeffs[1].v);

    /* Multiply by (x - roots[i]) for i = 1..n-1 */
    for (size_t i = 1; i < n; i++)
    {
        fp_poly linear;
        linear.coeffs.resize(2);
        fp_neg(linear.coeffs[0].v, roots[i]);
        fp_1(linear.coeffs[1].v);

        fp_poly tmp;
        fp_poly_mul(&tmp, r, &linear);
        *r = tmp;
    }
}

void fp_poly_divmod(fp_poly *q, fp_poly *rem, const fp_poly *a, const fp_poly *b)
{
    size_t na = a->coeffs.size();
    size_t nb = b->coeffs.size();

    /* Copy a into remainder */
    *rem = *a;
    fp_poly_strip(rem);
    na = rem->coeffs.size();

    /* Strip b to get true degree */
    fp_poly bstrip = *b;
    fp_poly_strip(&bstrip);
    nb = bstrip.coeffs.size();

    /* If deg(a) < deg(b), quotient is 0 and remainder is a */
    if (na < nb)
    {
        q->coeffs.resize(1);
        fp_0(q->coeffs[0].v);
        return;
    }

    size_t nq = na - nb + 1;
    q->coeffs.resize(nq);
    for (size_t i = 0; i < nq; i++)
    {
        fp_0(q->coeffs[i].v);
    }

    /* Invert the leading coefficient of b */
    fp_fe b_lead, b_lead_inv;
    fp_fe_load(b_lead, &bstrip.coeffs[nb - 1]);
    fp_invert(b_lead_inv, b_lead);

    /* Long division: work from highest degree down */
    for (size_t i = na; i >= nb; i--)
    {
        fp_fe rem_lead, coeff;
        fp_fe_load(rem_lead, &rem->coeffs[i - 1]);
        fp_mul(coeff, rem_lead, b_lead_inv);
        fp_fe_store(&q->coeffs[i - nb], coeff);

        /* Subtract coeff * b * x^(i - nb) from remainder */
        for (size_t j = 0; j < nb; j++)
        {
            fp_fe bj, prod, rval, diff;
            fp_fe_load(bj, &bstrip.coeffs[j]);
            fp_mul(prod, coeff, bj);
            fp_fe_load(rval, &rem->coeffs[i - nb + j]);
            fp_sub(diff, rval, prod);
            fp_fe_store(&rem->coeffs[i - nb + j], diff);
        }
    }

    /* Trim remainder to degree < deg(b) and strip */
    rem->coeffs.resize(nb - 1 > 0 ? nb - 1 : 1);
    fp_poly_strip(rem);
    fp_poly_strip(q);
}

/* ================================================================
 * F_q polynomial operations
 * ================================================================ */

void fq_poly_mul(fq_poly *r, const fq_poly *a, const fq_poly *b)
{
    size_t na = a->coeffs.size();
    size_t nb = b->coeffs.size();

    if (na == 0 || nb == 0)
    {
        r->coeffs.clear();
        fq_fe_storage z;
        fq_0(z.v);
        r->coeffs.push_back(z);
        return;
    }

    size_t nr = na + nb - 1;
    r->coeffs.resize(nr);

    for (size_t k = 0; k < nr; k++)
    {
        fq_0(r->coeffs[k].v);
    }

    for (size_t i = 0; i < na; i++)
    {
        fq_fe ai;
        fq_fe_load(ai, &a->coeffs[i]);
        for (size_t j = 0; j < nb; j++)
        {
            fq_fe bj, prod, sum;
            fq_fe_load(bj, &b->coeffs[j]);
            fq_mul(prod, ai, bj);
            fq_fe_load(sum, &r->coeffs[i + j]);
            fq_add(sum, sum, prod);
            fq_fe_store(&r->coeffs[i + j], sum);
        }
    }

    fq_poly_strip(r);
}

void fq_poly_eval(fq_fe result, const fq_poly *p, const fq_fe x)
{
    size_t n = p->coeffs.size();
    if (n == 0)
    {
        fq_0(result);
        return;
    }

    fq_fe_load(result, &p->coeffs[n - 1]);
    for (size_t i = n - 1; i > 0; i--)
    {
        fq_fe tmp, ci;
        fq_mul(tmp, result, x);
        fq_fe_load(ci, &p->coeffs[i - 1]);
        fq_add(result, tmp, ci);
    }
}

void fq_poly_from_roots(fq_poly *r, const fq_fe *roots, size_t n)
{
    if (n == 0)
    {
        r->coeffs.resize(1);
        fq_1(r->coeffs[0].v);
        return;
    }

    r->coeffs.resize(2);
    fq_neg(r->coeffs[0].v, roots[0]);
    fq_1(r->coeffs[1].v);

    for (size_t i = 1; i < n; i++)
    {
        fq_poly linear;
        linear.coeffs.resize(2);
        fq_neg(linear.coeffs[0].v, roots[i]);
        fq_1(linear.coeffs[1].v);

        fq_poly tmp;
        fq_poly_mul(&tmp, r, &linear);
        *r = tmp;
    }
}

void fq_poly_divmod(fq_poly *q, fq_poly *rem, const fq_poly *a, const fq_poly *b)
{
    size_t na = a->coeffs.size();
    size_t nb = b->coeffs.size();

    *rem = *a;
    fq_poly_strip(rem);
    na = rem->coeffs.size();

    fq_poly bstrip = *b;
    fq_poly_strip(&bstrip);
    nb = bstrip.coeffs.size();

    if (na < nb)
    {
        q->coeffs.resize(1);
        fq_0(q->coeffs[0].v);
        return;
    }

    size_t nq = na - nb + 1;
    q->coeffs.resize(nq);
    for (size_t i = 0; i < nq; i++)
    {
        fq_0(q->coeffs[i].v);
    }

    fq_fe b_lead, b_lead_inv;
    fq_fe_load(b_lead, &bstrip.coeffs[nb - 1]);
    fq_invert(b_lead_inv, b_lead);

    for (size_t i = na; i >= nb; i--)
    {
        fq_fe rem_lead, coeff;
        fq_fe_load(rem_lead, &rem->coeffs[i - 1]);
        fq_mul(coeff, rem_lead, b_lead_inv);
        fq_fe_store(&q->coeffs[i - nb], coeff);

        for (size_t j = 0; j < nb; j++)
        {
            fq_fe bj_val, prod, rval, diff;
            fq_fe_load(bj_val, &bstrip.coeffs[j]);
            fq_mul(prod, coeff, bj_val);
            fq_fe_load(rval, &rem->coeffs[i - nb + j]);
            fq_sub(diff, rval, prod);
            fq_fe_store(&rem->coeffs[i - nb + j], diff);
        }
    }

    rem->coeffs.resize(nb - 1 > 0 ? nb - 1 : 1);
    fq_poly_strip(rem);
    fq_poly_strip(q);
}
