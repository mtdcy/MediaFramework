/******************************************************************************
 * Copyright (c) 2020, Chen Fang <mtdcy.chen@gmail.com>
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
 * File:    AudioConverter.cpp
 * Author:  mtdcy.chen
 * Changes:
 *          1. 20181214     initial version
 *
 */


#define LOG_TAG "AudioConverter"

#include "AudioConverter.h"
#include "MediaUnit.h"
#include "primitive/clamp.h"

__BEGIN_DECLS

static const SampleDescriptor kSampleU8 = {
    .name       = "u8p",
    .format     = kSampleFormatU8,
    .similar    = kSampleFormatU8Packed,
    .planar     = True,
    .bytes      = 1,
};

static const SampleDescriptor kSampleS16 = {
    .name       = "s16p",
    .format     = kSampleFormatS16,
    .similar    = kSampleFormatS16Packed,
    .planar     = True,
    .bytes      = 2,
};

static const SampleDescriptor kSampleS32 = {
    .name       = "s32p",
    .format     = kSampleFormatS32,
    .similar    = kSampleFormatS32Packed,
    .planar     = True,
    .bytes      = 4,
};

static const SampleDescriptor kSampleF32 = {
    .name       = "f32p",
    .format     = kSampleFormatF32,
    .similar    = kSampleFormatF32Packed,
    .planar     = True,
    .bytes      = sizeof(Float32),
};

static const SampleDescriptor kSampleF64 = {
    .name       = "f64p",
    .format     = kSampleFormatF64,
    .similar    = kSampleFormatF64Packed,
    .planar     = True,
    .bytes      = sizeof(Float64),
};

static const SampleDescriptor kSampleU8Packed = {
    .name       = "u8i",
    .format     = kSampleFormatU8Packed,
    .similar    = kSampleFormatU8,
    .planar     = False,
    .bytes      = 1,
};

static const SampleDescriptor kSampleS16Packed = {
    .name       = "s16i",
    .format     = kSampleFormatS16Packed,
    .similar    = kSampleFormatS16,
    .planar     = False,
    .bytes      = 2,
};

static const SampleDescriptor kSampleS32Packed = {
    .name       = "s32i",
    .format     = kSampleFormatS32Packed,
    .similar    = kSampleFormatS32,
    .planar     = False,
    .bytes      = 4,
};

static const SampleDescriptor kSampleF32Packed = {
    .name       = "f32i",
    .format     = kSampleFormatF32Packed,
    .similar    = kSampleFormatF32,
    .planar     = False,
    .bytes      = sizeof(Float32),
};

static const SampleDescriptor kSampleF64Packed = {
    .name       = "f64i",
    .format     = kSampleFormatF64Packed,
    .similar    = kSampleFormatF64,
    .planar     = False,
    .bytes      = sizeof(Float64),
};

static const SampleDescriptor * kSampleDiscriptors[] = {
    &kSampleU8,
    &kSampleS16,
    &kSampleS32,
    &kSampleF32,
    &kSampleF64,
    &kSampleU8Packed,
    &kSampleS16Packed,
    &kSampleS32Packed,
    &kSampleF32Packed,
    &kSampleF64Packed,
    Nil
};

const SampleDescriptor * GetSampleFormatDescriptor(eSampleFormat sample) {
    for (UInt32 i = 0; kSampleDiscriptors[i] != Nil; ++i) {
        if (kSampleDiscriptors[i]->format == sample) {
            return kSampleDiscriptors[i];
        }
    }
    return Nil;
}

eSampleFormat GetSimilarSampleFormat(eSampleFormat sample) {
    const SampleDescriptor * desc = GetSampleFormatDescriptor(sample);
    CHECK_NULL(desc);
    return desc->similar;
}

UInt32 GetSampleFormatBytes(eSampleFormat sample) {
    const SampleDescriptor * desc = GetSampleFormatDescriptor(sample);
    CHECK_NULL(desc);
    return desc->bytes;
}

Bool IsPlanarSampleFormat(eSampleFormat sample) {
    const SampleDescriptor * desc = GetSampleFormatDescriptor(sample);
    CHECK_NULL(desc);
    return desc->planar;
}

static const eSampleFormat kPlanarSamples[] = {
    kSampleFormatU8,
    kSampleFormatS16,
    kSampleFormatS32,
    kSampleFormatF32,
    kSampleFormatF64,
    kSampleFormatUnknown,
};

static const eSampleFormat kPackedSamples[] = {
    kSampleFormatU8Packed,
    kSampleFormatS16Packed,
    kSampleFormatS32Packed,
    kSampleFormatF32Packed,
    kSampleFormatF64Packed,
    kSampleFormatUnknown,
};

__END_DECLS

__BEGIN_NAMESPACE_MFWK

// TO (*convert)(FROM from)
template <typename FROM, typename TO> struct expr;

// TYPE -> TYPE
template <typename TYPE> struct expr<TYPE, TYPE> {
    FORCE_INLINE TYPE operator()(const TYPE v)          { return v;                             }
};

// -> Int16
template <> struct expr<UInt8, Int16> {
    FORCE_INLINE Int16 operator()(const UInt8 v)    { return (((Int16)v - 0x80) << 8);    }
};
template <> struct expr<Int32, Int16> {
    FORCE_INLINE Int16 operator()(const Int32 v)    { return clamp16((v + (1<<15)) >> 16);  }
};
template <> struct expr<Float32, Int16> {
    FORCE_INLINE Int16 operator()(const Float32 v)      { return clamp16_from_float(v);         }
};
template <> struct expr<Float64, Int16> {
    FORCE_INLINE Int16 operator()(const Float64 v)     { return clamp16_from_float(v);         }
};

// -> Int32
template <> struct expr<UInt8, Int32> {
    FORCE_INLINE Int32 operator()(const UInt8 v)    { return (((Int16)v - 0x80) << 24);   }
};
template <> struct expr<Int16, Int32> {
    FORCE_INLINE Int32 operator()(const Int16 v)    { return (v << 16);                     }
};
template <> struct expr<Float32, Int32> {
    FORCE_INLINE Int32 operator()(const Float32 v)      { return clamp32_from_float(v);         }
};
template <> struct expr<Float64, Int32> {
    FORCE_INLINE Int32 operator()(const Float64 v)     { return clamp32_from_float(v);         }
};

struct DownmixContext : public SharedObject {
    AudioFormat                 iFormat;
    AudioFormat                 oFormat;
    const SampleDescriptor *    iDesc;
    const SampleDescriptor *    oDesc;
};

static MediaUnitContext downmix_alloc() {
    sp<DownmixContext> downmix = new DownmixContext;
    return downmix->RetainObject();
}

static void downmix_dealloc(MediaUnitContext ref) {
    sp<DownmixContext> downmix = static_cast<DownmixContext *>(ref);
    downmix->ReleaseObject();
}

static MediaError downmix_init(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    sp<DownmixContext> downmix = static_cast<DownmixContext *>(ref);
    if (iformat->audio.channels == 0 || iformat->audio.channels == oformat->audio.channels) {
        ERROR("bad parameters: %s -> %s", GetAudioFormatString(iformat->audio).c_str(),
              GetAudioFormatString(oformat->audio).c_str());
        return kMediaErrorBadParameters;
    }
    if (oformat->audio.channels != 2) {
        ERROR("donwmix only support -> stereo now");
        return kMediaErrorBadParameters;
    }
    
    downmix->iFormat    = iformat->audio;
    downmix->oFormat    = oformat->audio;
    downmix->iDesc      = GetSampleFormatDescriptor(iformat->format);
    downmix->oDesc      = GetSampleFormatDescriptor(oformat->format);
    if (downmix->iDesc->planar == False || downmix->oDesc->planar == False) {
        ERROR("downmix only support planar samples");
        return kMediaErrorBadParameters;
    }
    return kMediaNoError;
}

// downmix to stereo, reference:
// 1. https://trac.ffmpeg.org/wiki/AudioChannelManipulation#a5.1stereo
// 2.
template <typename FROM, typename TO>
static MediaError downmix_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    sp<DownmixContext> downmix = static_cast<DownmixContext *>(ref);
    if (input->count != downmix->iFormat.channels || output->count != downmix->oFormat.channels) {
        ERROR("bad MediaBufferList");
        return kMediaErrorBadParameters;
    }
    FROM * iPlanes[8];
    TO * oPlanes[2];
    
    for (UInt32 i = 0; i < input->count; ++i) {
        iPlanes[i] = (FROM *)input->buffers[i].data;
    }
    for (UInt32 i = 0; i < output->count; ++i) {
        oPlanes[i] = (TO *)output->buffers[i].data;
    }

    if (downmix->iFormat.channels >= 6) {
        for (UInt32 i = 0; i < input->buffers[0].size / sizeof(FROM); ++i) {
            oPlanes[0][i] = expr<FROM, TO>()(iPlanes[0][i] + 0.707 * iPlanes[2][i] + 0.707 + iPlanes[4][i] + iPlanes[3][i]);
            oPlanes[1][i] = expr<FROM, TO>()(iPlanes[i][i] + 0.707 * iPlanes[2][i] + 0.707 + iPlanes[5][i] + iPlanes[3][i]);
        }
    } else {
        FATAL("FIXME");
    }
    return kMediaNoError;
}

template <typename TYPE, typename COEFFS_TYPE> struct lerp;

// https://en.wikipedia.org/wiki/Linear_interpolation
template <typename TYPE> struct lerp<TYPE, Float64> {
    FORCE_INLINE TYPE operator()(const TYPE s0, const TYPE s1, Float64 t)  { return (1 - t) * s0 + t * s1;     }
};

template <typename TYPE, typename COEFFS_TYPE> struct State {
    TYPE        last;
    COEFFS_TYPE fraction;
    COEFFS_TYPE increment;

    State() : last(0) { }
    State(COEFFS_TYPE incr) : last(0), fraction(incr), increment(incr) { }
};

// UInt32 (*resample)(State&, const FROM *, UInt32, TO *)
template <typename FROM, typename TO, typename COEFFS_TYPE> struct resample1;

// using partial specilization
// FROM == TO
template <typename TYPE> struct resample1<TYPE, TYPE, Float64> {
    UInt32 operator()(State<TYPE, Float64>& state, const TYPE * in, UInt32 nsamples, TYPE * out) {
        Float64 inIndex = state.fraction;
        Float64 increment = state.increment;
        UInt32 outIndex = 0;

        // handle the first sample
        UInt32 x0 = (UInt32)inIndex;
        while (x0 == 0) {
            out[outIndex++] = lerp<TYPE, Float64>()(state.last, in[0], inIndex - x0);

            // TODO: using a advance template
            inIndex += increment;
            x0 = (UInt32)inIndex;
        }

        while (x0 < nsamples) {
            out[outIndex++] = lerp<TYPE, Float64>()(in[x0 - 1], in[x0], inIndex - x0);

            inIndex += increment;
            x0 = (UInt32)inIndex;
        }

        state.last = in[nsamples - 1];
        state.fraction = inIndex - (UInt32)inIndex;
        return outIndex;
    }
};

template <typename FROM, typename TO> struct resample1<FROM, TO, Float64> {
    UInt32 operator()(State<TO, Float64>& state, const FROM * in, UInt32 nsamples, TO * out) {
        Float64 inIndex = state.fraction;
        Float64 increment = state.increment;
        UInt32 outIndex = 0;

        // handle the first sample
        UInt32 x0 = (UInt32)inIndex;
        while (x0 == 0) {
            out[outIndex++] = lerp<TO, Float64>()(state.last,
                    expr<FROM, TO>()(in[0]),
                    inIndex - x0);

            // TODO: using a advance template
            inIndex += increment;
            x0 = (UInt32)inIndex;
        }

        while (x0 < nsamples) {
            out[outIndex++] = lerp<TO, Float64>()(expr<FROM, TO>()(in[x0 - 1]),
                    expr<FROM, TO>()(in[x0]),
                    inIndex - x0);

            inIndex += increment;
            x0 = (UInt32)inIndex;
        }

        state.last = expr<FROM, TO>()(in[nsamples - 1]);
        state.fraction = inIndex - (UInt32)inIndex;
        return outIndex;
    }
};

#define NB_CHANNELS     (8)
// default resampler: linear interpolation
template <typename TYPE, typename COEFFS_TYPE>
struct ResamplerContext : public SharedObject {
    AudioFormat                 iFormat;
    AudioFormat                 oFormat;
    const SampleDescriptor *    iDesc;
    const SampleDescriptor *    oDesc;
    State<TYPE, COEFFS_TYPE>    mStates[NB_CHANNELS];
};

template <typename TYPE, typename COEFFS_TYPE>
static MediaUnitContext resampler_alloc() {
    sp<ResamplerContext<TYPE, COEFFS_TYPE> > resampler = new ResamplerContext<TYPE, COEFFS_TYPE>;
    return resampler->RetainObject();
}

template <typename TYPE, typename COEFFS_TYPE>
static void resampler_dealloc(MediaUnitContext ref) {
    sp<ResamplerContext<TYPE, COEFFS_TYPE> > resampler = static_cast<ResamplerContext<TYPE, COEFFS_TYPE> *>(ref);
    resampler->ReleaseObject();
}

template <typename TYPE, typename COEFFS_TYPE>
static MediaError resampler_init(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    sp<ResamplerContext<TYPE, COEFFS_TYPE> > resampler = static_cast<ResamplerContext<TYPE, COEFFS_TYPE> *>(ref);
    if (iformat->format != oformat->format ||
        iformat->audio.channels != oformat->audio.channels ||
        iformat->audio.freq == oformat->audio.freq) {
        ERROR("bad parameters");
        return kMediaErrorBadParameters;
    }
    
    resampler->iFormat  = iformat->audio;
    resampler->oFormat  = oformat->audio;
    resampler->iDesc    = GetSampleFormatDescriptor(iformat->format);
    resampler->oDesc    = GetSampleFormatDescriptor(oformat->format);
    if (resampler->iDesc->planar == False || resampler->oDesc->planar == False) {
        ERROR("resampler only support planar samples");
        return kMediaErrorBadParameters;
    }
    return kMediaNoError;
}

template <typename FROM, typename TO, typename COEFFS_TYPE>
static MediaError resampler_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    sp<ResamplerContext<TO, COEFFS_TYPE> > resampler = static_cast<ResamplerContext<TO, COEFFS_TYPE> *>(ref);
    if (resampler->iFormat.channels != input->count ||
        resampler->oFormat.channels != output->count) {
        ERROR("bad MediaBufferList");
        return kMediaErrorBadParameters;
    }
    const UInt32 iSamples = input->buffers[0].size / sizeof(FROM);
    const UInt32 oSamples = (iSamples * resampler->oFormat.freq) / resampler->iFormat.freq + 1;
    if (output->buffers[0].capacity < oSamples * sizeof(TO)) {
        ERROR("bad output MediaBufferList");
        return kMediaErrorBadParameters;
    }
    for (UInt32 i = 0; i < resampler->iFormat.channels; ++i) {
        const UInt32 samples = resample1<FROM, TO, COEFFS_TYPE>()(resampler->mStates[i],
                                                                  (const FROM *)input->buffers[i].data,
                                                                  iSamples,
                                                                  (TO *)output->buffers[i].data);
        output->buffers[i].size = samples * sizeof(TO);
    }
    return kMediaNoError;
}

template <typename TYPE, typename COEFFS_TYPE>
static MediaError resampler_reset(MediaUnitContext ref) {
    sp<ResamplerContext<TYPE, COEFFS_TYPE> > resampler = static_cast<ResamplerContext<TYPE, COEFFS_TYPE> *>(ref);
    for (UInt32 i = 0; i < NB_CHANNELS; ++i) {
        resampler->mStates[i] = 0;
    }
    return kMediaNoError;
}

static MediaError planarization_init(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    sp<DownmixContext> planarization = static_cast<DownmixContext *>(ref);
    if (iformat->audio.channels != oformat->audio.channels ||
        iformat->audio.freq != oformat->audio.freq) {
        ERROR("bad input/output format");
        return kMediaErrorBadParameters;
    }
    
    planarization->iFormat  = iformat->audio;
    planarization->oFormat  = oformat->audio;
    planarization->iDesc    = GetSampleFormatDescriptor(iformat->format);
    planarization->oDesc    = GetSampleFormatDescriptor(oformat->format);
    // packed <-> planar
    if (planarization->iDesc->planar == planarization->oDesc->planar) {
        ERROR("bad input/output format");
        return kMediaErrorBadParameters;
    }
    return kMediaNoError;
}

template <typename FROM, typename TO>
static MediaError planarization_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    sp<DownmixContext> planarization = static_cast<DownmixContext *>(ref);
    if (input->count != 1 ||
        planarization->oFormat.channels != output->count) {
        return kMediaErrorBadParameters;
    }
    
    const UInt32 samples = input->buffers[0].size / (sizeof(FROM) * planarization->iFormat.channels);
    const FROM * src = (const FROM *)input->buffers[0].data;
    for (UInt32 i = 0; i < planarization->oFormat.channels; ++i) {
        if (output->buffers[i].capacity < samples * sizeof(TO)) {
            return kMediaErrorBadParameters;
        }
        
        TO * dst = (TO *)output->buffers[i].data;
        for (UInt32 j = 0; j < samples; ++j) {
            dst[j] = expr<FROM, TO>()(src[planarization->iFormat.channels * j + i]);
        }
        output->buffers[i].size = samples * sizeof(TO);
    }
    
    return kMediaNoError;
}

template <typename FROM, typename TO>
static MediaError interleave_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    sp<DownmixContext> interleave = static_cast<DownmixContext *>(ref);
    if (interleave->iFormat.channels != input->count ||
        output->count != 1) {
        ERROR("bad input/output buffer");
        return kMediaErrorBadParameters;
    }
    
    const UInt32 samples = input->buffers[0].size / sizeof(FROM);
    if (output->buffers[0].capacity < samples * interleave->oFormat.channels * sizeof(TO)) {
        ERROR("bad output buffer capacity");
        return kMediaErrorBadParameters;
    }
    
    TO * dst = (TO *)output->buffers[0].data;
    for (UInt32 i = 0; i < interleave->iFormat.channels; ++i) {
        const FROM * src = (const FROM *)input->buffers[i].data;
        for (UInt32 j = 0; j < samples; ++j) {
            dst[interleave->oFormat.channels * j + i] = expr<FROM, TO>()(src[j]);
        }
    }
    output->buffers[0].size = samples * interleave->oFormat.channels * sizeof(TO);
    return kMediaNoError;
}

#define DOWNMIX(FMT, TYPE)                                                          \
static const MediaUnit kDownmix##FMT = {                                            \
    .name       = "downmix " #FMT,                                                  \
    .flags      = kMediaUnitProcessInplace,                                         \
    .iformats   = (const UInt32[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .oformats   = (const UInt32[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .alloc      = downmix_alloc,                                                    \
    .dealloc    = downmix_dealloc,                                                  \
    .init       = downmix_init,                                                     \
    .process    = downmix_process<TYPE, TYPE>,                                      \
    .reset      = Nil                                                              \
};
DOWNMIX(U8, UInt8)
DOWNMIX(S16, Int16)
DOWNMIX(S32, Int32)
DOWNMIX(F32, Float32)
DOWNMIX(F64, Float64)

#define DOWNMIX16(FMT, TYPE)                                                        \
static const MediaUnit kDownmixS16From##FMT = {                                     \
    .name       = "downmix s16<" #FMT,                                              \
    .flags      = kMediaUnitProcessInplace,                                         \
    .iformats   = (const UInt32[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .oformats   = (const UInt32[]){ kSampleFormatS16, kSampleFormatUnknown },     \
    .alloc      = downmix_alloc,                                                    \
    .dealloc    = downmix_dealloc,                                                  \
    .init       = downmix_init,                                                     \
    .process    = downmix_process<TYPE, Int16>,                                   \
    .reset      = Nil                                                              \
};
DOWNMIX16(U8, UInt8)
DOWNMIX16(S32, Int32)
DOWNMIX16(F32, Float32)
DOWNMIX16(F64, Float64)

#define DOWNMIX32(FMT, TYPE)                                                        \
static const MediaUnit kDownmixS32From##FMT = {                                     \
    .name       = "downmix s32<" #FMT,                                              \
    .flags      = kMediaUnitProcessInplace,                                         \
    .iformats   = (const UInt32[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .oformats   = (const UInt32[]){ kSampleFormatS32, kSampleFormatUnknown },     \
    .alloc      = downmix_alloc,                                                    \
    .dealloc    = downmix_dealloc,                                                  \
    .init       = downmix_init,                                                     \
    .process    = downmix_process<TYPE, Int32>,                                   \
    .reset      = Nil                                                              \
};
DOWNMIX32(U8, UInt8)
DOWNMIX32(S16, Int16)
DOWNMIX32(S32, Int32)
DOWNMIX32(F32, Float32)
DOWNMIX32(F64, Float64)

#define RESAMPLE(FMT, TYPE)                                                         \
static const MediaUnit kResample##FMT = {                                           \
    .name       = "resampler " #FMT,                                                \
    .flags      = 0,                                                                \
    .iformats   = (const UInt32[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .oformats   = (const UInt32[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .alloc      = resampler_alloc<TYPE, Float64>,                                    \
    .dealloc    = resampler_dealloc<TYPE, Float64>,                                  \
    .init       = resampler_init<TYPE, Float64>,                                     \
    .process    = resampler_process<TYPE, TYPE, Float64>,                            \
    .reset      = resampler_reset<TYPE, Float64>                                     \
};
RESAMPLE(U8, UInt8)
RESAMPLE(S16, Int16)
RESAMPLE(S32, Int32)
RESAMPLE(F32, Float32)
RESAMPLE(F64, Float64)

#define RESAMPLE16(FMT, TYPE)                                                       \
static const MediaUnit kResampleS16From##FMT = {                                    \
    .name       = "resampler s16<" #FMT,                                            \
    .flags      = 0,                                                                \
    .iformats   = (const UInt32[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .oformats   = (const UInt32[]){ kSampleFormatS16, kSampleFormatUnknown },     \
    .alloc      = resampler_alloc<Int16, Float64>,                                 \
    .dealloc    = resampler_dealloc<Int16, Float64>,                               \
    .init       = resampler_init<Int16, Float64>,                                  \
    .process    = resampler_process<TYPE, Int16, Float64>,                         \
    .reset      = resampler_reset<Int16, Float64>                                  \
};
RESAMPLE16(U8, UInt8)
RESAMPLE16(S32, Int32)
RESAMPLE16(F32, Float32)
RESAMPLE16(F64, Float64)

#define RESAMPLE32(FMT, TYPE)                                                       \
static const MediaUnit kResampleS32From##FMT = {                                    \
    .name       = "resampler s32<" #FMT,                                            \
    .flags      = 0,                                                                \
    .iformats   = (const UInt32[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .oformats   = (const UInt32[]){ kSampleFormatS32, kSampleFormatUnknown },     \
    .alloc      = resampler_alloc<Int32, Float64>,                                 \
    .dealloc    = resampler_dealloc<Int32, Float64>,                               \
    .init       = resampler_init<Int32, Float64>,                                  \
    .process    = resampler_process<TYPE, Int32, Float64>,                         \
    .reset      = resampler_reset<Int32, Float64>                                  \
};
RESAMPLE32(U8, UInt8)
RESAMPLE32(S16, Int16)
RESAMPLE32(F32, Float32)
RESAMPLE32(F64, Float64)

#define PLANARIZATION(FMT, TYPE)                                                                \
static const MediaUnit kPlanarization##FMT = {                                                  \
    .name       = "planarization " #FMT,                                                        \
    .flags      = 0,                                                                            \
    .iformats   = (const eSampleFormat[]){ kSampleFormat##FMT##Packed, kSampleFormatUnknown },  \
    .oformats   = (const eSampleFormat[]){ kSampleFormat##FMT, kSampleFormatUnknown },          \
    .alloc      = downmix_alloc,                                                                \
    .dealloc    = downmix_dealloc,                                                              \
    .init       = planarization_init,                                                           \
    .process    = planarization_process<TYPE, TYPE>,                                            \
    .reset      = Nil                                                                          \
};
PLANARIZATION(U8, UInt8)
PLANARIZATION(S16, Int16)
PLANARIZATION(S32, Int32)
PLANARIZATION(F32, Float32)
PLANARIZATION(F64, Float64)

#define PLANARIZATION16(FMT, TYPE)                                                              \
static const MediaUnit kPlanarizationS16From##FMT = {                                           \
    .name       = "planarization s16<" #FMT,                                                    \
    .flags      = 0,                                                                            \
    .iformats   = (const eSampleFormat[]){ kSampleFormat##FMT##Packed, kSampleFormatUnknown },  \
    .oformats   = (const eSampleFormat[]){ kSampleFormatS16, kSampleFormatUnknown },            \
    .alloc      = downmix_alloc,                                                                \
    .dealloc    = downmix_dealloc,                                                              \
    .init       = planarization_init,                                                           \
    .process    = planarization_process<TYPE, Int16>,                                         \
    .reset      = Nil                                                                          \
};
PLANARIZATION16(U8, UInt8)
PLANARIZATION16(S32, Int32)
PLANARIZATION16(F32, Float32)
PLANARIZATION16(F64, Float64)

#define PLANARIZATION32(FMT, TYPE)                                                              \
static const MediaUnit kPlanarizationS32From##FMT = {                                           \
    .name       = "planarization s32<" #FMT,                                                    \
    .flags      = 0,                                                                            \
    .iformats   = (const eSampleFormat[]){ kSampleFormat##FMT##Packed, kSampleFormatUnknown },  \
    .oformats   = (const eSampleFormat[]){ kSampleFormatS32, kSampleFormatUnknown },            \
    .alloc      = downmix_alloc,                                                                \
    .dealloc    = downmix_dealloc,                                                              \
    .init       = planarization_init,                                                           \
    .process    = planarization_process<TYPE, Int32>,                                         \
    .reset      = Nil                                                                          \
};
PLANARIZATION32(U8, UInt8)
PLANARIZATION32(S16, Int16)
PLANARIZATION32(F32, Float32)
PLANARIZATION32(F64, Float64)

#define INTERLEAVE(FMT, TYPE)                                                                   \
static const MediaUnit kInterleave##FMT = {                                                     \
    .name       = "interleave " #FMT,                                                           \
    .flags      = 0,                                                                            \
    .iformats   = (const eSampleFormat[]){ kSampleFormat##FMT, kSampleFormatUnknown },          \
    .oformats   = (const eSampleFormat[]){ kSampleFormat##FMT##Packed, kSampleFormatUnknown },  \
    .alloc      = downmix_alloc,                                                                \
    .dealloc    = downmix_dealloc,                                                              \
    .init       = planarization_init,                                                           \
    .process    = interleave_process<TYPE, TYPE>,                                               \
    .reset      = Nil                                                                          \
};
INTERLEAVE(U8, UInt8)
INTERLEAVE(S16, Int16)
INTERLEAVE(S32, Int32)
INTERLEAVE(F32, Float32)
INTERLEAVE(F64, Float64)

#define INTERLEAVE16(FMT, TYPE)                                                                 \
static const MediaUnit kInterleaveS16From##FMT = {                                              \
    .name       = "interleave s16<" #FMT,                                                       \
    .flags      = 0,                                                                            \
    .iformats   = (const eSampleFormat[]){ kSampleFormat##FMT, kSampleFormatUnknown },          \
    .oformats   = (const eSampleFormat[]){ kSampleFormatS16Packed, kSampleFormatUnknown },      \
    .alloc      = downmix_alloc,                                                                \
    .dealloc    = downmix_dealloc,                                                              \
    .init       = planarization_init,                                                           \
    .process    = interleave_process<TYPE, Int16>,                                            \
    .reset      = Nil                                                                          \
};
INTERLEAVE16(U8, UInt8)
INTERLEAVE16(S32, Int32)
INTERLEAVE16(F32, Float32)
INTERLEAVE16(F64, Float64)

#define INTERLEAVE32(FMT, TYPE)                                                                 \
static const MediaUnit kInterleaveS32From##FMT = {                                              \
    .name       = "interleave s32<" #FMT,                                                       \
    .flags      = 0,                                                                            \
    .iformats   = (const eSampleFormat[]){ kSampleFormat##FMT, kSampleFormatUnknown },          \
    .oformats   = (const eSampleFormat[]){ kSampleFormatS32Packed, kSampleFormatUnknown },      \
    .alloc      = downmix_alloc,                                                                \
    .dealloc    = downmix_dealloc,                                                              \
    .init       = planarization_init,                                                           \
    .process    = interleave_process<TYPE, Int32>,                                            \
    .reset      = Nil                                                                          \
};
INTERLEAVE32(U8, UInt8)
INTERLEAVE32(S16, Int16)
INTERLEAVE32(F32, Float32)
INTERLEAVE32(F64, Float64)

static const MediaUnit * kAudioUnitList[] = {
    // downmix
    &kDownmixU8,
    &kDownmixS16,
    &kDownmixS32,
    &kDownmixF32,
    &kDownmixF64,
    &kDownmixS16FromU8,
    &kDownmixS16FromS32,
    &kDownmixS16FromF32,
    &kDownmixS16FromF64,
    &kDownmixS32FromU8,
    &kDownmixS32FromS16,
    &kDownmixS32FromS32,
    &kDownmixS32FromF32,
    &kDownmixS32FromF64,
    // resample
    &kResampleU8,
    &kResampleS16,
    &kResampleS32,
    &kResampleF32,
    &kResampleF64,
    &kResampleS16FromU8,
    &kResampleS16FromS32,
    &kResampleS16FromF32,
    &kResampleS16FromF64,
    &kResampleS32FromU8,
    &kResampleS32FromS16,
    &kResampleS32FromF32,
    &kResampleS32FromF64,
    // planarization
    &kPlanarizationU8,
    &kPlanarizationS16,
    &kPlanarizationS32,
    &kPlanarizationF32,
    &kPlanarizationF64,
    &kPlanarizationS16FromU8,
    &kPlanarizationS16FromS32,
    &kPlanarizationS16FromF32,
    &kPlanarizationS16FromF64,
    &kPlanarizationS32FromU8,
    &kPlanarizationS32FromS16,
    &kPlanarizationS32FromF32,
    &kPlanarizationS32FromF64,
    // interleave
    &kInterleaveU8,
    &kInterleaveS16,
    &kInterleaveS32,
    &kInterleaveF32,
    &kInterleaveF64,
    &kInterleaveS16FromU8,
    &kInterleaveS16FromS32,
    &kInterleaveS16FromF32,
    &kInterleaveS16FromF64,
    &kInterleaveS32FromU8,
    &kInterleaveS32FromS16,
    &kInterleaveS32FromF32,
    &kInterleaveS32FromF64,
    // END OF LIST
    Nil
};

static FORCE_INLINE Bool SampleFormatContains(const eSampleFormat * formats, const eSampleFormat& sample) {
    for (UInt32 i = 0; formats[i] != kSampleFormatUnknown; ++i) {
        if (formats[i] == sample) return True;
    }
    return False;
}
static const MediaUnit * FindAudioUnit(const eSampleFormat& iformat, const eSampleFormat& oformat) {
    for (UInt32 i = 0; kAudioUnitList[i] != Nil; ++i) {
        if (SampleFormatContains(kAudioUnitList[i]->iformats, iformat) &&
            SampleFormatContains(kAudioUnitList[i]->oformats, oformat)) {
            return kAudioUnitList[i];
        }
    }
    return Nil;
}

struct AudioConverter : public MediaDevice {
    AudioFormat                 oFormat;
    Vector<const MediaUnit *>   mUnits;
    Vector<MediaUnitContext>    mInstances;
    sp<MediaFrame>              mOutput;
    
    AudioConverter() : MediaDevice() { }
    
    virtual ~AudioConverter() {
        for (UInt32 i = 0; i < mUnits.size(); ++i) {
            mUnits[i]->dealloc(mInstances[i]);
        }
        mUnits.clear();
        mInstances.clear();
    }
    
    MediaError init(const AudioFormat& iformat, const AudioFormat& oformat, const sp<Message>& options) {
        INFO("init AudioConverter: %s => %s", GetAudioFormatString(iformat).c_str(), GetAudioFormatString(oformat).c_str());
        oFormat     = oformat;
        
        // direct convert
        const MediaUnit * unit = FindAudioUnit(iformat.format, oformat.format);
        if (unit) {
            MediaUnitContext instance = unit->alloc();
            if (unit->init(instance, (const MediaFormat *)&iformat, (const MediaFormat *)&oformat) == kMediaNoError) {
                mUnits.push(unit);
                mInstances.push(instance);
                return kMediaNoError;
            }
            unit->dealloc(instance);
        }
        
        // complex unit graph
        
        ERROR("init AudioConverter failed");
        return kMediaErrorBadParameters;
    }
    
    virtual sp<Message> formats() const {
        sp<Message> format = new Message;
        format->setInt32(kKeyFormat, oFormat.format);
        format->setInt32(kKeyChannels, oFormat.channels);
        format->setInt32(kKeySampleRate, oFormat.freq);
        return format;
    }
    
    virtual MediaError configure(const sp<Message>& options) {
        return kMediaErrorNotSupported;
    }
    
    virtual MediaError push(const sp<MediaFrame>& input) {
        if (!mOutput.isNil()) {
            return kMediaErrorResourceBusy;
        }
        
        AudioFormat             audio = oFormat;
        audio.samples           = input->audio.samples;
        sp<MediaFrame> output   = MediaFrame::Create(audio);
        
        MediaError st = mUnits[0]->process(mInstances[0],
                                           &input->planes,
                                           &output->planes);
        
        if (st != kMediaNoError) {
            ERROR("push %s failed", input->string().c_str());
            return kMediaErrorUnknown;
        }
        
        output->id          = input->id;
        output->flags       = input->flags;
        output->timecode    = input->timecode;
        output->duration    = input->duration;
        mOutput = output;
        return kMediaNoError;
    }
    
    virtual sp<MediaFrame> pull() {
        sp<MediaFrame> frame = mOutput;
        mOutput.clear();
        return frame;
    }
    
    virtual MediaError reset() {
        for (UInt32 i = 0; i < mUnits.size(); ++i) {
            if (mUnits[i]->reset) {
                mUnits[i]->reset(mInstances[i]);
            }
        }
        return kMediaNoError;
    }
};

sp<MediaDevice> CreateAudioConverter(const AudioFormat& iformat, const AudioFormat& oformat, const sp<Message>& options) {
    sp<AudioConverter> ac = new AudioConverter;
    if (ac->init(iformat, oformat, options) != kMediaNoError) {
        return Nil;
    }
    return ac;
}

__END_NAMESPACE_MFWK




