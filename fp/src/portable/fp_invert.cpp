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

#include "portable/fp_invert.h"

#include "portable/fp_divsteps.h"
#include "ranshaw_secure_erase.h"

/*
 * z^(-1) mod p via Bernstein-Yang safegcd/divsteps. 25 outer rounds of 30
 * divsteps = 750 iterations, above the 738 bound for a 255-bit modulus.
 * Constant-time: fixed iteration count, no secret-dependent branches.
 */
void fp_invert_portable(fp_fe out, const fp_fe z)
{
    fp_signed30 f, g, d, e;
    f = FP_MODULUS_S30;
    fp_fe_to_signed30(&g, z);
    for (int i = 0; i < 9; i++)
        d.v[i] = 0;
    e.v[0] = 1;
    for (int i = 1; i < 9; i++)
        e.v[i] = 0;

    int32_t delta = 1;

    for (int i = 0; i < 25; i++)
    {
        fp_trans2x2_30 t;
        delta = fp_divsteps_30(delta, (uint32_t)f.v[0], (uint32_t)g.v[0], &t);
        fp_update_fg_30(&f, &g, &t);
        fp_update_de_30(&d, &e, &t);
    }

    fp_divsteps_normalize_30(out, &d, &f);

    ranshaw_secure_erase(&f, sizeof(f));
    ranshaw_secure_erase(&g, sizeof(g));
    ranshaw_secure_erase(&d, sizeof(d));
    ranshaw_secure_erase(&e, sizeof(e));
}
