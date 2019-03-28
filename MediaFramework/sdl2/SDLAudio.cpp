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


// File:    SDLAudio.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//


#define LOG_TAG "SDLAudio"
//#define LOG_NDEBUG 0
#include "SDLAudio.h"

#include <SDL.h>


struct {
    eSampleFormat   a;
    SDL_AudioFormat b;
} kSampleMap[] = {
    {kSampleFormatS16,      AUDIO_S16SYS},
    {kSampleFormatS32,      AUDIO_S32SYS},
    {kSampleFormatFLT,      AUDIO_F32SYS},
    // END OF LIST
    {kSampleFormatUnknown,  0}
};

static eSampleFormat get_sample_format(SDL_AudioFormat b) {
    for (size_t i = 0; kSampleMap[i].a != kSampleFormatUnknown; ++i) {
        if (kSampleMap[i].b == b) return kSampleMap[i].a;
    }
    FATAL("FIX MAP");
    return kSampleFormatUnknown;
};

static SDL_AudioFormat get_sdl_sample_format(eSampleFormat a) {
    for (size_t i = 0; kSampleMap[i].a != kSampleFormatUnknown; ++i) {
        if (kSampleMap[i].a == a) return kSampleMap[i].b;
    }
    FATAL("FIX MAP");
    return 0;
}

__BEGIN_NAMESPACE_MPX

SDLAudio::SDLAudio() :
    MediaOut(),
    mInitByUs(false), 
    mPendingFrame(0), mFlushing(false), mSilence(0)
{ }

status_t SDLAudio::prepare(const Message& options) {
    if (SDL_WasInit(SDL_INIT_AUDIO) == SDL_INIT_AUDIO) {
        INFO("sdl audio has been initialized");
    } else {
        mInitByUs = true;
        SDL_InitSubSystem(SDL_INIT_AUDIO);
    }

    eSampleFormat format = (eSampleFormat)options.findInt32(kKeyFormat);
    uint32_t freq = options.findInt32(kKeySampleRate);
    uint32_t chan = options.findInt32(kKeyChannels);

    return openDevice(format, freq, chan);
}

status_t SDLAudio::openDevice(eSampleFormat format, uint32_t freq, uint32_t channels) {
    INFO("open device: %d %d %d", format, freq, channels);
    SDL_AudioSpec wanted_spec, spec;

    // sdl only support s16.
    wanted_spec.channels    = channels;
    wanted_spec.freq        = freq;
    wanted_spec.format      = get_sdl_sample_format((eSampleFormat)format);
    wanted_spec.silence     = 0;
    wanted_spec.samples     = 2048;
    wanted_spec.callback    = onSDLCallback;
    wanted_spec.userdata    = this;

    if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
        ERROR("SDL_OpenAudio failed. %s", SDL_GetError());
        return BAD_VALUE;
    }

    mSampleFormat   = get_sample_format(spec.format);
    mSampleRate     = spec.freq;
    mChannels       = spec.channels;

    INFO("SDL audio init done.");
    return OK;
}

SDLAudio::~SDLAudio() {
    INFO("SDL audio release.");
    {
        AutoLock _l(mLock);
        mFlushing = true;
        mWait.signal();
    }
    SDL_CloseAudio();

    if (mInitByUs) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

status_t SDLAudio::status() const {
    return mFormat != NULL ? OK : NO_INIT;
}

Message SDLAudio::formats() const {
    Message info;
    info.setInt32(kKeyFormat, mSampleFormat);
    info.setInt32(kKeySampleRate, mSampleRate);
    info.setInt32(kKeyChannels, mChannels);
    return info;
}

status_t SDLAudio::configure(const Message& options) {
    if (options.contains(kKeyPause)) {
        bool pause = options.findInt32(kKeyPause);

        AutoLock _l(mLock);
        if (SDL_GetAudioStatus() == SDL_AUDIO_PLAYING && pause) {
            mFlushing = true;
            mWait.signal();
        }
        return OK;
    }

    return INVALID_OPERATION;
}

String SDLAudio::string() const {
    return mFormat != NULL ? mFormat->string() : "SDLAudio";
}

status_t SDLAudio::flush() {
    AutoLock _l(mLock);
    INFO("flushing in state %d...", SDL_GetAudioStatus());

    if (SDL_GetAudioStatus() == SDL_AUDIO_PLAYING) {
        mFlushing = true;
        mWait.signal();
        mWait.wait(mLock);
    }
    return OK;
}

template <class TYPE>
sp<Buffer> interleave(const sp<MediaFrame>& frame) {
    TYPE * p[MEDIA_FRAME_NB_PLANES];
    for (size_t i = 0; i < frame->a.channels; ++i)
        p[i] = (TYPE*)frame->planes[i].data;

    size_t n = frame->planes[0].size * frame->a.channels;
    sp<Buffer> out = new Buffer(n);
    TYPE * dest = (TYPE*)out->data();

    for (size_t i = 0; i < frame->a.samples; ++i)
        for (size_t j = 0; j < frame->a.channels; ++j)
            dest[frame->a.channels * i + j] = p[j][i];

    out->step(n);

    return out;
}

status_t SDLAudio::write(const sp<MediaFrame>& input) {
    DEBUG("write");
    if (input == NULL) return flush();

    if (input->a.format != mSampleFormat ||
            input->a.channels != mChannels ||
            input->a.freq != mSampleRate) {
        SDL_CloseAudio();
        openDevice(input->a.format, input->a.freq, input->a.channels);
    }

    AutoLock _l(mLock);

    //DEBUG("write %s", input->string().c_str());
    if (SDL_GetAudioStatus() != SDL_AUDIO_PLAYING) {
        INFO("SDL_PauseAudio 0");
        SDL_PauseAudio(0);
        //mSilence = 1;
    }

    if (input->planes[1].data != NULL) {
        switch (mSampleFormat) {
            case kSampleFormatS16:
                mPendingFrame   = interleave<int16_t>(input);
                break;
            case kSampleFormatS32:
                mPendingFrame   = interleave<int32_t>(input);
                break;
            case kSampleFormatFLT:
                mPendingFrame   = interleave<float>(input);
                break;
            default:
                FATAL("FIXME");
                break;
        }
    } else {
        //INFO("frame %zu %d %d %d", input->planes[0].size, input->a.channels, input->a.freq, input->a.samples);
        mPendingFrame   = new Buffer((const char *)input->planes[0].data,
                input->planes[0].size);
    }

    mWait.signal();
    while (mPendingFrame->ready() > 0) {
        DEBUG("wait for eat frame");
        mWait.wait(mLock);
    }
    mPendingFrame.clear();

    return OK;
}

void SDLAudio::onSDLCallback(void *opaque, uint8_t *stream, int len) {
    SDLAudio *sdl = static_cast<SDLAudio*>(opaque);
    sdl->eatFrame(stream, len);
}

void SDLAudio::eatFrame(uint8_t *buffer, size_t len) {
    DEBUG("eatFrame %p %d", buffer, len);

    AutoLock _l(mLock);
    if (mSilence) {
        memset(buffer, 0, len);
        --mSilence;
        return;
    }


    while (len > 0 && !mFlushing) {
        if (mPendingFrame == NULL || mPendingFrame->ready() == 0) {
            DEBUG("wait for input");
            mWait.signal();
            mWait.wait(mLock);
            continue;
        }

        size_t copyBytes = len;
        if (mPendingFrame->ready() < copyBytes) {
            copyBytes = mPendingFrame->ready();
        }
        memcpy(buffer, mPendingFrame->data(), copyBytes);
        mPendingFrame->skip(copyBytes);
        DEBUG("mPendingFrame %s", mPendingFrame->string().c_str());

        len     -= copyBytes;
        buffer  += copyBytes;
    }

    if (mFlushing) {
        INFO("flush complete");
        memset(buffer, 0, len);
        mFlushing = false;
        SDL_PauseAudio(1);
        INFO("SDL_PauseAudio 1");
        mWait.signal();
    }
}
__END_NAMESPACE_MPX
