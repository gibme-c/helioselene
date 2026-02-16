#include "x64/fq_invert.h"

#include "helioselene_secure_erase.h"
#include "x64/fq51_chain.h"

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
    0x9d, 0xc7, 0x27, 0x79, 0x72, 0xd2, 0xb6, 0x6e,
    0x58, 0x6b, 0x65, 0xb7, 0x2c, 0x78, 0x7f, 0xbf,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f,
};

void fq_invert_x64(fq_fe out, const fq_fe z)
{
    fq_fe acc;

    /* acc = z (bit 254 is 1, so start with z) */
    acc[0] = z[0];
    acc[1] = z[1];
    acc[2] = z[2];
    acc[3] = z[3];
    acc[4] = z[4];

    /* Scan bits 253 down to 0 */
    for (int bit = 253; bit >= 0; bit--)
    {
        fq51_chain_sq(acc, acc);
        int byte_idx = bit >> 3;
        int bit_idx = bit & 7;
        if ((QM2[byte_idx] >> bit_idx) & 1)
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
