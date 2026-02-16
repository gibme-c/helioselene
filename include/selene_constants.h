#ifndef HELIOSELENE_SELENE_CONSTANTS_H
#define HELIOSELENE_SELENE_CONSTANTS_H

#include "fq.h"

/*
 * Selene curve: y^2 = x^3 - 3x + b over F_q (q = 2^255 - gamma)
 *
 * b = 50691664119640283727448954162351551669994268339720539671652090628799494505816
 */
static const fq_fe SELENE_B = {
    0x60983cb5a4558ULL, 0x3e0d5d201cd1bULL, 0x7ff89e7ce512fULL,
    0x360bfa8ddd2caULL, 0x7012771369587ULL
};

/*
 * Selene generator G = (1, Gy)
 * Gy = 55227837453588766352929163364143300868577356225733378474337919561890377498066
 */
static const fq_fe SELENE_GX = {
    0x1ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL
};

static const fq_fe SELENE_GY = {
    0x60aa6a1d3fdd2ULL, 0x3191e1366ee83ULL, 0x572097e4e2ec6ULL,
    0x5492be498bba2ULL, 0x7a19d927b85ccULL
};

/*
 * Selene group order = p = 2^255 - 19
 * = 57896044618658097711785492504343953926634992332820282019728792003956564819949
 *
 * Stored as 32 bytes little-endian for scalar operations.
 */
static const unsigned char SELENE_ORDER[32] = {
    0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f
};

#endif // HELIOSELENE_SELENE_CONSTANTS_H
