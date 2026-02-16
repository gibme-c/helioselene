#include "x64/fq_frombytes.h"

#include "x64/fq51.h"

void fq_frombytes_x64(fq_fe h, const unsigned char *s)
{
    uint64_t h0 = ((uint64_t)s[0]) | ((uint64_t)s[1] << 8) | ((uint64_t)s[2] << 16) | ((uint64_t)s[3] << 24)
                  | ((uint64_t)s[4] << 32) | ((uint64_t)s[5] << 40) | ((uint64_t)(s[6] & 0x07) << 48);

    uint64_t h1 = ((uint64_t)(s[6] >> 3)) | ((uint64_t)s[7] << 5) | ((uint64_t)s[8] << 13) | ((uint64_t)s[9] << 21)
                  | ((uint64_t)s[10] << 29) | ((uint64_t)s[11] << 37) | ((uint64_t)(s[12] & 0x3f) << 45);

    uint64_t h2 = ((uint64_t)(s[12] >> 6)) | ((uint64_t)s[13] << 2) | ((uint64_t)s[14] << 10) | ((uint64_t)s[15] << 18)
                  | ((uint64_t)s[16] << 26) | ((uint64_t)s[17] << 34) | ((uint64_t)s[18] << 42)
                  | ((uint64_t)(s[19] & 0x01) << 50);

    uint64_t h3 = ((uint64_t)(s[19] >> 1)) | ((uint64_t)s[20] << 7) | ((uint64_t)s[21] << 15) | ((uint64_t)s[22] << 23)
                  | ((uint64_t)s[23] << 31) | ((uint64_t)s[24] << 39) | ((uint64_t)(s[25] & 0x0f) << 47);

    uint64_t h4 = ((uint64_t)(s[25] >> 4)) | ((uint64_t)s[26] << 4) | ((uint64_t)s[27] << 12) | ((uint64_t)s[28] << 20)
                  | ((uint64_t)s[29] << 28) | ((uint64_t)s[30] << 36) | ((uint64_t)(s[31] & 0x7f) << 44);

    h[0] = h0 & FQ51_MASK;
    h[1] = h1 & FQ51_MASK;
    h[2] = h2 & FQ51_MASK;
    h[3] = h3 & FQ51_MASK;
    h[4] = h4 & FQ51_MASK;
}
