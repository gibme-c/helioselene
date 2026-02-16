#include "selene_scalarmult.h"

#include "selene.h"
#include "selene_ops.h"
#include "selene_dbl.h"
#include "selene_madd.h"
#include "selene_add.h"
#include "fq_ops.h"
#include "fq_invert.h"
#include "fq_mul.h"
#include "fq_sq.h"
#include "helioselene_secure_erase.h"

/*
 * Constant-time scalar multiplication for Selene (over F_q).
 * Same algorithm as helios_scalarmult but using fq_* field ops.
 */

static void scalar_recode_signed4(int8_t digits[64], const unsigned char scalar[32])
{
    uint8_t nibbles[64];
    for (int i = 0; i < 32; i++)
    {
        nibbles[2 * i] = scalar[i] & 0x0f;
        nibbles[2 * i + 1] = (scalar[i] >> 4) & 0x0f;
    }

    int carry = 0;
    for (int i = 0; i < 63; i++)
    {
        int val = nibbles[i] + carry;
        carry = 0;
        if (val > 8)
        {
            val -= 16;
            carry = 1;
        }
        digits[i] = (int8_t)val;
    }
    digits[63] = (int8_t)(nibbles[63] + carry);
}

static void batch_to_affine(selene_affine *out, const selene_jacobian *in, int n)
{
    if (n == 0) return;

    fq_fe *z_vals = new fq_fe[n];
    fq_fe *products = new fq_fe[n];

    for (int i = 0; i < n; i++)
        fq_copy(z_vals[i], in[i].Z);

    fq_copy(products[0], z_vals[0]);
    for (int i = 1; i < n; i++)
        fq_mul(products[i], products[i - 1], z_vals[i]);

    fq_fe inv;
    fq_invert(inv, products[n - 1]);

    for (int i = n - 1; i > 0; i--)
    {
        fq_fe z_inv;
        fq_mul(z_inv, inv, products[i - 1]);
        fq_mul(inv, inv, z_vals[i]);

        fq_fe z_inv2, z_inv3;
        fq_sq(z_inv2, z_inv);
        fq_mul(z_inv3, z_inv2, z_inv);
        fq_mul(out[i].x, in[i].X, z_inv2);
        fq_mul(out[i].y, in[i].Y, z_inv3);
    }

    {
        fq_fe z_inv2, z_inv3;
        fq_sq(z_inv2, inv);
        fq_mul(z_inv3, z_inv2, inv);
        fq_mul(out[0].x, in[0].X, z_inv2);
        fq_mul(out[0].y, in[0].Y, z_inv3);
    }

    helioselene_secure_erase(z_vals, n * sizeof(fq_fe));
    helioselene_secure_erase(products, n * sizeof(fq_fe));
    delete[] z_vals;
    delete[] products;
}

void selene_scalarmult_x64(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p)
{
    selene_jacobian table_jac[8];
    selene_copy(&table_jac[0], p);
    selene_dbl(&table_jac[1], p);
    selene_add(&table_jac[2], &table_jac[1], p);
    selene_dbl(&table_jac[3], &table_jac[1]);
    selene_add(&table_jac[4], &table_jac[3], p);
    selene_dbl(&table_jac[5], &table_jac[2]);
    selene_add(&table_jac[6], &table_jac[5], p);
    selene_dbl(&table_jac[7], &table_jac[3]);

    selene_affine table[8];
    batch_to_affine(table, table_jac, 8);

    int8_t digits[64];
    scalar_recode_signed4(digits, scalar);

    unsigned int abs_d = (unsigned int)((digits[63] < 0) ? -digits[63] : digits[63]);
    unsigned int neg = (digits[63] < 0) ? 1u : 0u;

    selene_affine selected;
    fq_0(selected.x);
    fq_0(selected.y);

    for (unsigned int j = 0; j < 8; j++)
    {
        unsigned int eq = ((abs_d ^ (j + 1)) - 1u) >> 31;
        selene_affine_cmov(&selected, &table[j], eq);
    }

    if (abs_d == 0)
    {
        selene_identity(r);
    }
    else
    {
        selene_affine_cneg(&selected, neg);
        selene_from_affine(r, &selected);
    }

    for (int i = 62; i >= 0; i--)
    {
        selene_dbl(r, r);
        selene_dbl(r, r);
        selene_dbl(r, r);
        selene_dbl(r, r);

        abs_d = (unsigned int)((digits[i] < 0) ? -digits[i] : digits[i]);
        neg = (digits[i] < 0) ? 1u : 0u;

        fq_1(selected.x);
        fq_1(selected.y);
        for (unsigned int j = 0; j < 8; j++)
        {
            unsigned int eq = ((abs_d ^ (j + 1)) - 1u) >> 31;
            selene_affine_cmov(&selected, &table[j], eq);
        }

        selene_affine_cneg(&selected, neg);

        unsigned int nonzero = 1u ^ ((abs_d - 1u) >> 31);
        unsigned int z_nonzero = (unsigned int)fq_isnonzero(r->Z);

        selene_jacobian tmp;
        selene_madd(&tmp, r, &selected);

        selene_jacobian fresh;
        selene_from_affine(&fresh, &selected);

        selene_cmov(r, &tmp, nonzero & z_nonzero);
        selene_cmov(r, &fresh, nonzero & (1u - z_nonzero));
    }

    helioselene_secure_erase(table_jac, sizeof(table_jac));
    helioselene_secure_erase(table, sizeof(table));
    helioselene_secure_erase(digits, sizeof(digits));
}
