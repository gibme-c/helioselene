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

#include "portable/fq_invert.h"

#include "portable/fq_divsteps.h"
#include "ranshaw_secure_erase.h"

/*
 * z^(-1) mod q via Bernstein-Yang safegcd/divsteps. 25 outer rounds of 30
 * divsteps = 750 iterations, above the 738 bound for a 255-bit modulus.
 * Constant-time: fixed iteration count, no secret-dependent branches.
 */
void fq_invert_portable(fq_fe out, const fq_fe z)
{
    /* Initialize: f = q (modulus), g = z (input), d = 0, e = 1, delta = 1 */
    fq_signed30 f, g, d, e;
    f = FQ_MODULUS_S30;
    fq_fe_to_signed30(&g, z);
    for (int i = 0; i < 9; i++)
        d.v[i] = 0;
    e.v[0] = 1;
    for (int i = 1; i < 9; i++)
        e.v[i] = 0;

    int32_t delta = 1;

    /* 25 outer iterations x 30 divsteps = 750 total (>= 738 needed for 255-bit prime) */
    for (int i = 0; i < 25; i++)
    {
        fq_trans2x2_30 t;
        delta = fq_divsteps_30(delta, (uint32_t)f.v[0], (uint32_t)g.v[0], &t);
        fq_update_fg_30(&f, &g, &t);
        fq_update_de_30(&d, &e, &t);
    }

    /* Normalize: conditionally negate d based on sign of f, reduce to [0, q) */
    fq_divsteps_normalize_30(out, &d, &f);

    /* Secure erase all temporaries */
    ranshaw_secure_erase(&f, sizeof(f));
    ranshaw_secure_erase(&g, sizeof(g));
    ranshaw_secure_erase(&d, sizeof(d));
    ranshaw_secure_erase(&e, sizeof(e));
}
