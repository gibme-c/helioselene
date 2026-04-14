// Copyright (c) 2025-2026, Brandon Lehmann
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file ran_complete_add.h
 * @brief Constant-time complete addition for Ran in homogeneous projective
 *        coordinates (Renes-Costello-Batina 2016, §A.2 for a = -3).
 *
 * Produces the correct result for every (P, Q) including identity inputs,
 * P = Q, and P = -Q. No branches, no early exits, no table lookups; consumes
 * only unconditional field ops on (X, Y, Z) triples. b3 = 3 * b is precomputed
 * as RAN_B3.
 *
 * Cost per add: 12 variable fp_mul + 2 muls by RAN_B3 + 29 fp_add/sub/neg.
 */

#ifndef RANSHAW_RAN_COMPLETE_ADD_H
#define RANSHAW_RAN_COMPLETE_ADD_H

#include "ran_projective.h"

void ran_complete_add(ran_projective *r, const ran_projective *p, const ran_projective *q);

#endif // RANSHAW_RAN_COMPLETE_ADD_H
