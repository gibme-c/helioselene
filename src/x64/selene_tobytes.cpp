#include "selene_tobytes.h"

#include "fq_ops.h"
#include "fq_invert.h"
#include "fq_mul.h"
#include "fq_sq.h"
#include "fq_tobytes.h"
#include "fq_utils.h"

/*
 * Serialize a Selene Jacobian point to 32 bytes.
 * Format: x-coordinate little-endian, y-parity in bit 255.
 *
 * For identity point, outputs all zeros.
 */
void selene_tobytes_x64(unsigned char s[32], const selene_jacobian *p)
{
    if (!fq_isnonzero(p->Z))
    {
        for (int i = 0; i < 32; i++)
            s[i] = 0;
        return;
    }

    fq_fe z_inv, z_inv2, z_inv3, x, y;

    fq_invert(z_inv, p->Z);
    fq_sq(z_inv2, z_inv);
    fq_mul(z_inv3, z_inv2, z_inv);
    fq_mul(x, p->X, z_inv2);
    fq_mul(y, p->Y, z_inv3);

    fq_tobytes(s, x);
    s[31] |= (unsigned char)(fq_isnegative(y) << 7);
}
