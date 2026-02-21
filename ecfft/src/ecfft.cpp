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

#ifdef HELIOSELENE_ECFFT

#include "ecfft_fp.h"
#include "ecfft_fq.h"

static ecfft_fp_ctx g_ecfft_fp_ctx = {};
static ecfft_fq_ctx g_ecfft_fq_ctx = {};

void ecfft_fp_global_init()
{
    if (!g_ecfft_fp_ctx.initialized)
        ecfft_fp_init(&g_ecfft_fp_ctx);
}

void ecfft_fp_global_free()
{
    ecfft_fp_free(&g_ecfft_fp_ctx);
}

const ecfft_fp_ctx *ecfft_fp_global_ctx()
{
    return g_ecfft_fp_ctx.initialized ? &g_ecfft_fp_ctx : nullptr;
}

void ecfft_fq_global_init()
{
    if (!g_ecfft_fq_ctx.initialized)
        ecfft_fq_init(&g_ecfft_fq_ctx);
}

void ecfft_fq_global_free()
{
    ecfft_fq_free(&g_ecfft_fq_ctx);
}

const ecfft_fq_ctx *ecfft_fq_global_ctx()
{
    return g_ecfft_fq_ctx.initialized ? &g_ecfft_fq_ctx : nullptr;
}

#endif /* HELIOSELENE_ECFFT */
