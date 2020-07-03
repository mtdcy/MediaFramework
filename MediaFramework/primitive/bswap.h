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


// File:    bswap.h
// Author:  mtdcy.chen
// Changes:
//          1. 20200630     initial version
//

#ifndef _MEDIA_PRIMITIVE_BSWAP_H
#define _MEDIA_PRIMITIVE_BSWAP_H 

#include "MediaTypes.h"

__BEGIN_DECLS

// aabb -> bbaa
static FORCE_INLINE uint16_t bswap16(uint16_t x) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__("rorw $8, %w0" : "+r"(x));
    return x;
#else
    return (x >> 8) | (x << 8);
#endif
}

// aabbccdd -> ddccbbaa
static FORCE_INLINE uint32_t bswap32(uint32_t x) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__("bswap %0" : "+r"(x));
    return x;
#else
    return bswap16(x) << 16 | bswap16(x >> 16);
#endif
}

// only swap low bytes of uint32_t 
// aabbccdd -> aaddccbb
static FORCE_INLINE uint32_t bswap32l(uint32_t x) {
    return ((x >> 24) & 0xff) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | ((x << 24) & 0xff000000);
}

static FORCE_INLINE uint64_t bswap64(uint64_t x) {
#if defined(__x86_64__) || defined(__i386__)
    __asm__("bswap %0" : "=r"(x) : "0"(x));
    return x;
#else
    return (uint64_t)bswap32(x) << 32 | bswap32(x >> 32);
#endif
}

__END_DECLS

#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

__END_NAMESPACE_MPX
#endif

#endif // _MEDIA_PRIMITIVE_BSWAP_H 
