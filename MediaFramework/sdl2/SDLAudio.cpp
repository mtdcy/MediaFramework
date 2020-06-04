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
#define NB_SAMPLES  2048    // buffer sample count

__BEGIN_NAMESPACE_MPX

struct {
    eSampleFormat   a;
    SDL_AudioFormat b;
} kSampleMap[] = {
    {kSampleFormatU8,       AUDIO_U8},
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
    bool                    mInputEOS;
    sp<MediaFrame>          mPendingFrame;
    size_t                  mBytesRead;
    bool                    mFlushing;

    SDLAudioContext() : SharedObject(), mInitByUs(false),
    mInputEOS(false), mBytesRead(0), mFlushing(false) { }
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
    wanted_spec.samples     = NB_SAMPLES;
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
        sdl->mWait.broadcast();
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
    
    while (len && !sdl->mFlushing) {
        if (sdl->mPendingFrame.isNIL()) {
            if (sdl->mInputEOS) {
                INFO("End Of Audio...");
                memset(buffer, 0, len);
                SDL_PauseAudio(1);
                break;
            }
            sdl->mWait.signal();
            sdl->mWait.wait(sdl->mLock);
            continue;
        }
        
        size_t copy = len;
        size_t left = sdl->mPendingFrame->planes[0].size - sdl->mBytesRead;
        if (copy > left) copy = left;
        
        memcpy(buffer, sdl->mPendingFrame->planes[0].data + sdl->mBytesRead, copy);
        sdl->mBytesRead     += copy;
        buffer              += copy;
        len                 -= copy;
        
        if (sdl->mBytesRead == sdl->mPendingFrame->planes[0].size) {
            sdl->mPendingFrame.clear();
            sdl->mWait.signal();
        }
    }
    
    // play silence until close device
    if (sdl->mFlushing) {
        INFO("flush complete");
        memset(buffer, 0, len);
        sdl->mFlushing = false;
        SDL_PauseAudio(1);
        INFO("SDL_PauseAudio 1");
        sdl->mWait.signal();
    }
}

struct PackedAudioFrame : public MediaFrame {
    sp<Buffer> data;
    PackedAudioFrame(size_t n) : data(new Buffer(n)) {
        planes[0].data      = (uint8_t*)data->data();
        planes[0].size      = n;
        for (size_t i = 1; i < MEDIA_FRAME_NB_PLANES; ++i) {
            planes[i].data  = NULL;
            planes[i].size  = 0;
        }
    }
};

template <class TYPE> FORCE_INLINE sp<MediaFrame> interleave(const sp<MediaFrame>& frame) {
    sp<MediaFrame> out = new PackedAudioFrame(frame->planes[0].size * frame->a.channels);
    out->a = frame->a;
    
    TYPE * p[MEDIA_FRAME_NB_PLANES];
    for (size_t i = 0; i < frame->a.channels; ++i)
        p[i] = (TYPE*)frame->planes[i].data;

    TYPE * dest = (TYPE*)out->planes[0].data;
    for (size_t i = 0; i < frame->a.samples; ++i)
        for (size_t j = 0; j < frame->a.channels; ++j)
            dest[frame->a.channels * i + j] = p[j][i];

    return out;
}

struct SDLAudio : public MediaOut {
    sp<SDLAudioContext>     mSDL;

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
        mSDL = openDevice(a);
        return mSDL != NULL ? kMediaNoError : kMediaErrorBadFormat;
    }

    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyFormat, mSDL->mAudioFormat.format);
        info->setInt32(kKeySampleRate, mSDL->mAudioFormat.freq);
        info->setInt32(kKeyChannels, mSDL->mAudioFormat.channels);
        info->setInt32(kKeyLatency, 2 * (1000000LL * NB_SAMPLES) / mSDL->mAudioFormat.freq);
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
        AutoLock _l(mSDL->mLock);
        if (input == NULL) {
            mSDL->mInputEOS = true;
            mSDL->mWait.signal();
            return kMediaNoError;
        }

        //DEBUG("write %s", input->string().c_str());
        if (SDL_GetAudioStatus() != SDL_AUDIO_PLAYING) {
            INFO("SDL_PauseAudio 0");
            SDL_PauseAudio(0);
        }
        
        // wait until pending frame finished
        while (!mSDL->mPendingFrame.isNIL()) {
            DEBUG("wait for callback");
            mSDL->mWait.wait(mSDL->mLock);
        }

        if (input->planes[1].data != NULL) {
            switch (mSDL->mAudioFormat.format) {
                case kSampleFormatS16:
                    mSDL->mPendingFrame   = interleave<int16_t>(input);
                    break;
                case kSampleFormatS32:
                    mSDL->mPendingFrame   = interleave<int32_t>(input);
                    break;
                case kSampleFormatFLT:
                    mSDL->mPendingFrame   = interleave<float>(input);
                    break;
                default:
                    FATAL("FIXME");
                    break;
            }
        } else {
            mSDL->mPendingFrame = input;
        }
        mSDL->mBytesRead    = 0;

        // wake up callback
        mSDL->mWait.signal();

        return kMediaNoError;
    }

    virtual MediaError flush() {
        AutoLock _l(mSDL->mLock);
        INFO("flushing in state %d...", SDL_GetAudioStatus());

        mSDL->mInputEOS = false;
        mSDL->mPendingFrame.clear();
        
        // stop callback
        if (SDL_GetAudioStatus() == SDL_AUDIO_PLAYING) {
            mSDL->mFlushing = true;
            mSDL->mWait.signal();
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
