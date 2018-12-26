/******************************************************************************
 * Copyright (c) 2016, Chen Fang <mtdcy.chen@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 * 
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 *  POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/


// File:    pcm_interleave.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#include "primitives.h"
#include "pcm_interleave.h"

#include <math.h>

// do pcm interleave.

#define PCM_HOOK(name, format)                                                           \
size_t PCM_INTERLEAVE_FUNC_NAME(name)(void **input, size_t linesize, int nlines, void *output) {    \
    format**  in  = (format**)input;                                                                \
    format*   out = (format*)output;                                                                \
    size_t samples = linesize / sizeof(format);                                                     \
    for (int i = 0; i < samples; i++) {                                                             \
        for (int j = 0; j < nlines; j++) {                                                          \
            *out++  = in[j][i];                                                                     \
        }                                                                                           \
    }                                                                                               \
    return linesize * nlines;                                                                       \
}

PCM_HOOK(u8, uint8_t);
PCM_HOOK(s16, int16_t);
PCM_HOOK(s32, int32_t);
PCM_HOOK(flt, float);
PCM_HOOK(dbl, double);

// special case 

size_t PCM_INTERLEAVE_FUNC_NAME(s24)(void **input, size_t linesize, int nlines, void *output) {
    uint8_t** in    = (uint8_t**)input;
    uint8_t* out    = (uint8_t*)output;
    for (int i = 0; i < linesize; i += 3) {
        for (int j = 0; j < nlines; j++) {
            *out++  = in[j][i];
            *out++  = in[j][i+1];
            *out++  = in[j][i+2];
        }
    }
    return linesize * nlines;
}

// do pcm interleave and convert 
#define PCM_HOOK2(from, to, ifmt, ofmt, expr)                                                           \
size_t PCM_INTERLEAVE2_FUNC_NAME(from, to)(void **input, size_t linesize, int nlines, void *output) {   \
    ifmt**  in      = (ifmt**)input;                                                                    \
    ofmt*  out     = (ofmt*)output;                                                                     \
    size_t samples  = linesize / sizeof(ifmt);                                                          \
    for (int i = 0; i < samples; i++) {                                                                 \
        for (int j = 0; j < nlines; j++) {                                                              \
            ifmt e  = in[j][i];                                                                         \
            *out++  = expr;                                                                             \
        }                                                                                               \
    }                                                                                                   \
    return samples * nlines * sizeof(ofmt);                                                             \
}

PCM_HOOK2(u8,   s16,    uint8_t,    int16_t,    (((int16_t)e - 0x80) << 8));
PCM_HOOK2(s32,  s16,    int32_t,    int16_t,    clamp16((e + (1LL<<15)) >> 16));
PCM_HOOK2(flt,  s16,    float,      int16_t,    clamp16_from_float(e));
PCM_HOOK2(dbl,  s16,    double,     int16_t,    clamp16_from_float(e));

PCM_HOOK2(u8,   s32,    uint8_t,    int32_t,    (((int32_t)e - 0x80) << 24));
PCM_HOOK2(s16,  s32,    int16_t,    int32_t,    (e << 16));
PCM_HOOK2(flt,  s32,    float,      int32_t,    clamp32_from_float(e));
PCM_HOOK2(dbl,  s32,    double,     int32_t,    clamp32_from_float(e));

// special case 
size_t PCM_INTERLEAVE2_FUNC_NAME(s24, s16)(void **input, size_t linesize, int nlines, void *output) { 
    uint8_t** in    = (uint8_t**)input;
    int16_t* out    = (int16_t*)output;
    size_t samples  = linesize / 3;
    for (int i = 0; i < samples; i++) {
        for (int j = 0; j < nlines; j++) { 
            *out++  = in[j][i] | ((in[j][i+1]) << 8);
        }
    }
    return samples * nlines * sizeof(int16_t);
}

