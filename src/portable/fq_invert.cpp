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

#include "helioselene_secure_erase.h"
#include "portable/fq25_chain.h"

/*
 * Compute z^(q-2) mod q via square-and-multiply.
 *
 * q-2 = 0x7fffffffffffffffffffffffffffffffbf7f782cb7656b586eb6d2727927c79d
 *
 * We scan bits from the top (bit 254) down to bit 0.
 * Bit 255 is 0 (q-2 < 2^255). Bit 254 is 1.
 */

/* q-2 in little-endian bytes */
static const unsigned char QM2[32] = {
    0x9d, 0xc7, 0x27, 0x79, 0x72, 0xd2, 0xb6, 0x6e, 0x58, 0x6b, 0x65, 0xb7, 0x2c, 0x78, 0x7f, 0xbf,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
};

void fq_invert_portable(fq_fe out, const fq_fe z)
{
    fq_fe acc;

    /* acc = z (bit 254 is 1, so start with z) */
    for (int i = 0; i < 10; i++)
        acc[i] = z[i];

    /* Scan bits 253 down to 0 */
    for (int bit = 253; bit >= 0; bit--)
    {
        fq25_chain_sq(acc, acc);
        int byte_idx = bit >> 3;
        int bit_idx = bit & 7;
        if ((QM2[byte_idx] >> bit_idx) & 1)
        {
            fq25_chain_mul(acc, acc, z);
        }
    }

    for (int i = 0; i < 10; i++)
        out[i] = acc[i];

    helioselene_secure_erase(acc, sizeof(fq_fe));
}
