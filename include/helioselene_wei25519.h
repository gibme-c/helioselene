#ifndef HELIOSELENE_WEI25519_H
#define HELIOSELENE_WEI25519_H

#include "fp.h"
#include "fp_frombytes.h"
#include "fp_tobytes.h"

/*
 * Wei25519 bridge: accept a raw 32-byte x-coordinate and validate it
 * as an F_p element. The caller's ed25519 library handles the
 * Ed25519 -> Wei25519 coordinate transform externally.
 *
 * This function just ingests the raw x-coordinate bytes as an F_p
 * field element (which is also a Selene scalar).
 *
 * Returns 0 on success, -1 if x >= p (non-canonical).
 */
static inline int helioselene_wei25519_to_fp(fp_fe out, const unsigned char x_bytes[32])
{
    /* Check bit 255 is clear (any valid field element has bit 255 = 0) */
    if (x_bytes[31] & 0x80)
        return -1;

    /* Deserialize */
    fp_frombytes(out, x_bytes);

    /* Reject non-canonical: re-serialize and compare */
    unsigned char check[32];
    fp_tobytes(check, out);

    unsigned char diff = 0;
    for (int i = 0; i < 32; i++)
        diff |= check[i] ^ x_bytes[i];

    if (diff != 0)
        return -1;

    return 0;
}

#endif // HELIOSELENE_WEI25519_H
