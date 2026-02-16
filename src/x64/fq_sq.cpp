#include "x64/fq_sq.h"

#include "x64/fq51.h"
#include "x64/fq51_inline.h"
#include "x64/mul128.h"

void fq_sq_x64(fq_fe h, const fq_fe f)
{
    fq51_sq_inline(h, f);
}
