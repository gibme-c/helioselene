#include "helios_tobytes.h"

#include "fp_ops.h"
#include "fp_invert.h"
#include "fp_mul.h"
#include "fp_sq.h"
#include "fp_tobytes.h"
#include "fp_utils.h"

/*
 * Serialize a Helios Jacobian point to 32 bytes.
 * Format: x-coordinate little-endian, y-parity in bit 255.
 *
 * For identity point, outputs all zeros.
 */
void helios_tobytes_x64(unsigned char s[32], const helios_jacobian *p)
{
    /* Check for identity */
    if (!fp_isnonzero(p->Z))
    {
        for (int i = 0; i < 32; i++)
            s[i] = 0;
        return;
    }

    fp_fe z_inv, z_inv2, z_inv3, x, y;

    /* x = X/Z^2, y = Y/Z^3 */
    fp_invert(z_inv, p->Z);
    fp_sq(z_inv2, z_inv);
    fp_mul(z_inv3, z_inv2, z_inv);
    fp_mul(x, p->X, z_inv2);
    fp_mul(y, p->Y, z_inv3);

    /* Serialize x-coordinate */
    fp_tobytes(s, x);

    /* Pack y-parity into bit 255 */
    s[31] |= (unsigned char)(fp_isnegative(y) << 7);
}
