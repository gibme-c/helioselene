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

#include "portable/fp_sqrt.h"

#include "fp_ops.h"
#include "fp_tobytes.h"
#include "helioselene_secure_erase.h"
#include "portable/fp25.h"
#include "portable/fp25_chain.h"

/* sqrt(-1) mod p — same constant, but in 10-limb representation */
static const fp_fe SQRT_M1 =
    {-32595792, -7943725, 9377950, 3500415, 12389472, -272473, -25146209, -2005654, 326686, 11406482};

/* Forward declaration */
void fp_pow22523_portable(fp_fe out, const fp_fe z);

int fp_sqrt_portable(fp_fe out, const fp_fe z)
{
    fp_fe beta, beta_sq, neg_z, check;

    /* beta = z^((p+3)/8) = pow22523(z) * z */
    fp_pow22523_portable(beta, z);
    fp25_chain_mul(beta, beta, z);

    /* check = beta^2 */
    fp25_chain_sq(beta_sq, beta);

    /* Is beta^2 == z? */
    fp_sub(check, beta_sq, z);
    unsigned char check_bytes[32];
    fp_tobytes(check_bytes, check);

    unsigned int is_zero = 1;
    for (int i = 0; i < 32; i++)
        is_zero &= (check_bytes[i] == 0) ? 1 : 0;

    if (is_zero)
    {
        fp_copy(out, beta);
        helioselene_secure_erase(beta, sizeof(fp_fe));
        helioselene_secure_erase(beta_sq, sizeof(fp_fe));
        helioselene_secure_erase(check, sizeof(fp_fe));
        return 0;
    }

    /* Is beta^2 == -z? */
    fp_neg(neg_z, z);
    fp_sub(check, beta_sq, neg_z);
    fp_tobytes(check_bytes, check);

    is_zero = 1;
    for (int i = 0; i < 32; i++)
        is_zero &= (check_bytes[i] == 0) ? 1 : 0;

    if (is_zero)
    {
        /* out = beta * sqrt(-1) */
        fp25_chain_mul(out, beta, SQRT_M1);
        helioselene_secure_erase(beta, sizeof(fp_fe));
        helioselene_secure_erase(beta_sq, sizeof(fp_fe));
        helioselene_secure_erase(check, sizeof(fp_fe));
        helioselene_secure_erase(neg_z, sizeof(fp_fe));
        return 0;
    }

    /* z is not a quadratic residue */
    fp_0(out);
    helioselene_secure_erase(beta, sizeof(fp_fe));
    helioselene_secure_erase(beta_sq, sizeof(fp_fe));
    helioselene_secure_erase(check, sizeof(fp_fe));
    helioselene_secure_erase(neg_z, sizeof(fp_fe));
    return -1;
}
