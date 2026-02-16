#ifndef HELIOSELENE_FQ_UTILS_H
#define HELIOSELENE_FQ_UTILS_H

#include "fq.h"
#include "fq_tobytes.h"

/*
 * Returns 1 if h is nonzero (in canonical form), 0 if zero.
 */
static inline int fq_isnonzero(const fq_fe h)
{
    unsigned char s[32];
    fq_tobytes(s, h);
    unsigned char d = 0;
    for (int i = 0; i < 32; i++)
        d |= s[i];
    return d != 0;
}

/*
 * Returns the "sign" of h: the least significant bit of the canonical
 * representation. 0 = even (non-negative), 1 = odd (negative).
 */
static inline int fq_isnegative(const fq_fe h)
{
    unsigned char s[32];
    fq_tobytes(s, h);
    return s[0] & 1;
}

#endif // HELIOSELENE_FQ_UTILS_H
