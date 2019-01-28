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
#include <MediaToolkit/Toolkit.h>

__BEGIN_DECLS

#define kTimeValueBegin     (0)
#define kTimeValueInvalid   (-1)
#define kTimeValueEnd       (-2)

__END_DECLS

#ifdef __cplusplus
namespace mtdcy {
#endif
    
    /**
     * time struct for represent decoding and presentation time
     */
    struct MediaTime {
        int64_t     value;
        int64_t     timescale;

#ifdef __cplusplus
        MediaTime() : value(0), timescale(1) { }
        MediaTime(int64_t t, int64_t scale) : value(t), timescale(scale) { }
        MediaTime(int64_t us) : value(us), timescale(1000000LL) { }

        double seconds() const { return (double)value / timescale; }
        int64_t useconds() const { return (1000000LL * value) / timescale; }

        inline MediaTime scale(int64_t new_scale) const {
            if (new_scale == timescale) return *this;
            else return MediaTime((new_scale * value) / timescale, new_scale);
        }
#endif
    };

#ifdef __cplusplus

#define kTimeInvalid        MediaTime( kTimeValueInvalid )
#define kTimeBegin          MediaTime( kTimeValueBegin )
#define kTimeEnd            MediaTime( kTimeValueEnd )

    inline MediaTime operator+(const MediaTime& a, double sec) {
        return MediaTime(a.value + sec * a.timescale, a.timescale);
    }

    inline MediaTime operator+(const MediaTime& a, int64_t us) {
        if (a.timescale == 1000000LL) return MediaTime(a.value + us, a.timescale);
        return MediaTime(a.value + (a.timescale * us) / 1000000LL, a.timescale);
    }

    inline MediaTime operator+(const MediaTime& a, const MediaTime& b) {
        if (a.timescale == b.timescale) return MediaTime(a.value + b.value, a.timescale);
        return MediaTime(a.value + (a.timescale * b.value) / b.timescale, a.timescale);
    }

    inline MediaTime operator-(const MediaTime& a, double sec) {
        return MediaTime(a.value - sec * a.timescale, a.timescale);
    }

    inline MediaTime operator-(const MediaTime& a, int64_t us) {
        if (a.timescale == 1000000LL) return MediaTime(a.value - us, a.timescale);
        return MediaTime(a.value - (a.timescale * us) / 1000000LL, a.timescale);
    }

    inline MediaTime operator-(const MediaTime& a, const MediaTime& b) {
        if (a.timescale == b.timescale) return MediaTime(a.value - b.value, a.timescale);
        return MediaTime(a.value - (a.timescale * b.value) / b.timescale, a.timescale);
    }

    inline MediaTime operator*(const MediaTime& a, double mul) {
        return MediaTime(a.value * mul, a.timescale);
    }

    inline MediaTime& operator+=(MediaTime& a, const MediaTime& b) {
        if (a.timescale == b.timescale) a.value += b.value;
        else a.value += (a.timescale * b.value) / b.timescale;
        return a;
    }

    inline MediaTime& operator+=(MediaTime& a, double sec) {
        a.value += sec * a.timescale;
        return a;
    }

    inline MediaTime& operator+=(MediaTime& a, int64_t us) {
        if (a.timescale == 1000000LL) a.value += us;
        else a.value += (a.timescale * us) / 1000000LL;
        return a;
    }

    inline MediaTime& operator-=(MediaTime& a, const MediaTime& b) {
        if (a.timescale == b.timescale) a.value -= b.value;
        else a.value -= (a.timescale * b.value) / b.timescale;
        return a;
    }

    inline MediaTime& operator-=(MediaTime& a, double sec) {
        a.value -= sec * a.timescale;
        return a;
    }

    inline MediaTime& operator-=(MediaTime& a, int64_t us) {
        if (a.timescale == 1000000LL) a.value -= us;
        else a.value -= (a.timescale * us) / 1000000LL;
        return a;
    }

    inline MediaTime& operator*=(MediaTime& a, double mul) {
        a.value *= mul;
        return a;
    }

#define COMPARE(op)                                                         \
    inline bool operator op(const MediaTime& a, const MediaTime& b)         \
    { return (double)a.value/a.timescale op (double)b.value/b.timescale; }  \
    inline bool operator op(const MediaTime& a, double sec)                 \
    { return (double)a.value/a.timescale op sec; }                          \
    inline bool operator op(const MediaTime& a, int64_t us)                 \
    { return (double)a.value/a.timescale op us / 1E6; }
    
    COMPARE(<)
    COMPARE(<=)
    COMPARE(==)
    COMPARE(!=)
    COMPARE(>)
    COMPARE(>=)
    
#undef COMPARE
    
#endif

#ifdef __cplusplus
};
#endif

#endif // _MEDIA_TIME_H 
