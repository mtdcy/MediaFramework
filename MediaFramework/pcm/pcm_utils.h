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
 * 3. Neither the name of the copyright holder nor the names of its 
 *    contributors may be used to endorse or promote products derived from 
 *    this software without specific prior written permission.
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


// File:    pcm_utils.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef MODULES_PCM_UTILS_H
#define MODULES_PCM_UTILS_H 

#include "pcm_convert.h"
#include "pcm_interleave.h"

///!!!!! api compatible code.
typedef pcm_convert_t       pcm_hook_t;
typedef pcm_interleave_t    pcm_interleave_hook_t;

#define PCM_HOOK_NAME       PCM_CONVERT_FUNC_NAME
#define PCM_HOOK_FUNC       PCM_CONVERT_FUNC
#define PCM_INTERLEAVE_HOOK_NAME        PCM_INTERLEAVE_FUNC_NAME
#define PCM_INTERLEAVE_HOOK_FUNC        PCM_INTERLEAVE_FUNC
#define PCM_INTERLEAVE2_HOOK_NAME       PCM_INTERLEAVE2_FUNC_NAME
#define PCM_INTERLEAVE2_HOOK_FUNC       PCM_INTERLEAVE2_FUNC

#endif // MODULES_PCM_UTILS_H
