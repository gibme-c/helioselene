#ifndef HELIOSELENE_HELIOS_CONSTANTS_H
#define HELIOSELENE_HELIOS_CONSTANTS_H

#include "fp.h"

/*
 * Helios curve: y^2 = x^3 - 3x + b over F_p (p = 2^255 - 19)
 *
 * b = 15789920373731020205926570676277057129217619222203920395806844808978996083412
 */
static const fp_fe HELIOS_B = {
    0x49ee1edd73ad4ULL, 0x7082277e6a456ULL, 0x2edecec10fdbcULL,
    0x5c5f4a53b59fULL, 0x22e8c739b0ea7ULL
};

/*
 * Helios generator G = (3, Gy)
 * Gy = 37760095087190773158272406437720879471285821656958791565335581949097084993268
 */
static const fp_fe HELIOS_GX = {
    0x3ULL, 0x0ULL, 0x0ULL, 0x0ULL, 0x0ULL
};

static const fp_fe HELIOS_GY = {
    0x3e639e3183ef4ULL, 0x3b8b0d4bb9a48ULL, 0x817c1d6400efULL,
    0x10e5ec93341a8ULL, 0x537b74d97ac07ULL
};

/*
 * Helios group order = q = 2^255 - gamma
 * = 57896044618658097711785492504343953926549254372227246365156541811699034343327
 *
 * Stored as 32 bytes little-endian for scalar operations.
 */
static const unsigned char HELIOS_ORDER[32] = {
    0x9f, 0xc7, 0x27, 0x79, 0x72, 0xd2, 0xb6, 0x6e,
    0x58, 0x6b, 0x65, 0xb7, 0x2c, 0x78, 0x7f, 0xbf,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f
};

#endif // HELIOSELENE_HELIOS_CONSTANTS_H
