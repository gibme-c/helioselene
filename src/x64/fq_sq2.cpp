#include "x64/fq_sq2.h"

#include "x64/fq51_inline.h"

void fq_sq2_x64(fq_fe h, const fq_fe f)
{
    fq51_sq_inline(h, f);
    fq_fe t;
    t[0] = h[0];
    t[1] = h[1];
    t[2] = h[2];
    t[3] = h[3];
    t[4] = h[4];
    h[0] = t[0] + t[0];
    h[1] = t[1] + t[1];
    h[2] = t[2] + t[2];
    h[3] = t[3] + t[3];
    h[4] = t[4] + t[4];
}
