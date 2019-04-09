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

template <typename sample_type, typename coeffs_type>
static FORCE_INLINE sample_type lerp(sample_type s0, sample_type s1, coeffs_type t) {
    return s0 + t * (s1 - s0);
}

template <typename sample_type, typename coeffs_type>
struct State {
    sample_type     last;
    coeffs_type     fraction;
    coeffs_type     increment;
    
    State() : last(0) { }
    State(coeffs_type incr) : last(0), fraction(incr), increment(incr) { }
};

template <typename sample_type, typename coeffs_type>
static FORCE_INLINE size_t resample1(State<sample_type, coeffs_type>& state, const sample_type * in, size_t nsamples, sample_type * out) {
    coeffs_type inIndex = state.fraction;
    coeffs_type increment = state.increment;
    size_t outIndex = 0;
    
    // handle the first sample
    size_t x0 = (size_t)inIndex;
    while (x0 == 0) {
        out[outIndex++] = lerp(state.last, in[0], inIndex - x0);
        
        // TODO: using a advance template
        inIndex += increment;
        x0 = (size_t)inIndex;
    }
    
    while (x0 < nsamples) {
        out[outIndex++] = lerp(in[x0 - 1], in[x0], inIndex - x0);
        
        inIndex += increment;
        x0 = (size_t)inIndex;
    }
    
    state.last = in[nsamples - 1];
    state.fraction = inIndex - (size_t)inIndex;
    return outIndex;
}

// default resampler: linear interpolation
template <typename sample_type, typename coeffs_type>
struct AudioResamplerLinear : public AudioResampler {
    int32_t     mInSampleRate;
    int32_t     mOutSampleRate;
    State<sample_type, coeffs_type>     mStates[MEDIA_FRAME_NB_PLANES];
    
    AudioResamplerLinear(int32_t in, int32_t out) : AudioResampler(), mInSampleRate(in), mOutSampleRate(out) {
        State<sample_type, coeffs_type> state((coeffs_type)in / out);
        for (size_t i = 0; i < MEDIA_FRAME_NB_PLANES; ++i) {
            mStates[i] = state;
        }
    }
    
    ~AudioResamplerLinear() {
        
    }
    
    virtual Object<MediaFrame> resample(const Object<MediaFrame>& input) {
        size_t nb_samples = (input->a.samples * mOutSampleRate) / mInSampleRate + 1;
        
        AudioFormat _a  = input->a;
        _a.samples      = nb_samples;
        Object<MediaFrame> output = MediaFrameCreate(_a);
        
        for (size_t i = 0; i < input->a.channels; ++i) {
            output->a.samples = resample1<sample_type, coeffs_type>(mStates[i],
                                                                    (const sample_type *)input->planes[i].data,
                                                                    input->a.samples,
                                                                    (sample_type *)output->planes[i].data);
            output->planes[i].size = sizeof(sample_type) * output->a.samples;
        }
        
        return output;
    }
    
    virtual void reset() {
        State<sample_type, coeffs_type> state((coeffs_type)mInSampleRate / mOutSampleRate);
        for (size_t i = 0; i < MEDIA_FRAME_NB_PLANES; ++i) {
            mStates[i] = state;
        }
    }
};

Object<AudioResampler> AudioResampler::Create(const AudioFormat& in,
                                              const AudioFormat& out,
                                              const Message& options) {
    
    switch (in.format) {
        case kSampleFormatU8:
            return new AudioResamplerLinear<uint8_t, double>(in.freq, out.freq);
        case kSampleFormatS16:
            return new AudioResamplerLinear<int16_t, double>(in.freq, out.freq);
        case kSampleFormatS32:
            return new AudioResamplerLinear<int32_t, double>(in.freq, out.freq);
        case kSampleFormatFLT:
            return new AudioResamplerLinear<float, double>(in.freq, out.freq);
        case kSampleFormatDBL:
            return new AudioResamplerLinear<double, double>(in.freq, out.freq);
        default:
            FATAL("FIXME");
            return NULL;
    }
}

__END_NAMESPACE_MPX

