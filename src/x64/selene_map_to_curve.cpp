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
 * @file x64/selene_map_to_curve.cpp
 * @brief Simplified SWU map-to-curve for Selene (RFC 9380 section 6.6.2).
 *
 * Selene: y^2 = x^3 - 3x + b over F_q (q = 2^255 - gamma).
 * A = -3, B = b. Since A != 0 and B != 0, simplified SWU applies directly.
 * Z = -4 (non-square in F_q, g(B/(Z*A)) is square).
 *
 * Since q ≡ 3 (mod 4), fq_sqrt computes z^((q+1)/4) which is the principal
 * square root when z is a QR. To check if gx is a QR, we compute sqrt and
 * verify by squaring.
 */

#include "selene_map_to_curve.h"

#include "fq_frombytes.h"
#include "fq_invert.h"
#include "fq_mul.h"
#include "fq_ops.h"
#include "fq_sq.h"
#include "fq_sqrt.h"
#include "fq_tobytes.h"
#include "fq_utils.h"
#include "selene_add.h"
#include "selene_constants.h"
#include "selene_ops.h"

/* Z = -4 mod q */
static const fq_fe SSWU_Z =
    {0x6d2727927c79bULL, 0x596ecad6b0dd6ULL, 0x7fffffefdfde0ULL, 0x7ffffffffffffULL, 0x7ffffffffffffULL};

/* -B/A = b/3 mod q */
static const fq_fe SSWU_NEG_B_OVER_A =
    {0x7588143c8c1c8ULL, 0x6a047460099b3ULL, 0x7ffd8a29a1b0fULL, 0x1203fe2f49b98ULL, 0x255b7d067872dULL};

/* B/(Z*A) = b/(-4*(-3)) mod q = b/12 mod q */
static const fq_fe SSWU_B_OVER_ZA =
    {0x7d62050f23072ULL, 0x7a811d180266cULL, 0x1fff628a686c3ULL, 0x2480ff8bd26e6ULL, 0x0956df419e1cbULL};

/* A = -3 mod q */
static const fq_fe SSWU_A =
    {0x6d2727927c79cULL, 0x596ecad6b0dd6ULL, 0x7fffffefdfde0ULL, 0x7ffffffffffffULL, 0x7ffffffffffffULL};

/* Check if two field elements are equal by serializing and comparing */
static int fq_equal(const fq_fe a, const fq_fe b)
{
    unsigned char sa[32], sb[32];
    fq_tobytes(sa, a);
    fq_tobytes(sb, b);
    unsigned char d = 0;
    for (int i = 0; i < 32; i++)
        d |= sa[i] ^ sb[i];
    return d == 0;
}

/*
 * Simplified SWU (RFC 9380 section 6.6.2)
 *
 * Input: field element u
 * Output: Jacobian point (x:y:1) on Selene
 */
static void sswu_selene(selene_jacobian *r, const fq_fe u)
{
    fq_fe u2, u4, Zu2, Z2u4, denom, tv1;
    fq_fe x1, gx1, x2, gx2;
    fq_fe x, y;

    /* u^2 */
    fq_sq(u2, u);

    /* Z * u^2 */
    fq_mul(Zu2, SSWU_Z, u2);

    /* Z^2 * u^4 */
    fq_sq(u4, u2);
    fq_fe Z2;
    fq_sq(Z2, SSWU_Z);
    fq_mul(Z2u4, Z2, u4);

    /* denom = Z^2*u^4 + Z*u^2 */
    fq_add(denom, Z2u4, Zu2);

    /* tv1 = inv0(denom) -- returns 0 if denom is 0 */
    int denom_is_zero = !fq_isnonzero(denom);
    if (denom_is_zero)
    {
        /* x1 = B/(Z*A) (exceptional case) */
        fq_copy(x1, SSWU_B_OVER_ZA);
    }
    else
    {
        fq_invert(tv1, denom);
        /* x1 = (-B/A) * (1 + tv1) */
        fq_fe one_plus_tv1;
        fq_fe one;
        fq_1(one);
        fq_add(one_plus_tv1, one, tv1);
        fq_mul(x1, SSWU_NEG_B_OVER_A, one_plus_tv1);
    }

    /* gx1 = x1^3 + A*x1 + B */
    fq_fe x1_sq, x1_cu, ax1;
    fq_sq(x1_sq, x1);
    fq_mul(x1_cu, x1_sq, x1);
    fq_mul(ax1, SSWU_A, x1);
    fq_add(gx1, x1_cu, ax1);
    fq_add(gx1, gx1, SELENE_B);

    /* x2 = Z * u^2 * x1 */
    fq_mul(x2, Zu2, x1);

    /* gx2 = x2^3 + A*x2 + B */
    fq_fe x2_sq, x2_cu, ax2;
    fq_sq(x2_sq, x2);
    fq_mul(x2_cu, x2_sq, x2);
    fq_mul(ax2, SSWU_A, x2);
    fq_add(gx2, x2_cu, ax2);
    fq_add(gx2, gx2, SELENE_B);

    /* Try sqrt(gx1); verify by squaring since fq_sqrt returns void */
    fq_fe sqrt_out, check;
    fq_sqrt(sqrt_out, gx1);
    fq_sq(check, sqrt_out);
    int gx1_is_square = fq_equal(check, gx1);

    if (gx1_is_square)
    {
        fq_copy(x, x1);
        fq_copy(y, sqrt_out);
    }
    else
    {
        fq_copy(x, x2);
        fq_sqrt(y, gx2);
    }

    /* sgn0(u) != sgn0(y) => negate y */
    int u_sign = fq_isnegative(u);
    int y_sign = fq_isnegative(y);
    if (u_sign != y_sign)
    {
        fq_neg(y, y);
    }

    /* Output as Jacobian with Z=1 */
    fq_copy(r->X, x);
    fq_copy(r->Y, y);
    fq_1(r->Z);
}

void selene_map_to_curve_x64(selene_jacobian *r, const unsigned char u[32])
{
    fq_fe u_fe;
    fq_frombytes(u_fe, u);
    sswu_selene(r, u_fe);
}

void selene_map_to_curve2_x64(selene_jacobian *r, const unsigned char u0[32], const unsigned char u1[32])
{
    selene_jacobian p0, p1;
    selene_map_to_curve_x64(&p0, u0);
    selene_map_to_curve_x64(&p1, u1);
    selene_add(r, &p0, &p1);
}
