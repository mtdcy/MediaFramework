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


// File:    MediaTime.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_TIME_H
#define _MEDIA_TIME_H
#include <MediaFramework/MediaDefs.h>

__BEGIN_DECLS

#define kTimeValueBegin     (0)
#define kTimeValueInvalid   (-1)
#define kTimeValueEnd       (-2)

__END_DECLS

__BEGIN_NAMESPACE_MPX

/**
 * time struct for represent decoding and presentation time
 */
struct API_EXPORT MediaTime {
    int64_t     value;
    int64_t     timescale;
    MediaTime() : value(0), timescale(1) { }
    MediaTime(int64_t t, int64_t scale) : value(t), timescale(scale) { }
    MediaTime(int64_t us) : value(us), timescale(1000000LL) { }

    FORCE_INLINE double seconds() const { return (double)value / timescale; }
    FORCE_INLINE int64_t useconds() const { return (1000000LL * value) / timescale; }

    FORCE_INLINE MediaTime scale(int64_t new_scale) const {
        if (new_scale == timescale) return *this;
        else return MediaTime((new_scale * value) / timescale, new_scale);
    }
    
    FORCE_INLINE MediaTime operator+(double sec) const {
        return MediaTime(value + sec * timescale, timescale);
    }
    
    FORCE_INLINE MediaTime operator+(int64_t us) const {
        if (timescale == 1000000LL) return MediaTime(value + us, timescale);
        return MediaTime(value + (timescale * us) / 1000000LL, timescale);
    }
    
    FORCE_INLINE MediaTime operator+(const MediaTime& b) const {
        if (timescale == b.timescale) return MediaTime(value + b.value, timescale);
        return MediaTime(value + (timescale * b.value) / b.timescale, timescale);
    }
    
    FORCE_INLINE MediaTime operator-(double sec) const {
        return MediaTime(value - sec * timescale, timescale);
    }
    
    FORCE_INLINE MediaTime operator-(int64_t us) const {
        if (timescale == 1000000LL) return MediaTime(value - us, timescale);
        return MediaTime(value - (timescale * us) / 1000000LL, timescale);
    }
    
    FORCE_INLINE MediaTime operator-(const MediaTime& b) const {
        if (timescale == b.timescale) return MediaTime(value - b.value, timescale);
        return MediaTime(value - (timescale * b.value) / b.timescale, timescale);
    }
    
    FORCE_INLINE MediaTime operator*(double mul) {
        return MediaTime(value * mul, timescale);
    }
    
    FORCE_INLINE MediaTime& operator+=(const MediaTime& b) {
        if (timescale == b.timescale) value += b.value;
        else value += (timescale * b.value) / b.timescale;
        return *this;
    }
    
    FORCE_INLINE MediaTime& operator+=(double sec) {
        value += sec * timescale;
        return *this;
    }
    
    FORCE_INLINE MediaTime& operator+=(int64_t us) {
        if (timescale == 1000000LL) value += us;
        else value += (timescale * us) / 1000000LL;
        return *this;
    }
    
    FORCE_INLINE MediaTime& operator-=(const MediaTime& b) {
        if (timescale == b.timescale) value -= b.value;
        else value -= (timescale * b.value) / b.timescale;
        return *this;
    }
    
    FORCE_INLINE MediaTime& operator-=(double sec) {
        value -= sec * timescale;
        return *this;
    }
    
    FORCE_INLINE MediaTime& operator-=(int64_t us) {
        if (timescale == 1000000LL) value -= us;
        else value -= (timescale * us) / 1000000LL;
        return *this;
    }
    
    FORCE_INLINE MediaTime& operator*=(double mul) {
        value *= mul;
        return *this;
    }
    
#define COMPARE(op)                                                     \
    FORCE_INLINE bool operator op(const MediaTime& b) const             \
    { return (double)value/timescale op (double)b.value/b.timescale; }  \
    FORCE_INLINE bool operator op(double sec) const                     \
    { return (double)value/timescale op sec; }                          \
    FORCE_INLINE bool operator op(int64_t us) const                     \
    { return (double)value/timescale op us / 1E6; }
    
    COMPARE(<)
    COMPARE(<=)
    COMPARE(==)
    COMPARE(!=)
    COMPARE(>)
    COMPARE(>=)
    
#undef COMPARE
};

#define kTimeInvalid        MediaTime( kTimeValueInvalid )
#define kTimeBegin          MediaTime( kTimeValueBegin )
#define kTimeEnd            MediaTime( kTimeValueEnd )

__END_NAMESPACE_MPX

#endif // _MEDIA_TIME_H 
