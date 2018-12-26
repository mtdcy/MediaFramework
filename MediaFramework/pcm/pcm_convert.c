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


// File:    pcm_convert.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//
#include <MediaToolkit/Toolkit.h>

#include "primitives.h"
#include "pcm_convert.h"
#include <math.h> // lrintf

#include <string.h> // memcpy

#define PCM_HOOK(from, to, ifmt, ofmt, expr) \
    size_t PCM_CONVERT_FUNC_NAME(from, to)(void *input, size_t size, void *output) {  \
        size_t samples = size / sizeof(ifmt);                               \
        ifmt *in = (ifmt*)input;                                            \
        ofmt *out = (ofmt*)output;                                          \
        if (sizeof(ifmt) >= sizeof(ofmt)) {                                 \
            for (size_t i = 0; i < samples; i++) {                          \
                ifmt e = *in++;                                             \
                *out++ = expr;                                              \
            }                                                               \
        } else {                                                            \
            in += samples;                                                  \
            out += samples;                                                 \
            for (size_t i = 0; i < samples; i++) {                          \
                ifmt e = *--in;                                             \
                *--out = expr;                                              \
            }                                                               \
        }                                                                   \
        return samples * sizeof(ofmt);                                      \
    }

PCM_HOOK(u8,    s16,    uint8_t,    int16_t,    (((int16_t)e - 0x80) << 8));
PCM_HOOK(s32,   s16,    int32_t,    int16_t,    clamp16((e + (1LL<<15)) >> 16));
PCM_HOOK(flt,   s16,    float,      int16_t,    clamp16_from_float(e));
PCM_HOOK(dbl,   s16,    double,     int16_t,    clamp16_from_float(e));

PCM_HOOK(u8,    s32,    uint8_t,    int32_t,    (((int16_t)e - 0x80) << 24));
PCM_HOOK(s16,   s32,    int16_t,    int32_t,    (e << 16));
PCM_HOOK(flt,   s32,    float,      int32_t,    clamp32_from_float(e));
PCM_HOOK(dbl,   s32,    double,     int32_t,    clamp32_from_float(e));

///// special case
size_t PCM_CONVERT_FUNC_NAME(s24, s32)(void *input, size_t size, void *output) {
    uint8_t *in = (uint8_t*)input; 
    int32_t *out = (int32_t*)output; 
    size_t samples = size / 3;
    in += 3 * samples; 
    out += samples; 
    for (size_t i = 0; i < samples; i++) {
        in -= 3;
        int32_t e = in[0] | in[1]<<8 | in[2]<<16;
        *--out = e<<8;
    }
    return samples * sizeof(int32_t);
}

size_t PCM_CONVERT_FUNC_NAME(s24, s16)(void *input, size_t size, void *output) {
    uint8_t *in = (uint8_t*)input; 
    int16_t *out = (int16_t*)output; 
    size_t samples = size / 3;
    for (size_t i = 0; i < samples; i++) {
        *out++ = (in[0] | in[1]<<8 | in[2]<<16) >> 8;
        in += 3;
    }
    return samples * sizeof(int16_t);
}

size_t PCM_CONVERT_FUNC_NAME(s16, s16)(void *input, size_t size, void *output) {
    memcpy(output, input, size);
    return size;
}

size_t PCM_CONVERT_FUNC_NAME(s32, s32)(void *input, size_t size, void *output) {
    memcpy(output, input, size);
    return size;
}
