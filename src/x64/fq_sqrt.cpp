#include "x64/fq_sqrt.h"

#include "helioselene_secure_erase.h"
#include "x64/fq51_chain.h"

/*
 * Compute z^((q+1)/4) mod q.
 * Since q ≡ 3 (mod 4), this gives the principal square root when z is a QR.
 *
 * (q+1)/4 = 0x1fffffffffffffffffffffffffffffffefdfde0b2dd95ad61badb49c9e49f1e8
 * This is 253 bits. Bit 252 is the MSB.
 */

/* (q+1)/4 in little-endian bytes */
static const unsigned char QP1D4[32] = {
    0xe8, 0xf1, 0x49, 0x9e, 0x9c, 0xb4, 0xad, 0x1b,
    0xd6, 0x5a, 0xd9, 0x2d, 0x0b, 0xde, 0xdf, 0xef,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f,
};

void fq_sqrt_x64(fq_fe out, const fq_fe z)
{
    fq_fe acc;

    /* acc = z (bit 252 is 1) */
    acc[0] = z[0];
    acc[1] = z[1];
    acc[2] = z[2];
    acc[3] = z[3];
    acc[4] = z[4];

    /* Scan bits 251 down to 0 */
    for (int bit = 251; bit >= 0; bit--)
    {
        fq51_chain_sq(acc, acc);
        int byte_idx = bit >> 3;
        int bit_idx = bit & 7;
        if ((QP1D4[byte_idx] >> bit_idx) & 1)
        {
            fq51_chain_mul(acc, acc, z);
        }
    }

    out[0] = acc[0];
    out[1] = acc[1];
    out[2] = acc[2];
    out[3] = acc[3];
    out[4] = acc[4];

    helioselene_secure_erase(acc, sizeof(fq_fe));
}
