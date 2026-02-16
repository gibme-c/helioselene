#include "x64/fq_mul.h"

#include "x64/fq51.h"
#include "x64/fq51_inline.h"
#include "x64/mul128.h"

void fq_mul_x64(fq_fe h, const fq_fe f, const fq_fe g)
{
    fq51_mul_inline(h, f, g);
}
