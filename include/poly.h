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

#ifndef HELIOSELENE_POLY_H
#define HELIOSELENE_POLY_H

#include "fp.h"
#include "fq.h"

#include <cstring>
#include <vector>

/* Storage types for polynomial coefficients (plain arrays, copyable) */
struct fp_fe_storage
{
    fp_fe v;
};
struct fq_fe_storage
{
    fq_fe v;
};

/* Polynomial types - coefficients stored low-degree first */
struct fp_poly
{
    std::vector<fp_fe_storage> coeffs;
};

struct fq_poly
{
    std::vector<fq_fe_storage> coeffs;
};

/* ---- F_p polynomials ---- */

/* Multiply: r = a * b */
void fp_poly_mul(fp_poly *r, const fp_poly *a, const fp_poly *b);

/* Evaluate polynomial at point x (Horner's method) */
void fp_poly_eval(fp_fe result, const fp_poly *p, const fp_fe x);

/* Build polynomial from roots: (x - roots[0]) * (x - roots[1]) * ... */
void fp_poly_from_roots(fp_poly *r, const fp_fe *roots, size_t n);

/* Polynomial division: a = b*q + rem. Writes quotient to q, remainder to rem. */
void fp_poly_divmod(fp_poly *q, fp_poly *rem, const fp_poly *a, const fp_poly *b);

/* ---- F_q polynomials ---- */

void fq_poly_mul(fq_poly *r, const fq_poly *a, const fq_poly *b);
void fq_poly_eval(fq_fe result, const fq_poly *p, const fq_fe x);
void fq_poly_from_roots(fq_poly *r, const fq_fe *roots, size_t n);
void fq_poly_divmod(fq_poly *q, fq_poly *rem, const fq_poly *a, const fq_poly *b);

#endif // HELIOSELENE_POLY_H
