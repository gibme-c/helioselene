#include "x64/fq_sqn.h"

#include "x64/fq51_inline.h"

void fq_sqn_x64(fq_fe h, const fq_fe f, int n)
{
    fq51_sq_inline(h, f);
    for (int i = 1; i < n; i++)
        fq51_sq_inline(h, h);
}
