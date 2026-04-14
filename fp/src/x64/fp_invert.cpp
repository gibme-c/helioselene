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

#include "x64/fp_invert.h"

#include "ranshaw_secure_erase.h"
#include "x64/fp_divsteps.h"

/*
 * Compute z^(-1) mod p via Bernstein-Yang safegcd/divsteps.
 *
 * Replaces the Fermat exponentiation (z^(p-2)) addition chain (254 sq + 11 mul)
 * with ~12 rounds of 62 divsteps each, using cheap 256-bit integer ops instead
 * of field multiplications with Crandall reduction.
 *
 * Constant-time: fixed 12 x 62 = 744 iterations (>= 738 bound for 255-bit prime).
 */
void fp_invert_x64(fp_fe out, const fp_fe z)
{
    /* Initialize: f = p (modulus), g = z (input), d = 0, e = 1, delta = 1 */
    fp_signed62 f, g, d, e;
    f = FP_MODULUS_S62;
    fp_fe_to_signed62(&g, z);
    for (int i = 0; i < 5; i++)
        d.v[i] = 0;
    e.v[0] = 1;
    for (int i = 1; i < 5; i++)
        e.v[i] = 0;

    int64_t delta = 1;

    /* 12 outer iterations x 62 divsteps = 744 total (>= 738 needed for 255-bit prime) */
    for (int i = 0; i < 12; i++)
    {
        fp_trans2x2 t;
        delta = fp_divsteps_62(delta, (uint64_t)f.v[0], (uint64_t)g.v[0], &t);
        fp_update_fg(&f, &g, &t);
        fp_update_de(&d, &e, &t);
    }

    /* Normalize: conditionally negate d based on sign of f, reduce to [0, p) */
    fp_divsteps_normalize(out, &d, &f);

    /* Secure erase all temporaries */
    ranshaw_secure_erase(&f, sizeof(f));
    ranshaw_secure_erase(&g, sizeof(g));
    ranshaw_secure_erase(&d, sizeof(d));
    ranshaw_secure_erase(&e, sizeof(e));
}
