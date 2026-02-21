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

/**
 * @file divisor_eval.h
 * @brief Optimized divisor evaluation using structure-of-arrays (SoA) layout
 *        for batch point evaluation.
 */

#ifndef HELIOSELENE_DIVISOR_EVAL_H
#define HELIOSELENE_DIVISOR_EVAL_H

#include "divisor.h"
#include "helios.h"
#include "poly.h"
#include "selene.h"

#include <cstddef>

static const size_t EVAL_DOMAIN_SIZE = 256;

#if HELIOSELENE_PLATFORM_64BIT
static const int FP_EVALS_NLIMBS = 5;
static const int FQ_EVALS_NLIMBS = 5;
typedef uint64_t fp_evals_limb_t;
typedef uint64_t fq_evals_limb_t;
#else
static const int FP_EVALS_NLIMBS = 10;
static const int FQ_EVALS_NLIMBS = 10;
typedef int32_t fp_evals_limb_t;
typedef int32_t fq_evals_limb_t;
#endif

/**
 * Evaluation-domain polynomial representation (SoA layout).
 * limbs[j][i] = j-th limb of the field element at domain point i.
 * Contiguous limb arrays enable aligned SIMD load/store (no gather/scatter).
 */
struct fp_evals
{
    alignas(64) fp_evals_limb_t limbs[FP_EVALS_NLIMBS][EVAL_DOMAIN_SIZE];
    size_t degree; /* logical degree of underlying polynomial */
};

struct fq_evals
{
    alignas(64) fq_evals_limb_t limbs[FQ_EVALS_NLIMBS][EVAL_DOMAIN_SIZE];
    size_t degree;
};

/* SoA accessor: read element i from eval-domain polynomial */
static inline void fp_evals_get(fp_fe out, const fp_evals *ev, size_t i)
{
    for (int j = 0; j < FP_EVALS_NLIMBS; j++)
        out[j] = ev->limbs[j][i];
}

static inline void fp_evals_set(fp_evals *ev, size_t i, const fp_fe val)
{
    for (int j = 0; j < FP_EVALS_NLIMBS; j++)
        ev->limbs[j][i] = val[j];
}

static inline void fq_evals_get(fq_fe out, const fq_evals *ev, size_t i)
{
    for (int j = 0; j < FQ_EVALS_NLIMBS; j++)
        out[j] = ev->limbs[j][i];
}

static inline void fq_evals_set(fq_evals *ev, size_t i, const fq_fe val)
{
    for (int j = 0; j < FQ_EVALS_NLIMBS; j++)
        ev->limbs[j][i] = val[j];
}

/**
 * Evaluation-domain EC-divisor: D(x,y) = a(x) - y*b(x)
 * represented as evaluations at domain points.
 */
struct helios_eval_divisor
{
    fp_evals a;
    fp_evals b;
};

struct selene_eval_divisor
{
    fq_evals a;
    fq_evals b;
};

/* ---- Initialization (precompute tables, thread-safe) ---- */

void helios_eval_divisor_init();
void selene_eval_divisor_init();

/* ---- F_p eval-domain polynomial operations ---- */

void fp_evals_mul(fp_evals *r, const fp_evals *a, const fp_evals *b);
void fp_evals_add(fp_evals *r, const fp_evals *a, const fp_evals *b);
void fp_evals_sub(fp_evals *r, const fp_evals *a, const fp_evals *b);

void fp_evals_from_constant(fp_evals *r, const fp_fe c);
void fp_evals_from_linear(fp_evals *r, const fp_fe c); /* f(x) = x - c */

void fp_evals_to_poly(fp_poly *out, const fp_evals *ev);

void fp_evals_div_linear(fp_evals *q, const fp_evals *f, const fp_fe c);

/* ---- F_q eval-domain polynomial operations ---- */

void fq_evals_mul(fq_evals *r, const fq_evals *a, const fq_evals *b);
void fq_evals_add(fq_evals *r, const fq_evals *a, const fq_evals *b);
void fq_evals_sub(fq_evals *r, const fq_evals *a, const fq_evals *b);

void fq_evals_from_constant(fq_evals *r, const fq_fe c);
void fq_evals_from_linear(fq_evals *r, const fq_fe c);

void fq_evals_to_poly(fq_poly *out, const fq_evals *ev);

void fq_evals_div_linear(fq_evals *q, const fq_evals *f, const fq_fe c);

/* ---- Helios eval-domain divisor operations ---- */

void helios_eval_divisor_mul(helios_eval_divisor *r, const helios_eval_divisor *d1, const helios_eval_divisor *d2);

void helios_eval_divisor_from_point(helios_eval_divisor *d, const helios_affine *point);

void helios_eval_divisor_merge(
    helios_eval_divisor *r,
    const helios_eval_divisor *d1,
    const helios_eval_divisor *d2,
    const helios_affine *sum1,
    const helios_affine *sum2,
    const helios_affine *sum_total);

void helios_eval_divisor_to_divisor(helios_divisor *out, const helios_eval_divisor *ed);

void helios_eval_divisor_tree_reduce(
    helios_eval_divisor *out,
    helios_eval_divisor *divisors,
    helios_affine *points,
    size_t n);

void helios_scalar_mul_divisor(helios_divisor *d, const unsigned char *scalar, const helios_affine *point);

/* ---- Selene eval-domain divisor operations ---- */

void selene_eval_divisor_mul(selene_eval_divisor *r, const selene_eval_divisor *d1, const selene_eval_divisor *d2);

void selene_eval_divisor_from_point(selene_eval_divisor *d, const selene_affine *point);

void selene_eval_divisor_merge(
    selene_eval_divisor *r,
    const selene_eval_divisor *d1,
    const selene_eval_divisor *d2,
    const selene_affine *sum1,
    const selene_affine *sum2,
    const selene_affine *sum_total);

void selene_eval_divisor_to_divisor(selene_divisor *out, const selene_eval_divisor *ed);

void selene_eval_divisor_tree_reduce(
    selene_eval_divisor *out,
    selene_eval_divisor *divisors,
    selene_affine *points,
    size_t n);

void selene_scalar_mul_divisor(selene_divisor *d, const unsigned char *scalar, const selene_affine *point);

#endif // HELIOSELENE_DIVISOR_EVAL_H
