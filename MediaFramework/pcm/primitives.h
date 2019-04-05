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


// File:    primitives.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//


#ifndef PCM_PRIMITIVES_H
#define PCM_PRIMITIVES_H 

#include "MediaDefs.h"

__BEGIN_DECLS

// these code are borrowed from android
// system/media/audio_utils/include/audio_utils/primitives.h

/**
 * Clamp (aka hard limit or clip) a signed 32-bit sample to 16-bit range.
 */
static FORCE_INLINE int16_t clamp16(int32_t sample)
{
    if ((sample>>15) ^ (sample>>31))
        sample = 0x7FFF ^ (sample>>31);
    return sample;
}


static FORCE_INLINE int32_t clamp32(int64_t v) {
    if ((v>>31) ^ (v>>63))
        v = 0x7fffffff ^ (v>>63);
    return v;
}

/*
 * Convert a IEEE 754 single precision float [-1.0, 1.0) to int16_t [-32768, 32767]
 * with clamping.  Note the open bound at 1.0, values within 1/65536 of 1.0 map
 * to 32767 instead of 32768 (early clamping due to the smaller positive integer subrange).
 *
 * Values outside the range [-1.0, 1.0) are properly clamped to -32768 and 32767,
 * including -Inf and +Inf. NaN will generally be treated either as -32768 or 32767,
 * depending on the sign bit inside NaN (whose representation is not unique).
 * Nevertheless, strictly speaking, NaN behavior should be considered undefined.
 *
 * Rounding of 0.5 lsb is to even (default for IEEE 754).
 */
static FORCE_INLINE int16_t clamp16_from_float(float f)
{
    /* Offset is used to resize the valid range of [-1.0, 1.0) into the 16 lsbs of the
     * floating point significand. The normal shift is 3<<22, but the -15 offset
     * is used to multiply by 32768.
     */
    static const float offset = (float)(3 << (22 - 15));
    /* zero = (0x10f << 22) =  0x43c00000 (not directly used) */
    static const int32_t limneg = (0x10f << 22) /*zero*/ - 32768; /* 0x43bf8000 */
    static const int32_t limpos = (0x10f << 22) /*zero*/ + 32767; /* 0x43c07fff */

    union {
        float f;
        int32_t i;
    } u;

    u.f = f + offset; /* recenter valid range */
    /* Now the valid range is represented as integers between [limneg, limpos].
     * Clamp using the fact that float representation (as an integer) is an ordered set.
     */
    if (u.i < limneg)
        u.i = -32768;
    else if (u.i > limpos)
        u.i = 32767;
    return u.i; /* Return lower 16 bits, the part of interest in the significand. */
}

/* Convert a single-precision floating point value to a Q0.31 integer value.
 * Rounds to nearest, ties away from 0.
 *
 * Values outside the range [-1.0, 1.0) are properly clamped to -2147483648 and 2147483647,
 * including -Inf and +Inf. NaN values are considered undefined, and behavior may change
 * depending on hardware and future implementation of this function.
 */
static FORCE_INLINE int32_t clamp32_from_float(float f)
{
    static const float scale = (float)(1UL << 31);
    static const float limpos = 1.;
    static const float limneg = -1.;

    if (f <= limneg) {
        return -0x80000000; /* or 0x80000000 */
    } else if (f >= limpos) {
        return 0x7fffffff;
    }
    f *= scale;
    /* integer conversion is through truncation (though int to float is not).
     * ensure that we round to nearest, ties away from 0.
     */
    return f > 0 ? f + 0.5 : f - 0.5;
}

__END_DECLS

#endif // PCM_PRIMITIVES_H
