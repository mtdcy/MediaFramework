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


// File:    pcm_interleave.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_MODULES_PCM_INTERLEAVE_H
#define _MEDIA_MODULES_PCM_INTERLEAVE_H

#include <ABE/ABE.h>

/** NOT support in-place processing */
typedef size_t (*pcm_interleave_t)(void **input, size_t linesize/*bytes*/, int nlines, void *output);

#ifdef __cplusplus 
extern "C" {
#endif 

#define PCM_INTERLEAVE_FUNC_NAME(name) pcm_interleave_##name

#define PCM_INTERLEAVE_FUNC(name) \
size_t PCM_INTERLEAVE_FUNC_NAME(name)(void **input, size_t linesize, int nlines, void *output)

PCM_INTERLEAVE_FUNC(u8);
PCM_INTERLEAVE_FUNC(s16);
PCM_INTERLEAVE_FUNC(s24);
PCM_INTERLEAVE_FUNC(s32);
PCM_INTERLEAVE_FUNC(flt);
PCM_INTERLEAVE_FUNC(dbl);

#define PCM_INTERLEAVE2_FUNC_NAME(from, to) pcm_interleave_##from##_to_##to

#define PCM_INTERLEAVE2_FUNC(from, to) \
size_t PCM_INTERLEAVE2_FUNC_NAME(from, to)(void **input, size_t linesize, int nlines, void *output)

PCM_INTERLEAVE2_FUNC(u8, s16);
PCM_INTERLEAVE2_FUNC(s24, s16);
PCM_INTERLEAVE2_FUNC(s32, s16);
PCM_INTERLEAVE2_FUNC(flt, s16);
PCM_INTERLEAVE2_FUNC(dbl, s16);

PCM_INTERLEAVE2_FUNC(u8, s32);
PCM_INTERLEAVE2_FUNC(s16, s32);
PCM_INTERLEAVE2_FUNC(s24, s32);
PCM_INTERLEAVE2_FUNC(flt, s32);
PCM_INTERLEAVE2_FUNC(dbl, s32);

#ifdef __cplusplus 
};
#endif
#endif // _MEDIA_MODULES_PCM_INTERLEAVE_H
