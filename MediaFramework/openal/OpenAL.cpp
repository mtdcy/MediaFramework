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


// File:    OpenAL.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "OpenAL"
//#define LOG_NDEBUG 0
#include "MediaTypes.h"
#include "MediaOut.h"

#ifdef __APPLE__
#include <OpenAL/OpenAL.h>
#define MAC_OPENAL_EXT  // TODO
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#if LOG_NDEBUG == 0
#define CHECK_AL_ERROR() do {       \
    ALCenum err = alGetError();     \
    if (err != AL_NO_ERROR)         \
    ERROR("error %#x", err);        \
} while(0)
#else
#define CHECK_AL_ERROR()
#endif

__BEGIN_NAMESPACE_MPX

// https://www.openal.org/documentation/OpenAL_Programmers_Guide.pdf
/**
 * OpenAL only takes s16 stereo interleaved pcm samples
 */
struct OpenALContext : public SharedObject {
    AudioFormat     mAudioFormat;
    ALCdevice *     mDevice;
    ALCcontext *    mContext;
    ALuint          mSource;
    
    OpenALContext() : mDevice(NULL), mContext(NULL), mSource(0) { }
};

static MediaError flushOpenAL(sp<OpenALContext>& openAL);
static sp<OpenALContext> initOpenALContext(const AudioFormat& audio) {
    DEBUG("init");
    sp<OpenALContext> openAL = new OpenALContext;
    openAL->mAudioFormat.format     = kSampleFormatS16;   // OpenAL only support s8 & s16
    openAL->mAudioFormat.channels   = audio.channels > 1 ? 2 : 1;
    openAL->mAudioFormat.freq       = audio.freq;
    if (openAL->mAudioFormat.channels > 1) {
        // -> packed sample format
        // OpenAL only support interleaved/packed samples
        openAL->mAudioFormat.format = GetSimilarSampleFormat(openAL->mAudioFormat.format);
    }
    
    openAL->mDevice = alcOpenDevice(NULL);
    CHECK_AL_ERROR();
    if (openAL->mDevice == NULL) {
        ERROR("init OpenAL device failed");
        return NULL;
    }
    
    // device enumeration
    if (alcIsExtensionPresent(openAL->mDevice, "ALC_ENUMERATION_EXT") == AL_TRUE) {
        const ALCchar * devices = alcGetString(openAL->mDevice, ALC_DEVICE_SPECIFIER);
        const ALCchar * device = devices, *next = devices + 1;
        size_t len = 0;

        INFO("Devices list:");
        INFO("----------");
        while (device && *device != '\0' && next && *next != '\0') {
                INFO("%s", device);
                len = strlen(device);
                device += (len + 1);
                next += (len + 2);
        }
        INFO("----------");
    }
    CHECK_AL_ERROR();

    openAL->mContext = alcCreateContext(openAL->mDevice, NULL);
    CHECK_AL_ERROR();
    ALboolean success = alcMakeContextCurrent(openAL->mContext);
    CHECK_AL_ERROR();
    if (!success) {
        ERROR("alcMakeContextCurrent failed");
        return NULL;
    }
    
    flushOpenAL(openAL);
    
    // FIXME:
    // alcProcessContext is asynchronous, it takes some time to complete
    // the operation internally. If we start playing immediately after
    // init, playback will not start immediately, and will cause the buffer
    // queue to become larger.
    // In short, the client does not know when playback will start.
    alcProcessContext(openAL->mContext);
    CHECK_AL_ERROR();
    
    return openAL;
}

static MediaError flushOpenAL(sp<OpenALContext>& openAL) {
    if (openAL->mSource) {
        alSourceStop(openAL->mSource);
        CHECK_AL_ERROR();
        alDeleteSources((ALsizei)1, &openAL->mSource);
        CHECK_AL_ERROR();
    }
    alGenSources((ALsizei)1, &openAL->mSource);
    CHECK_AL_ERROR();
    
    alSourcef(openAL->mSource, AL_PITCH, 1);
    CHECK_AL_ERROR();
    alSourcef(openAL->mSource, AL_GAIN, 1);
    CHECK_AL_ERROR();
    alSource3f(openAL->mSource, AL_POSITION, 0, 0, 0);
    CHECK_AL_ERROR();
    alSource3f(openAL->mSource, AL_VELOCITY, 0, 0, 0);
    CHECK_AL_ERROR();
    alSourcei(openAL->mSource, AL_LOOPING, AL_FALSE);
    CHECK_AL_ERROR();
    
    return kMediaNoError;
}

static void deinitOpenAL(sp<OpenALContext>& openAL) {
    alcSuspendContext(openAL->mContext);
    CHECK_AL_ERROR();
    
    ALint queued;
    ALint processed;
    alGetSourcei(openAL->mSource, AL_BUFFERS_QUEUED, &queued);
    alGetSourcei(openAL->mSource, AL_BUFFERS_PROCESSED, &processed);
    CHECK_AL_ERROR();
    if (queued != processed) {
        ERROR("deinit: %zu buffers queued but only %zu buffers processed", queued, processed);
    }
    INFO("delete %zu buffers", (size_t)processed);
    while (processed-- > 0) {
        ALuint buffer;
        alSourceUnqueueBuffers(openAL->mSource, 1, &buffer);
        alDeleteBuffers(processed, &buffer);
    }
    alDeleteSources((ALsizei)1, &openAL->mSource);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(openAL->mContext);
    alcCloseDevice(openAL->mDevice);
}

static MediaError playFrame(const sp<OpenALContext>& openAL, const sp<MediaFrame>& frame) {
    const int64_t now = SystemTimeUs();
    ALint state;
    alGetSourcei(openAL->mSource, AL_SOURCE_STATE, &state);
    CHECK_AL_ERROR();
    
    if (frame.isNIL()) {
        DEBUG("eos");
        while (state == AL_PLAYING) {
            DEBUG("wait for al stopping, state %#x", state);
            // FIXME: find a better way to do this
            SleepTimeMs(100);
            alGetSourcei(openAL->mSource, AL_SOURCE_STATE, &state);
            CHECK_AL_ERROR();
        }
        return kMediaNoError;
    }
    CHECK_EQ(openAL->mAudioFormat.format, frame->a.format);
    
    ALenum alFormat = frame->a.channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
    if (frame->a.format == kSampleFormatU8Packed) {
        alFormat = frame->a.channels == 2 ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;
    }
    
    ALint queued, processed;
    alGetSourcei(openAL->mSource, AL_BUFFERS_QUEUED, &queued);
    CHECK_AL_ERROR();
    alGetSourcei(openAL->mSource, AL_BUFFERS_PROCESSED, &processed);
    CHECK_AL_ERROR();
    DEBUG("queued %d, processed %d, state %#x", queued, processed, state);
    
    // underrun happens?
    if (queued && queued == processed) {
        WARN("underrun..., queued buffers %d vs processed buffers %d",
             queued, processed);
    }

    ALuint buffer;
    if (ABE_LIKELY(processed > 0)) {
        if (ABE_UNLIKELY(processed > 1)) {
            ALuint buffers[processed - 1];
            alSourceUnqueueBuffers(openAL->mSource, processed - 1, &buffers[0]);
            alDeleteBuffers(processed - 1, &buffers[0]);
            CHECK_AL_ERROR();
        }
        alSourceUnqueueBuffers(openAL->mSource, 1, &buffer);
    } else {
        alGenBuffers(1, &buffer);
    }
    CHECK_AL_ERROR();
    alBufferData(buffer,
                 alFormat,
                 (const ALvoid *)frame->planes[0].data,
                 (ALsizei)frame->planes[0].size,
                 (ALsizei)frame->a.freq);
    CHECK_AL_ERROR();
    
    alSourceQueueBuffers(openAL->mSource, 1, &buffer);
    CHECK_AL_ERROR();
    
    // the first frame or after underrun happens
    if (state != AL_PLAYING) {
        INFO("start open al source");
        alSourcePlay(openAL->mSource);
        CHECK_AL_ERROR();
    }
    
    return kMediaNoError;
}

struct OpenALOut : public MediaOut {
    sp<OpenALContext>   mOpenAL;
    
    OpenALOut() : MediaOut() { }
    
    virtual ~OpenALOut() {
        if (mOpenAL.isNIL()) return;
        deinitOpenAL(mOpenAL);
        mOpenAL.clear();
    }
    
    MediaError init(const sp<Message>& formats, const sp<Message>& options) {
        AudioFormat audio;
        audio.format = (eSampleFormat)formats->findInt32(kKeyFormat);
        audio.channels = formats->findInt32(kKeyChannels);
        audio.freq = formats->findInt32(kKeySampleRate);
        
        mOpenAL = initOpenALContext(audio);
        if (mOpenAL.isNIL()) return kMediaErrorBadFormat;
        return kMediaNoError;
    }
    
    virtual sp<Message> formats() const {
        sp<Message> format = new Message;
        format->setInt32(kKeyFormat, mOpenAL->mAudioFormat.format);
        format->setInt32(kKeyChannels, mOpenAL->mAudioFormat.channels);
        format->setInt32(kKeySampleRate, mOpenAL->mAudioFormat.freq);
        format->setInt32(kKeyMode, kModeNonBlock);
        format->setInt32(kKeyLatency, 0);   // FIXME
        return format;
    }
    
    virtual MediaError configure(const sp<Message>& options) {
        if (options->contains(kKeyPause)) {
            int32_t pause = options->findInt32(kKeyPause);
            if (pause) {
                alSourcePause(mOpenAL->mSource);
            }
            return kMediaNoError;
        }
        return kMediaErrorInvalidOperation;
    }
    
    virtual MediaError write(const sp<MediaFrame>& frame) {
        return playFrame(mOpenAL, frame);
    }
    
    virtual MediaError flush() {
        return flushOpenAL(mOpenAL);
    }
};

sp<MediaOut> CreateOpenALOut(const sp<Message>& formats, const sp<Message>& options) {
    sp<OpenALOut> out = new OpenALOut;
    if (out->init(formats, options) != kMediaNoError)
        return NULL;
    return out;
}

__END_NAMESPACE_MPX
