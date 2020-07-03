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
    .planar     = true,
    .bytes      = 1,
};

static const SampleDescriptor kSampleS16 = {
    .name       = "s16p",
    .format     = kSampleFormatS16,
    .similar    = kSampleFormatS16Packed,
    .planar     = true,
    .bytes      = 2,
};

static const SampleDescriptor kSampleS32 = {
    .name       = "s32p",
    .format     = kSampleFormatS32,
    .similar    = kSampleFormatS32Packed,
    .planar     = true,
    .bytes      = 4,
};

static const SampleDescriptor kSampleFLT = {
    .name       = "fltp",
    .format     = kSampleFormatFLT,
    .similar    = kSampleFormatFLTPacked,
    .planar     = true,
    .bytes      = sizeof(float),
};

static const SampleDescriptor kSampleDBL = {
    .name       = "dblp",
    .format     = kSampleFormatDBL,
    .similar    = kSampleFormatDBLPacked,
    .planar     = true,
    .bytes      = sizeof(double),
};

static const SampleDescriptor kSampleU8Packed = {
    .name       = "u8i",
    .format     = kSampleFormatU8Packed,
    .similar    = kSampleFormatU8,
    .planar     = false,
    .bytes      = 1,
};

static const SampleDescriptor kSampleS16Packed = {
    .name       = "s16i",
    .format     = kSampleFormatS16Packed,
    .similar    = kSampleFormatS16,
    .planar     = false,
    .bytes      = 2,
};

static const SampleDescriptor kSampleS32Packed = {
    .name       = "s32i",
    .format     = kSampleFormatS32Packed,
    .similar    = kSampleFormatS32,
    .planar     = false,
    .bytes      = 4,
};

static const SampleDescriptor kSampleFLTPacked = {
    .name       = "flti",
    .format     = kSampleFormatFLTPacked,
    .similar    = kSampleFormatFLT,
    .planar     = false,
    .bytes      = sizeof(float),
};

static const SampleDescriptor kSampleDBLPacked = {
    .name       = "dbli",
    .format     = kSampleFormatDBLPacked,
    .similar    = kSampleFormatDBL,
    .planar     = false,
    .bytes      = sizeof(double),
};

static const SampleDescriptor * kSampleDiscriptors[] = {
    &kSampleU8,
    &kSampleS16,
    &kSampleS32,
    &kSampleFLT,
    &kSampleDBL,
    &kSampleU8Packed,
    &kSampleS16Packed,
    &kSampleS32Packed,
    &kSampleFLTPacked,
    &kSampleDBLPacked,
    NULL
};

const SampleDescriptor * GetSampleFormatDescriptor(eSampleFormat sample) {
    for (size_t i = 0; kSampleDiscriptors[i] != NULL; ++i) {
        if (kSampleDiscriptors[i]->format == sample) {
            return kSampleDiscriptors[i];
        }
    }
    return NULL;
}

eSampleFormat GetSimilarSampleFormat(eSampleFormat sample) {
    const SampleDescriptor * desc = GetSampleFormatDescriptor(sample);
    CHECK_NULL(desc);
    return desc->similar;
}

size_t GetSampleFormatBytes(eSampleFormat sample) {
    const SampleDescriptor * desc = GetSampleFormatDescriptor(sample);
    CHECK_NULL(desc);
    return desc->bytes;
}

bool IsPlanarSampleFormat(eSampleFormat sample) {
    const SampleDescriptor * desc = GetSampleFormatDescriptor(sample);
    CHECK_NULL(desc);
    return desc->planar;
}

static const eSampleFormat kPlanarSamples[] = {
    kSampleFormatU8,
    kSampleFormatS16,
    kSampleFormatS32,
    kSampleFormatFLT,
    kSampleFormatDBL,
    kSampleFormatUnknown,
};

static const eSampleFormat kPackedSamples[] = {
    kSampleFormatU8Packed,
    kSampleFormatS16Packed,
    kSampleFormatS32Packed,
    kSampleFormatFLTPacked,
    kSampleFormatDBLPacked,
    kSampleFormatUnknown,
};

__END_DECLS

__BEGIN_NAMESPACE_MPX

// TO (*convert)(FROM from)
template <typename FROM, typename TO> struct expr;

// TYPE -> TYPE
template <typename TYPE> struct expr<TYPE, TYPE> {
    FORCE_INLINE TYPE operator()(const TYPE v)          { return v;                             }
};

// -> int16_t
template <> struct expr<uint8_t, int16_t> {
    FORCE_INLINE int16_t operator()(const uint8_t v)    { return (((int16_t)v - 0x80) << 8);    }
};
template <> struct expr<int32_t, int16_t> {
    FORCE_INLINE int16_t operator()(const int32_t v)    { return clamp16((v + (1<<15)) >> 16);  }
};
template <> struct expr<float, int16_t> {
    FORCE_INLINE int16_t operator()(const float v)      { return clamp16_from_float(v);         }
};
template <> struct expr<double, int16_t> {
    FORCE_INLINE int16_t operator()(const double v)     { return clamp16_from_float(v);         }
};

// -> int32_t
template <> struct expr<uint8_t, int32_t> {
    FORCE_INLINE int32_t operator()(const uint8_t v)    { return (((int16_t)v - 0x80) << 24);   }
};
template <> struct expr<int16_t, int32_t> {
    FORCE_INLINE int32_t operator()(const int16_t v)    { return (v << 16);                     }
};
template <> struct expr<float, int32_t> {
    FORCE_INLINE int32_t operator()(const float v)      { return clamp32_from_float(v);         }
};
template <> struct expr<double, int32_t> {
    FORCE_INLINE int32_t operator()(const double v)     { return clamp32_from_float(v);         }
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
    sp<DownmixContext> downmix = ref;
    downmix->ReleaseObject();
}

static MediaError downmix_init(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    sp<DownmixContext> downmix = ref;
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
    if (downmix->iDesc->planar == false || downmix->oDesc->planar == false) {
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
    sp<DownmixContext> downmix = ref;
    if (input->count != downmix->iFormat.channels || output->count != downmix->oFormat.channels) {
        ERROR("bad MediaBufferList");
        return kMediaErrorBadParameters;
    }
    FROM * iPlanes[8];
    TO * oPlanes[2];
    
    for (size_t i = 0; i < input->count; ++i) {
        iPlanes[i] = (FROM *)input->buffers[i].data;
    }
    for (size_t i = 0; i < output->count; ++i) {
        oPlanes[i] = (TO *)output->buffers[i].data;
    }

    if (downmix->iFormat.channels >= 6) {
        for (size_t i = 0; i < input->buffers[0].size / sizeof(FROM); ++i) {
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
template <typename TYPE> struct lerp<TYPE, double> {
    FORCE_INLINE TYPE operator()(const TYPE s0, const TYPE s1, double t)  { return (1 - t) * s0 + t * s1;     }
};

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
            out[outIndex++] = lerp<TYPE, double>()(state.last, in[0], inIndex - x0);

            // TODO: using a advance template
            inIndex += increment;
            x0 = (size_t)inIndex;
        }

        while (x0 < nsamples) {
            out[outIndex++] = lerp<TYPE, double>()(in[x0 - 1], in[x0], inIndex - x0);

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
            out[outIndex++] = lerp<TO, double>()(state.last,
                    expr<FROM, TO>()(in[0]),
                    inIndex - x0);

            // TODO: using a advance template
            inIndex += increment;
            x0 = (size_t)inIndex;
        }

        while (x0 < nsamples) {
            out[outIndex++] = lerp<TO, double>()(expr<FROM, TO>()(in[x0 - 1]),
                    expr<FROM, TO>()(in[x0]),
                    inIndex - x0);

            inIndex += increment;
            x0 = (size_t)inIndex;
        }

        state.last = expr<FROM, TO>()(in[nsamples - 1]);
        state.fraction = inIndex - (size_t)inIndex;
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
    sp<ResamplerContext<TYPE, COEFFS_TYPE> > resampler = ref;
    resampler->ReleaseObject();
}

template <typename TYPE, typename COEFFS_TYPE>
static MediaError resampler_init(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    sp<ResamplerContext<TYPE, COEFFS_TYPE> > resampler = ref;
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
    if (resampler->iDesc->planar == false || resampler->oDesc->planar == false) {
        ERROR("resampler only support planar samples");
        return kMediaErrorBadParameters;
    }
    return kMediaNoError;
}

template <typename FROM, typename TO, typename COEFFS_TYPE>
static MediaError resampler_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    sp<ResamplerContext<TO, COEFFS_TYPE> > resampler = ref;
    if (resampler->iFormat.channels != input->count ||
        resampler->oFormat.channels != output->count) {
        ERROR("bad MediaBufferList");
        return kMediaErrorBadParameters;
    }
    const size_t iSamples = input->buffers[0].size / sizeof(FROM);
    const size_t oSamples = (iSamples * resampler->oFormat.freq) / resampler->iFormat.freq + 1;
    if (output->buffers[0].capacity < oSamples * sizeof(TO)) {
        ERROR("bad output MediaBufferList");
        return kMediaErrorBadParameters;
    }
    for (size_t i = 0; i < resampler->iFormat.channels; ++i) {
        const size_t samples = resample1<FROM, TO, COEFFS_TYPE>()(resampler->mStates[i],
                                                                  (const FROM *)input->buffers[i].data,
                                                                  iSamples,
                                                                  (TO *)output->buffers[i].data);
        output->buffers[i].size = samples * sizeof(TO);
    }
    return kMediaNoError;
}

template <typename TYPE, typename COEFFS_TYPE>
static MediaError resampler_reset(MediaUnitContext ref) {
    sp<ResamplerContext<TYPE, COEFFS_TYPE> > resampler = ref;
    for (size_t i = 0; i < NB_CHANNELS; ++i) {
        resampler->mStates[i] = 0;
    }
    return kMediaNoError;
}

static MediaError planarization_init(MediaUnitContext ref, const MediaFormat * iformat, const MediaFormat * oformat) {
    sp<DownmixContext> planarization = ref;
    if (iformat->audio.channels != oformat->audio.channels ||
        iformat->audio.freq != oformat->audio.freq) {
        return kMediaErrorBadParameters;
    }
    
    planarization->iFormat  = iformat->audio;
    planarization->oFormat  = oformat->audio;
    planarization->iDesc    = GetSampleFormatDescriptor(iformat->format);
    planarization->oDesc    = GetSampleFormatDescriptor(oformat->format);
    // packed <-> planar
    if (planarization->iDesc->format != planarization->oDesc->similar ||
        planarization->iDesc->planar == planarization->oDesc->planar) {
        return kMediaErrorBadParameters;
    }
    return kMediaNoError;
}

template <typename FROM, typename TO>
static MediaError planarization_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    sp<DownmixContext> planarization = ref;
    if (input->count != 1 ||
        planarization->oFormat.channels != output->count) {
        return kMediaErrorBadParameters;
    }
    
    const size_t samples = input->buffers[0].size / (sizeof(FROM) * planarization->iFormat.channels);
    const FROM * src = (const FROM *)input->buffers[0].data;
    for (size_t i = 0; i < planarization->oFormat.channels; ++i) {
        if (output->buffers[i].capacity < samples * sizeof(TO)) {
            return kMediaErrorBadParameters;
        }
        
        TO * dst = (TO *)output->buffers[i].data;
        for (size_t j = 0; j < samples; ++j) {
            dst[j] = expr<FROM, TO>()(src[planarization->iFormat.channels * j + i]);
        }
        output->buffers[i].size = samples * sizeof(TO);
    }
    
    return kMediaNoError;
}

template <typename FROM, typename TO>
static MediaError interleave_process(MediaUnitContext ref, const MediaBufferList * input, MediaBufferList * output) {
    sp<DownmixContext> interleave = ref;
    if (interleave->iFormat.channels != input->count ||
        output->count != 1) {
        return kMediaErrorBadParameters;
    }
    
    const size_t samples = input->buffers[0].size / sizeof(FROM);
    if (output->buffers[0].capacity < samples * interleave->oFormat.channels * sizeof(TO)) {
        return kMediaErrorBadParameters;
    }
    
    TO * dst = (TO *)output->buffers[0].data;
    for (size_t i = 0; i < interleave->iFormat.channels; ++i) {
        const FROM * src = (const FROM *)input->buffers[i].data;
        for (size_t j = 0; j < samples; ++j) {
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
    .iformats   = (const uint32_t[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .oformats   = (const uint32_t[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .alloc      = downmix_alloc,                                                    \
    .dealloc    = downmix_dealloc,                                                  \
    .init       = downmix_init,                                                     \
    .process    = downmix_process<TYPE, TYPE>,                                      \
    .reset      = NULL                                                              \
};
DOWNMIX(U8, uint8_t)
DOWNMIX(S16, int16_t)
DOWNMIX(S32, int32_t)
DOWNMIX(FLT, float)
DOWNMIX(DBL, double)

#define DOWNMIX16(FMT, TYPE)                                                        \
static const MediaUnit kDownmixS16From##FMT = {                                     \
    .name       = "downmix s16<" #FMT,                                              \
    .flags      = kMediaUnitProcessInplace,                                         \
    .iformats   = (const uint32_t[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .oformats   = (const uint32_t[]){ kSampleFormatS16, kSampleFormatUnknown },     \
    .alloc      = downmix_alloc,                                                    \
    .dealloc    = downmix_dealloc,                                                  \
    .init       = downmix_init,                                                     \
    .process    = downmix_process<TYPE, int16_t>,                                   \
    .reset      = NULL                                                              \
};
DOWNMIX16(U8, uint8_t)
DOWNMIX16(S32, int32_t)
DOWNMIX16(FLT, float)
DOWNMIX16(DBL, double)

#define DOWNMIX32(FMT, TYPE)                                                        \
static const MediaUnit kDownmixS32From##FMT = {                                     \
    .name       = "downmix s32<" #FMT,                                              \
    .flags      = kMediaUnitProcessInplace,                                         \
    .iformats   = (const uint32_t[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .oformats   = (const uint32_t[]){ kSampleFormatS32, kSampleFormatUnknown },     \
    .alloc      = downmix_alloc,                                                    \
    .dealloc    = downmix_dealloc,                                                  \
    .init       = downmix_init,                                                     \
    .process    = downmix_process<TYPE, int32_t>,                                   \
    .reset      = NULL                                                              \
};
DOWNMIX32(U8, uint8_t)
DOWNMIX32(S16, int16_t)
DOWNMIX32(S32, int32_t)
DOWNMIX32(FLT, float)
DOWNMIX32(DBL, double)

#define RESAMPLE(FMT, TYPE)                                                         \
static const MediaUnit kResample##FMT = {                                           \
    .name       = "resampler " #FMT,                                                \
    .flags      = 0,                                                                \
    .iformats   = (const uint32_t[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .oformats   = (const uint32_t[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .alloc      = resampler_alloc<TYPE, double>,                                    \
    .dealloc    = resampler_dealloc<TYPE, double>,                                  \
    .init       = resampler_init<TYPE, double>,                                     \
    .process    = resampler_process<TYPE, TYPE, double>,                            \
    .reset      = resampler_reset<TYPE, double>                                     \
};
RESAMPLE(U8, uint8_t)
RESAMPLE(S16, int16_t)
RESAMPLE(S32, int32_t)
RESAMPLE(FLT, float)
RESAMPLE(DBL, double)

#define RESAMPLE16(FMT, TYPE)                                                       \
static const MediaUnit kResampleS16From##FMT = {                                    \
    .name       = "resampler s16<" #FMT,                                            \
    .flags      = 0,                                                                \
    .iformats   = (const uint32_t[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .oformats   = (const uint32_t[]){ kSampleFormatS16, kSampleFormatUnknown },     \
    .alloc      = resampler_alloc<int16_t, double>,                                 \
    .dealloc    = resampler_dealloc<int16_t, double>,                               \
    .init       = resampler_init<int16_t, double>,                                  \
    .process    = resampler_process<TYPE, int16_t, double>,                         \
    .reset      = resampler_reset<int16_t, double>                                  \
};
RESAMPLE16(U8, uint8_t)
RESAMPLE16(S32, int32_t)
RESAMPLE16(FLT, float)
RESAMPLE16(DBL, double)

#define RESAMPLE32(FMT, TYPE)                                                       \
static const MediaUnit kResampleS32From##FMT = {                                    \
    .name       = "resampler s32<" #FMT,                                            \
    .flags      = 0,                                                                \
    .iformats   = (const uint32_t[]){ kSampleFormat##FMT, kSampleFormatUnknown },   \
    .oformats   = (const uint32_t[]){ kSampleFormatS32, kSampleFormatUnknown },     \
    .alloc      = resampler_alloc<int32_t, double>,                                 \
    .dealloc    = resampler_dealloc<int32_t, double>,                               \
    .init       = resampler_init<int32_t, double>,                                  \
    .process    = resampler_process<TYPE, int32_t, double>,                         \
    .reset      = resampler_reset<int32_t, double>                                  \
};
RESAMPLE32(U8, uint8_t)
RESAMPLE32(S16, int16_t)
RESAMPLE32(FLT, float)
RESAMPLE32(DBL, double)

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
    .reset      = NULL                                                                          \
};
PLANARIZATION(U8, uint8_t)
PLANARIZATION(S16, int16_t)
PLANARIZATION(S32, int32_t)
PLANARIZATION(FLT, float)
PLANARIZATION(DBL, double)

#define PLANARIZATION16(FMT, TYPE)                                                              \
static const MediaUnit kPlanarizationS16From##FMT = {                                           \
    .name       = "planarization s16<" #FMT,                                                    \
    .flags      = 0,                                                                            \
    .iformats   = (const eSampleFormat[]){ kSampleFormat##FMT##Packed, kSampleFormatUnknown },  \
    .oformats   = (const eSampleFormat[]){ kSampleFormatS16, kSampleFormatUnknown },            \
    .alloc      = downmix_alloc,                                                                \
    .dealloc    = downmix_dealloc,                                                              \
    .init       = planarization_init,                                                           \
    .process    = planarization_process<TYPE, int16_t>,                                         \
    .reset      = NULL                                                                          \
};
PLANARIZATION16(U8, uint8_t)
PLANARIZATION16(S32, int32_t)
PLANARIZATION16(FLT, float)
PLANARIZATION16(DBL, double)

#define PLANARIZATION32(FMT, TYPE)                                                              \
static const MediaUnit kPlanarizationS32From##FMT = {                                           \
    .name       = "planarization s32<" #FMT,                                                    \
    .flags      = 0,                                                                            \
    .iformats   = (const eSampleFormat[]){ kSampleFormat##FMT##Packed, kSampleFormatUnknown },  \
    .oformats   = (const eSampleFormat[]){ kSampleFormatS32, kSampleFormatUnknown },            \
    .alloc      = downmix_alloc,                                                                \
    .dealloc    = downmix_dealloc,                                                              \
    .init       = planarization_init,                                                           \
    .process    = planarization_process<TYPE, int32_t>,                                         \
    .reset      = NULL                                                                          \
};
PLANARIZATION32(U8, uint8_t)
PLANARIZATION32(S16, int16_t)
PLANARIZATION32(FLT, float)
PLANARIZATION32(DBL, double)

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
    .reset      = NULL                                                                          \
};
INTERLEAVE(U8, uint8_t)
INTERLEAVE(S16, int16_t)
INTERLEAVE(S32, int32_t)
INTERLEAVE(FLT, float)
INTERLEAVE(DBL, double)

#define INTERLEAVE16(FMT, TYPE)                                                                 \
static const MediaUnit kInterleaveS16From##FMT = {                                              \
    .name       = "interleave s16<" #FMT,                                                       \
    .flags      = 0,                                                                            \
    .iformats   = (const eSampleFormat[]){ kSampleFormat##FMT, kSampleFormatUnknown },          \
    .oformats   = (const eSampleFormat[]){ kSampleFormatS16Packed, kSampleFormatUnknown },      \
    .alloc      = downmix_alloc,                                                                \
    .dealloc    = downmix_dealloc,                                                              \
    .init       = planarization_init,                                                           \
    .process    = interleave_process<TYPE, int16_t>,                                            \
    .reset      = NULL                                                                          \
};
INTERLEAVE16(U8, uint8_t)
INTERLEAVE16(S32, int32_t)
INTERLEAVE16(FLT, float)
INTERLEAVE16(DBL, double)

#define INTERLEAVE32(FMT, TYPE)                                                                 \
static const MediaUnit kInterleaveS32From##FMT = {                                              \
    .name       = "interleave s32<" #FMT,                                                       \
    .flags      = 0,                                                                            \
    .iformats   = (const eSampleFormat[]){ kSampleFormat##FMT, kSampleFormatUnknown },          \
    .oformats   = (const eSampleFormat[]){ kSampleFormatS32Packed, kSampleFormatUnknown },      \
    .alloc      = downmix_alloc,                                                                \
    .dealloc    = downmix_dealloc,                                                              \
    .init       = planarization_init,                                                           \
    .process    = interleave_process<TYPE, int32_t>,                                            \
    .reset      = NULL                                                                          \
};
INTERLEAVE32(U8, uint8_t)
INTERLEAVE32(S16, int16_t)
INTERLEAVE32(FLT, float)
INTERLEAVE32(DBL, double)

static const MediaUnit * kAudioUnitList[] = {
    // downmix
    &kDownmixU8,
    &kDownmixS16,
    &kDownmixS32,
    &kDownmixFLT,
    &kDownmixDBL,
    &kDownmixS16FromU8,
    &kDownmixS16FromS32,
    &kDownmixS16FromFLT,
    &kDownmixS16FromDBL,
    &kDownmixS32FromU8,
    &kDownmixS32FromS16,
    &kDownmixS32FromS32,
    &kDownmixS32FromFLT,
    &kDownmixS32FromDBL,
    // resample
    &kResampleU8,
    &kResampleS16,
    &kResampleS32,
    &kResampleFLT,
    &kResampleDBL,
    &kResampleS16FromU8,
    &kResampleS16FromS32,
    &kResampleS16FromFLT,
    &kResampleS16FromDBL,
    &kResampleS32FromU8,
    &kResampleS32FromS16,
    &kResampleS32FromFLT,
    &kResampleS32FromDBL,
    // planarization
    &kPlanarizationU8,
    &kPlanarizationS16,
    &kPlanarizationS32,
    &kPlanarizationFLT,
    &kPlanarizationDBL,
    &kPlanarizationS16FromU8,
    &kPlanarizationS16FromS32,
    &kPlanarizationS16FromFLT,
    &kPlanarizationS16FromDBL,
    &kPlanarizationS32FromU8,
    &kPlanarizationS32FromS16,
    &kPlanarizationS32FromFLT,
    &kPlanarizationS32FromDBL,
    // interleave
    &kInterleaveU8,
    &kInterleaveS16,
    &kInterleaveS32,
    &kInterleaveFLT,
    &kInterleaveDBL,
    &kInterleaveS16FromU8,
    &kInterleaveS16FromS32,
    &kInterleaveS16FromFLT,
    &kInterleaveS16FromDBL,
    &kInterleaveS32FromU8,
    &kInterleaveS32FromS16,
    &kInterleaveS32FromFLT,
    &kInterleaveS32FromDBL,
    // END OF LIST
    NULL
};

static FORCE_INLINE bool SampleFormatContains(const eSampleFormat * formats, const eSampleFormat& sample) {
    for (size_t i = 0; formats[i] != kSampleFormatUnknown; ++i) {
        if (formats[i] == sample) return true;
    }
    return false;
}
static const MediaUnit * FindAudioUnit(const eSampleFormat& iformat, const eSampleFormat& oformat) {
    for (size_t i = 0; kAudioUnitList[i] != NULL; ++i) {
        if (SampleFormatContains(kAudioUnitList[i]->iformats, iformat) &&
            SampleFormatContains(kAudioUnitList[i]->oformats, oformat)) {
            return kAudioUnitList[i];
        }
    }
    return NULL;
}

struct AudioConverter : public MediaDevice {
    AudioFormat                 oFormat;
    Vector<const MediaUnit *>   mUnits;
    Vector<MediaUnitContext>    mInstances;
    sp<MediaFrame>              mOutput;
    
    AudioConverter() : MediaDevice() { }
    
    virtual ~AudioConverter() {
        for (size_t i = 0; i < mUnits.size(); ++i) {
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
        if (!mOutput.isNIL()) {
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
        for (size_t i = 0; i < mUnits.size(); ++i) {
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
        return NULL;
    }
    return ac;
}

__END_NAMESPACE_MPX




