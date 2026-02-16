#include "selene_frombytes.h"

#include "selene_constants.h"
#include "selene_validate.h"
#include "fq_ops.h"
#include "fq_mul.h"
#include "fq_sq.h"
#include "fq_sqrt.h"
#include "fq_frombytes.h"
#include "fq_tobytes.h"
#include "fq_utils.h"

/*
 * Deserialize 32 bytes to a Selene Jacobian point.
 * Same algorithm as helios_frombytes but over F_q.
 *
 * For F_q sqrt: q ≡ 3 (mod 4), so sqrt is z^((q+1)/4).
 * We need to verify the result since not all elements are QR.
 *
 * Returns 0 on success, -1 on invalid input.
 */
int selene_frombytes_x64(selene_jacobian *r, const unsigned char s[32])
{
    unsigned int y_parity = (s[31] >> 7) & 1;

    unsigned char x_bytes[32];
    for (int i = 0; i < 32; i++)
        x_bytes[i] = s[i];
    x_bytes[31] &= 0x7f;

    /* Reject non-canonical x */
    fq_fe x;
    fq_frombytes(x, x_bytes);
    unsigned char x_check[32];
    fq_tobytes(x_check, x);

    for (int i = 0; i < 32; i++)
    {
        if (x_check[i] != x_bytes[i])
            return -1;
    }

    /* Compute rhs = x^3 - 3x + b */
    fq_fe x2, x3, rhs;
    fq_sq(x2, x);
    fq_mul(x3, x2, x);

    fq_fe three_x;
    fq_add(three_x, x, x);
    fq_add(three_x, three_x, x);

    fq_sub(rhs, x3, three_x);
    fq_add(rhs, rhs, SELENE_B);

    /* Compute y = sqrt(rhs) — for q ≡ 3 mod 4, sqrt = rhs^((q+1)/4) */
    fq_fe y;
    fq_sqrt(y, rhs);

    /* Verify: y^2 == rhs */
    fq_fe y2, diff;
    fq_sq(y2, y);
    fq_sub(diff, y2, rhs);
    unsigned char diff_bytes[32];
    fq_tobytes(diff_bytes, diff);
    unsigned char d = 0;
    for (int i = 0; i < 32; i++)
        d |= diff_bytes[i];
    if (d != 0)
        return -1;

    /* Adjust y parity */
    if ((unsigned int)fq_isnegative(y) != y_parity)
        fq_neg(y, y);

    /* Return Jacobian (x, y, 1) */
    fq_copy(r->X, x);
    fq_copy(r->Y, y);
    fq_1(r->Z);

    return 0;
}
