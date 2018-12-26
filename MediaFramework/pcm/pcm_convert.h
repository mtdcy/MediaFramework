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


// File:    pcm_convert.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_MODULES_PCM_CONVERT_H
#define _MEDIA_MODULES_PCM_CONVERT_H 

#include <MediaToolkit/tkTypes.h>

typedef size_t (*pcm_convert_t)(void *input, size_t size /*bytes*/, void *output);

#ifdef __cplusplus 
extern "C" {
#endif 

#define PCM_CONVERT_FUNC_NAME(from, to) pcm_##from##_to_##to

#define PCM_CONVERT_FUNC(from, to) \
size_t PCM_CONVERT_FUNC_NAME(from, to)(void *input, size_t size, void *output)

PCM_CONVERT_FUNC(u8, s16);
PCM_CONVERT_FUNC(s16, s16);
PCM_CONVERT_FUNC(s24, s16);
PCM_CONVERT_FUNC(s32, s16);
PCM_CONVERT_FUNC(flt, s16);
PCM_CONVERT_FUNC(dbl, s16);

PCM_CONVERT_FUNC(u8, s32);
PCM_CONVERT_FUNC(s16, s32);
PCM_CONVERT_FUNC(s24, s32);
PCM_CONVERT_FUNC(s32, s32);
PCM_CONVERT_FUNC(flt, s32);
PCM_CONVERT_FUNC(dbl, s32);

#ifdef __cplusplus 
};
#endif

#endif  // _MEDIA_MODULES_PCM_CONVERT_H
