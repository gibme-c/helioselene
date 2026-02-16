#ifndef HELIOSELENE_X64_FQ51_CHAIN_H
#define HELIOSELENE_X64_FQ51_CHAIN_H

#if defined(_MSC_VER)

#include "fq.h"

void fq_mul_x64(fq_fe h, const fq_fe f, const fq_fe g);
void fq_sq_x64(fq_fe h, const fq_fe f);
void fq_sqn_x64(fq_fe h, const fq_fe f, int n);

#define fq51_chain_mul fq_mul_x64
#define fq51_chain_sq fq_sq_x64
#define fq51_chain_sqn fq_sqn_x64

#else

#include "x64/fq51_inline.h"

#define fq51_chain_mul fq51_mul_inline
#define fq51_chain_sq fq51_sq_inline

static HELIOSELENE_FORCE_INLINE void fq51_sqn_inline(fq_fe h, const fq_fe f, int n)
{
    fq51_sq_inline(h, f);
    for (int i = 1; i < n; i++)
        fq51_sq_inline(h, h);
}

#define fq51_chain_sqn fq51_sqn_inline

#endif

#endif // HELIOSELENE_X64_FQ51_CHAIN_H
