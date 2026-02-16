#include "selene_scalarmult_vartime.h"

#include "selene.h"
#include "selene_ops.h"
#include "selene_dbl.h"
#include "selene_madd.h"
#include "selene_add.h"
#include "fq_ops.h"
#include "fq_utils.h"

/*
 * Variable-time scalar multiplication for Selene using wNAF w=5.
 * Same algorithm as helios_scalarmult_vartime but over F_q.
 */

static int wnaf_encode(int8_t naf[257], const unsigned char scalar[32])
{
    uint32_t bits[9] = {0};
    for (int i = 0; i < 32; i++)
        bits[i / 4] |= (uint32_t)scalar[i] << ((i % 4) * 8);

    int pos = 0;
    int highest = 0;

    for (int i = 0; i <= 256; i++)
        naf[i] = 0;

    while (pos <= 256)
    {
        if (!((bits[pos / 32] >> (pos % 32)) & 1))
        {
            pos++;
            continue;
        }

        int word_idx = pos / 32;
        int bit_idx = pos % 32;
        int32_t val = (int32_t)((bits[word_idx] >> bit_idx) & 0x1f);
        if (bit_idx > 27 && word_idx + 1 < 9)
            val |= (int32_t)((bits[word_idx + 1] << (32 - bit_idx)) & 0x1f);

        if (val > 16)
            val -= 32;

        naf[pos] = (int8_t)val;
        highest = pos + 1;

        {
            int wi = pos / 32;
            int bi = pos % 32;
            if (val > 0)
            {
                uint64_t sub = (uint64_t)(uint32_t)val << bi;
                uint32_t borrow = 0;
                for (int k = wi; k < 9 && (sub || borrow); k++)
                {
                    uint64_t lo = (k == wi) ? (sub & 0xffffffffULL) : ((k == wi + 1) ? (sub >> 32) : 0);
                    lo += borrow;
                    borrow = (bits[k] < lo) ? 1 : 0;
                    bits[k] -= (uint32_t)lo;
                }
            }
            else
            {
                uint64_t add = (uint64_t)(uint32_t)(-val) << bi;
                uint32_t carry = 0;
                for (int k = wi; k < 9 && (add || carry); k++)
                {
                    uint64_t lo = (k == wi) ? (add & 0xffffffffULL) : ((k == wi + 1) ? (add >> 32) : 0);
                    uint64_t sum = (uint64_t)bits[k] + lo + carry;
                    bits[k] = (uint32_t)sum;
                    carry = (uint32_t)(sum >> 32);
                }
            }
        }

        pos += 5;
    }

    return highest;
}

void selene_scalarmult_vartime_x64(selene_jacobian *r, const unsigned char scalar[32], const selene_jacobian *p)
{
    selene_jacobian table[8];
    selene_jacobian p2;

    selene_copy(&table[0], p);
    selene_dbl(&p2, p);

    for (int i = 1; i < 8; i++)
        selene_add(&table[i], &table[i - 1], &p2);

    int8_t naf[257];
    int top = wnaf_encode(naf, scalar);

    if (top == 0)
    {
        selene_identity(r);
        return;
    }

    int start = top - 1;
    while (start >= 0 && naf[start] == 0)
        start--;

    if (start < 0)
    {
        selene_identity(r);
        return;
    }

    int8_t d = naf[start];
    int idx = ((d < 0) ? -d : d) / 2;
    selene_copy(r, &table[idx]);
    if (d < 0)
        selene_neg(r, r);

    for (int i = start - 1; i >= 0; i--)
    {
        selene_dbl(r, r);

        if (naf[i] != 0)
        {
            d = naf[i];
            idx = ((d < 0) ? -d : d) / 2;
            if (d > 0)
                selene_add(r, r, &table[idx]);
            else
            {
                selene_jacobian neg_pt;
                selene_neg(&neg_pt, &table[idx]);
                selene_add(r, r, &neg_pt);
            }
        }
    }
}
