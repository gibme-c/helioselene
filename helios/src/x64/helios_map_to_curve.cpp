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
 * @file x64/helios_map_to_curve.cpp
 * @brief Simplified SWU map-to-curve for Helios (RFC 9380 section 6.6.2).
 *
 * Helios: y^2 = x^3 - 3x + b over F_p (p = 2^255 - 19).
 * A = -3, B = b. Since A != 0 and B != 0, simplified SWU applies directly.
 * Z = 7 (non-square in F_p, g(B/(Z*A)) is square).
 */

#include "helios_map_to_curve.h"

#include "fp_frombytes.h"
#include "fp_invert.h"
#include "fp_mul.h"
#include "fp_ops.h"
#include "fp_sq.h"
#include "fp_sqrt.h"
#include "fp_tobytes.h"
#include "fp_utils.h"
#include "helios_add.h"
#include "helios_constants.h"
#include "helios_ops.h"

/* Z = 7 */
static const fp_fe SSWU_Z =
    {0x0000000000007ULL, 0x0000000000000ULL, 0x0000000000000ULL, 0x0000000000000ULL, 0x0000000000000ULL};

/* -B/A = b/3 mod p */
static const fp_fe SSWU_NEG_B_OVER_A =
    {0x6dfa0a49d139cULL, 0x502b627f78c1cULL, 0x0f9f9a405a9e9ULL, 0x01eca6e1be735ULL, 0x0ba2ed133af8dULL};

/* B/(Z*A) = b/(7*(-3)) mod p */
static const fp_fe SSWU_B_OVER_ZA =
    {0x27256c3e98f69ULL, 0x6242f1edca2d7ULL, 0x7dc4a0d23c327ULL, 0x7fb99f045281cULL, 0x7e56706af7934ULL};

/* A = -3 mod p */
static const fp_fe SSWU_A =
    {0x7ffffffffffeaULL, 0x7ffffffffffffULL, 0x7ffffffffffffULL, 0x7ffffffffffffULL, 0x7ffffffffffffULL};

/* Check if two field elements are equal by serializing and comparing */
static int fp_equal(const fp_fe a, const fp_fe b)
{
    unsigned char sa[32], sb[32];
    fp_tobytes(sa, a);
    fp_tobytes(sb, b);
    unsigned char d = 0;
    for (int i = 0; i < 32; i++)
        d |= sa[i] ^ sb[i];
    return d == 0;
}

/*
 * Simplified SWU (RFC 9380 section 6.6.2)
 *
 * Input: field element u
 * Output: Jacobian point (x:y:1) on Helios
 */
static void sswu_helios(helios_jacobian *r, const fp_fe u)
{
    fp_fe u2, u4, Zu2, Z2u4, denom, tv1;
    fp_fe x1, gx1, x2, gx2;
    fp_fe x, y;

    /* u^2 */
    fp_sq(u2, u);

    /* Z * u^2 */
    fp_mul(Zu2, SSWU_Z, u2);

    /* Z^2 * u^4 */
    fp_sq(u4, u2);
    fp_fe Z2;
    fp_sq(Z2, SSWU_Z);
    fp_mul(Z2u4, Z2, u4);

    /* denom = Z^2*u^4 + Z*u^2 */
    fp_add(denom, Z2u4, Zu2);

    /* tv1 = inv0(denom) -- returns 0 if denom is 0 */
    int denom_is_zero = !fp_isnonzero(denom);
    if (denom_is_zero)
    {
        /* x1 = B/(Z*A) (exceptional case) */
        fp_copy(x1, SSWU_B_OVER_ZA);
    }
    else
    {
        fp_invert(tv1, denom);
        /* x1 = (-B/A) * (1 + tv1) */
        fp_fe one_plus_tv1;
        fp_fe one;
        fp_1(one);
        fp_add(one_plus_tv1, one, tv1);
        fp_mul(x1, SSWU_NEG_B_OVER_A, one_plus_tv1);
    }

    /* gx1 = x1^3 + A*x1 + B */
    fp_fe x1_sq, x1_cu, ax1;
    fp_sq(x1_sq, x1);
    fp_mul(x1_cu, x1_sq, x1);
    fp_mul(ax1, SSWU_A, x1);
    fp_add(gx1, x1_cu, ax1);
    fp_add(gx1, gx1, HELIOS_B);

    /* x2 = Z * u^2 * x1 */
    fp_mul(x2, Zu2, x1);

    /* gx2 = x2^3 + A*x2 + B */
    fp_fe x2_sq, x2_cu, ax2;
    fp_sq(x2_sq, x2);
    fp_mul(x2_cu, x2_sq, x2);
    fp_mul(ax2, SSWU_A, x2);
    fp_add(gx2, x2_cu, ax2);
    fp_add(gx2, gx2, HELIOS_B);

    /* Try sqrt(gx1); verify by squaring since fp_sqrt may give false positives */
    fp_fe sqrt_out, check;
    fp_sqrt(sqrt_out, gx1);
    fp_sq(check, sqrt_out);
    int gx1_is_square = fp_equal(check, gx1);

    if (gx1_is_square)
    {
        fp_copy(x, x1);
        fp_copy(y, sqrt_out);
    }
    else
    {
        fp_copy(x, x2);
        fp_sqrt(y, gx2);
    }

    /* sgn0(u) != sgn0(y) => negate y */
    int u_sign = fp_isnegative(u);
    int y_sign = fp_isnegative(y);
    if (u_sign != y_sign)
    {
        fp_neg(y, y);
    }

    /* Output as Jacobian with Z=1 */
    fp_copy(r->X, x);
    fp_copy(r->Y, y);
    fp_1(r->Z);
}

void helios_map_to_curve_x64(helios_jacobian *r, const unsigned char u[32])
{
    fp_fe u_fe;
    fp_frombytes(u_fe, u);
    sswu_helios(r, u_fe);
}

void helios_map_to_curve2_x64(helios_jacobian *r, const unsigned char u0[32], const unsigned char u1[32])
{
    helios_jacobian p0, p1;
    helios_map_to_curve_x64(&p0, u0);
    helios_map_to_curve_x64(&p1, u1);
    helios_add(r, &p0, &p1);
}
