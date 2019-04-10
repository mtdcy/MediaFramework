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


/**
 * File:    AudioResampler.cpp
 * Author:  mtdcy.chen
 * Changes:
 *          1. 20181214     initial version
 *
 */


#define LOG_TAG "AudioResampler"

#include "AudioResampler.h"

__BEGIN_NAMESPACE_MPX

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

// TO (*value_convert)(FROM from)
template <typename FROM, typename TO> static FORCE_INLINE TO expr(const FROM v);

// -> int16_t
template <> int16_t expr(const uint8_t v)   { return (((int16_t)v - 0x80) << 8);    }
template <> int16_t expr(const int16_t v)   { return v;                             }
template <> int16_t expr(const int32_t v)   { return clamp16((v + (1<<15)) >> 16);  }
template <> int16_t expr(const float v)     { return clamp16_from_float(v);         }
template <> int16_t expr(const double v)    { return clamp16_from_float(v);         }

// -> int32_t
template <> int32_t expr(const uint8_t v)   { return (((int16_t)v - 0x80) << 24);   }
template <> int32_t expr(const int16_t v)   { return (v << 16);                     }
template <> int32_t expr(const int32_t v)   { return v;                             }
template <> int32_t expr(const float v)     { return clamp32_from_float(v);         }
template <> int32_t expr(const double v)    { return clamp32_from_float(v);         }

template <typename TYPE, typename COEFFS_TYPE> static FORCE_INLINE TYPE lerp(const TYPE s0, const TYPE s1, COEFFS_TYPE t);

// https://en.wikipedia.org/wiki/Linear_interpolation
template <> int16_t lerp(const int16_t s0, const int16_t s1, double t)  { return (1 - t) * s0 + t * s1;     }
template <> int32_t lerp(const int32_t s0, const int32_t s1, double t)  { return (1 - t) * s0 + t * s1;     }

template <typename TYPE, typename COEFFS_TYPE> struct State {
    TYPE        last;
    COEFFS_TYPE fraction;
    COEFFS_TYPE increment;
    
    State() : last(0) { }
    State(COEFFS_TYPE incr) : last(0), fraction(incr), increment(incr) { }
};

// size_t (*resample)(State&, const FROM *, size_t, TO *)
template <typename FROM, typename TO, typename COEFFS_TYPE> struct resample1;

// using partial specilization
// FROM == TO
template <typename TYPE> struct resample1<TYPE, TYPE, double> {
    size_t operator()(State<TYPE, double>& state, const TYPE * in, size_t nsamples, TYPE * out) {
        double inIndex = state.fraction;
        double increment = state.increment;
        size_t outIndex = 0;
        
        // handle the first sample
        size_t x0 = (size_t)inIndex;
        while (x0 == 0) {
            out[outIndex++] = lerp<TYPE, double>(state.last, in[0], inIndex - x0);
            
            // TODO: using a advance template
            inIndex += increment;
            x0 = (size_t)inIndex;
        }
        
        while (x0 < nsamples) {
            out[outIndex++] = lerp<TYPE, double>(in[x0 - 1], in[x0], inIndex - x0);
            
            inIndex += increment;
            x0 = (size_t)inIndex;
        }
        
        state.last = in[nsamples - 1];
        state.fraction = inIndex - (size_t)inIndex;
        return outIndex;
    }
};

template <typename FROM, typename TO> struct resample1<FROM, TO, double> {
    size_t operator()(State<TO, double>& state, const FROM * in, size_t nsamples, TO * out) {
        double inIndex = state.fraction;
        double increment = state.increment;
        size_t outIndex = 0;
        
        // handle the first sample
        size_t x0 = (size_t)inIndex;
        while (x0 == 0) {
            out[outIndex++] = lerp<TO, double>(state.last,
                                               expr<FROM, TO>(in[0]),
                                               inIndex - x0);
            
            // TODO: using a advance template
            inIndex += increment;
            x0 = (size_t)inIndex;
        }
        
        while (x0 < nsamples) {
            out[outIndex++] = lerp<TO, double>(expr<FROM, TO>(in[x0 - 1]),
                                               expr<FROM, TO>(in[x0]),
                                               inIndex - x0);
            
            inIndex += increment;
            x0 = (size_t)inIndex;
        }
        
        state.last = expr<FROM, TO>(in[nsamples - 1]);
        state.fraction = inIndex - (size_t)inIndex;
        return outIndex;
    }
};

// default resampler: linear interpolation
template <typename FROM, typename TO, typename COEFFS_TYPE>
struct AudioResamplerLinear : public AudioResampler {
    AudioFormat     mInput;
    AudioFormat     mOutput;
    State<TO, COEFFS_TYPE>     mStates[MEDIA_FRAME_NB_PLANES];
    
    AudioResamplerLinear(const AudioFormat& input, const AudioFormat& output) : AudioResampler(),
    mInput(input), mOutput(output) {
        reset();
    }
    
    virtual void reset() {
        State<TO, COEFFS_TYPE> state((COEFFS_TYPE)mInput.freq / mOutput.freq);
        for (size_t i = 0; i < MEDIA_FRAME_NB_PLANES; ++i) {
            mStates[i] = state;
        }
    }
    
    virtual Object<MediaFrame> resample(const Object<MediaFrame>& input) {
        size_t nb_samples = (input->a.samples * mOutput.freq) / mInput.freq + 1;
        AudioFormat format = mOutput;
        format.samples = nb_samples;
        
        Object<MediaFrame> output = MediaFrameCreate(format);
        
        for (size_t i = 0; i < input->a.channels; ++i) {
            output->a.samples = resample1<FROM, TO, COEFFS_TYPE>()(mStates[i],
                                                                 (const FROM *)input->planes[i].data,
                                                                 input->a.samples,
                                                                 (TO *)output->planes[i].data);
            output->planes[i].size = sizeof(TO) * output->a.samples;
        }
        
        return output;
    }
};

Object<AudioResampler> AudioResampler::Create(const AudioFormat& in,
                                              const AudioFormat& out,
                                              const Message& options) {
    INFO("init AudioResampler %s -> %s", GetAudioFormatString(in).c_str(), GetAudioFormatString(out).c_str());
    
    switch (out.format) {
        case kSampleFormatS16:
            switch (in.format) {
                case kSampleFormatU8:
                    return new AudioResamplerLinear<uint8_t, int16_t, double>(in, out);
                case kSampleFormatS16:
                    return new AudioResamplerLinear<int16_t, int16_t, double>(in, out);
                case kSampleFormatS32:
                    return new AudioResamplerLinear<int32_t, int16_t, double>(in, out);
                case kSampleFormatFLT:
                    return new AudioResamplerLinear<float, int16_t, double>(in, out);
                case kSampleFormatDBL:
                    return new AudioResamplerLinear<double, int16_t, double>(in, out);
                default:
                    break;
            }
            break;
        case kSampleFormatS32:
            switch (in.format) {
                case kSampleFormatU8:
                    return new AudioResamplerLinear<uint8_t, int32_t, double>(in, out);
                case kSampleFormatS16:
                    return new AudioResamplerLinear<int16_t, int32_t, double>(in, out);
                case kSampleFormatS32:
                    return new AudioResamplerLinear<int32_t, int32_t, double>(in, out);
                case kSampleFormatFLT:
                    return new AudioResamplerLinear<float, int32_t, double>(in, out);
                case kSampleFormatDBL:
                    return new AudioResamplerLinear<double, int32_t, double>(in, out);
                default:
                    break;
            }
            break;
        default:
            break;
    }
    ERROR("only support s16/s32 output");
    return NULL;
}

__END_NAMESPACE_MPX



