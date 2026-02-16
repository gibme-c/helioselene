#include "x64/fp_sqrt.h"

#include "helioselene_secure_erase.h"
#include "x64/fp51.h"
#include "x64/fp51_chain.h"
#include "fp_ops.h"
#include "fp_tobytes.h"

/*
 * sqrt(-1) mod p, where p = 2^255 - 19.
 * = 2^((p-1)/4) mod p
 * = 19681161376707505956807079304988542015446066515923890162744021073123829784752
 */
static const fp_fe SQRT_M1 = {
    0x61b274a0ea0b0ULL, 0xd5a5fc8f189dULL, 0x7ef5e9cbd0c60ULL,
    0x78595a6804c9eULL, 0x2b8324804fc1dULL
};

/*
 * Atkin's square root for p ≡ 5 (mod 8).
 *
 * Algorithm:
 *   beta = z^((p+3)/8)
 *   if beta^2 == z: return beta
 *   if beta^2 == -z: return beta * sqrt(-1)
 *   else: z is not a QR, return -1
 *
 * Note: z^((p+3)/8) = z^((p-5)/8 + 1) = fp_pow22523(z) * z
 * where fp_pow22523 computes z^(2^252 - 3) = z^((p-5)/8).
 */

/* Forward declaration */
void fp_pow22523_x64(fp_fe out, const fp_fe z);

int fp_sqrt_x64(fp_fe out, const fp_fe z)
{
    fp_fe beta, beta_sq, neg_z, check;

    /* beta = z^((p+3)/8) = pow22523(z) * z */
    fp_pow22523_x64(beta, z);
    fp51_chain_mul(beta, beta, z);

    /* check = beta^2 */
    fp51_chain_sq(beta_sq, beta);

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
        fp51_chain_mul(out, beta, SQRT_M1);
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
