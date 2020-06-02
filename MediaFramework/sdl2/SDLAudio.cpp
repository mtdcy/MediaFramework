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
#include "MediaOut.h"
#include "AudioResampler.h"

#include <SDL.h>

#define FORCE_FREQ  48000   // for testing
#define FORCE_FMT   kSampleFormatS32

__BEGIN_NAMESPACE_MPX

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

static FORCE_INLINE eSampleFormat get_sample_format(SDL_AudioFormat b) {
    for (size_t i = 0; kSampleMap[i].a != kSampleFormatUnknown; ++i) {
        if (kSampleMap[i].b == b) return kSampleMap[i].a;
    }
    FATAL("FIX MAP");
    return kSampleFormatUnknown;
};

static FORCE_INLINE SDL_AudioFormat get_sdl_sample_format(eSampleFormat a) {
    for (size_t i = 0; kSampleMap[i].a != kSampleFormatUnknown; ++i) {
        if (kSampleMap[i].a == a) return kSampleMap[i].b;
    }
    FATAL("FIX MAP");
    return 0;
}

struct SDLAudioContext : public SharedObject {
    bool                    mInitByUs;
    sp<Message>             mFormat;
    AudioFormat             mAudioFormat;
    
    mutable Mutex           mLock;
    Condition               mWait;
    sp<Buffer>              mPendingFrame;
    size_t                  mBytesLeft;
    bool                    mFlushing;
    size_t                  mSilence;

    SDLAudioContext() : SharedObject(), mInitByUs(false),
    mPendingFrame(0), mFlushing(false), mSilence(0) { }
};

static void SDLAudioCallback(void *opaque, uint8_t *stream, int len);
static FORCE_INLINE sp<SDLAudioContext> openDevice(const AudioFormat& format) {
    INFO("open device: %d %d %d", format.format, format.freq, format.channels);
    sp<SDLAudioContext> sdl = new SDLAudioContext;

    if (SDL_WasInit(SDL_INIT_AUDIO) == SDL_INIT_AUDIO) {
        INFO("sdl audio has been initialized");
    } else {
        sdl->mInitByUs = true;
        SDL_InitSubSystem(SDL_INIT_AUDIO);
    }

    SDL_AudioSpec wanted_spec, spec;

    // sdl only support s16.
    wanted_spec.channels    = format.channels;
    wanted_spec.freq        = format.freq;
    wanted_spec.format      = get_sdl_sample_format((eSampleFormat)format.format);
    wanted_spec.silence     = 0;
    wanted_spec.samples     = 2048;
    wanted_spec.callback    = SDLAudioCallback;
    wanted_spec.userdata    = sdl.get();

    if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
        ERROR("SDL_OpenAudio failed. %s", SDL_GetError());
        return NULL;
    }

    sdl->mAudioFormat.format    = get_sample_format(spec.format);
    sdl->mAudioFormat.freq      = spec.freq;
    sdl->mAudioFormat.channels  = spec.channels;

    INFO("SDL audio init done.");
    return sdl;
}

static FORCE_INLINE void closeDevice(sp<SDLAudioContext>& sdl) {
    INFO("SDL audio release.");
    {
        AutoLock _l(sdl->mLock);
        sdl->mFlushing = true;
        sdl->mWait.signal();
    }
    SDL_CloseAudio();

    if (sdl->mInitByUs) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

static void SDLAudioCallback(void *opaque, uint8_t *buffer, int len) {
    sp<SDLAudioContext> sdl = static_cast<SDLAudioContext*>(opaque);

    DEBUG("eatFrame %p %d", buffer, len);

    AutoLock _l(sdl->mLock);
    if (sdl->mSilence) {
        memset(buffer, 0, len);
        --sdl->mSilence;
        return;
    }

    while (len > 0 && !sdl->mFlushing) {
        if (sdl->mPendingFrame == NULL || sdl->mPendingFrame->ready() == 0) {
            DEBUG("wait for input");
            sdl->mWait.signal();
            sdl->mWait.wait(sdl->mLock);
            continue;
        }

        size_t copyBytes = len;
        if (sdl->mPendingFrame->ready() < copyBytes) {
            copyBytes = sdl->mPendingFrame->ready();
        }
        memcpy(buffer, sdl->mPendingFrame->data(), copyBytes);
        sdl->mPendingFrame->skip(copyBytes);
        DEBUG("mPendingFrame %s", mPendingFrame->string().c_str());

        len     -= copyBytes;
        buffer  += copyBytes;
    }

    if (sdl->mFlushing) {
        INFO("flush complete");
        memset(buffer, 0, len);
        sdl->mFlushing = false;
        SDL_PauseAudio(1);
        INFO("SDL_PauseAudio 1");
        sdl->mWait.signal();
    }
}

template <class TYPE> FORCE_INLINE sp<Buffer> interleave(const sp<MediaFrame>& frame) {
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

struct SDLAudio : public MediaOut {
    sp<SDLAudioContext>     mSDL;
    Object<AudioResampler>  mResampler;

    FORCE_INLINE SDLAudio() : MediaOut(), mSDL(NULL) { }
    FORCE_INLINE virtual ~SDLAudio() {
        if (mSDL != NULL) closeDevice(mSDL);
        mSDL.clear();
    }
    
    MediaError prepare(const sp<Message>& format, const sp<Message>& options) {
        AudioFormat a;
        
        a.format    = (eSampleFormat)format->findInt32(kKeyFormat);
        a.freq      = format->findInt32(kKeySampleRate);
        a.channels  = format->findInt32(kKeyChannels);
#ifdef FORCE_FREQ
        AudioFormat _a  = a;
        _a.freq         = FORCE_FREQ;
#ifdef FORCE_FMT
        _a.format       = FORCE_FMT;
#endif
        
        mSDL = openDevice(_a);
        if (_a != a) {
            sp<Message> options = new Message;
            mResampler = AudioResampler::Create(a, _a, options);
        }
#else
        mSDL = openDevice(format);
#endif
        return mSDL != NULL ? kMediaNoError : kMediaErrorBadFormat;
    }

    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyFormat, mSDL->mAudioFormat.format);
        info->setInt32(kKeySampleRate, mSDL->mAudioFormat.freq);
        info->setInt32(kKeyChannels, mSDL->mAudioFormat.channels);
        return info;
    }
    virtual MediaError configure(const sp<Message>& options) {
        if (options->contains(kKeyPause)) {
            bool pause = options->findInt32(kKeyPause);

            AutoLock _l(mSDL->mLock);
            if (SDL_GetAudioStatus() == SDL_AUDIO_PLAYING && pause) {
                mSDL->mFlushing = true;
                mSDL->mWait.signal();
            }
            return kMediaNoError;
        }

        return kMediaErrorInvalidOperation;
    }

    virtual MediaError write(const sp<MediaFrame>& input) {
        DEBUG("write");
        if (input == NULL) return flush();

        if (mResampler == NULL && input->a != mSDL->mAudioFormat) {
            closeDevice(mSDL);
            mSDL = openDevice(input->a);
        }
        
        sp<MediaFrame> frame = input;
        if (mResampler != NULL) {
            frame = mResampler->resample(input);
        }

        AutoLock _l(mSDL->mLock);

        //DEBUG("write %s", input->string().c_str());
        if (SDL_GetAudioStatus() != SDL_AUDIO_PLAYING) {
            INFO("SDL_PauseAudio 0");
            SDL_PauseAudio(0);
            //mSilence = 1;
        }

        if (frame->planes[1].data != NULL) {
            switch (mSDL->mAudioFormat.format) {
                case kSampleFormatS16:
                    mSDL->mPendingFrame   = interleave<int16_t>(frame);
                    break;
                case kSampleFormatS32:
                    mSDL->mPendingFrame   = interleave<int32_t>(frame);
                    break;
                case kSampleFormatFLT:
                    mSDL->mPendingFrame   = interleave<float>(frame);
                    break;
                default:
                    FATAL("FIXME");
                    break;
            }
        } else {
            //INFO("frame %zu %d %d %d", input->planes[0].size, input->a.channels, input->a.freq, input->a.samples);
            mSDL->mPendingFrame   = new Buffer((const char *)frame->planes[0].data,
                    frame->planes[0].size);
        }

        mSDL->mWait.signal();
        while (mSDL->mPendingFrame->ready() > 0) {
            DEBUG("wait for eat frame");
            mSDL->mWait.wait(mSDL->mLock);
        }
        mSDL->mPendingFrame.clear();

        return kMediaNoError;
    }

    virtual MediaError flush() {
        AutoLock _l(mSDL->mLock);
        INFO("flushing in state %d...", SDL_GetAudioStatus());

        if (SDL_GetAudioStatus() == SDL_AUDIO_PLAYING) {
            mSDL->mFlushing = true;
            mSDL->mWait.signal();
            mSDL->mWait.wait(mSDL->mLock);
        }
        return kMediaNoError;
    }

};

sp<MediaOut> CreateSDLAudio(const sp<Message>& formats, const sp<Message>& options) {
    sp<SDLAudio> sdl = new SDLAudio;
    if (sdl->prepare(formats, options) == kMediaNoError)
        return sdl;
    return NULL;
}
__END_NAMESPACE_MPX
