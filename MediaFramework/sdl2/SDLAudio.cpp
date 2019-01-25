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

namespace mtdcy {

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

        SDL_AudioSpec wanted_spec, spec;

        // sdl only support s16.
        wanted_spec.channels    = options.findInt32(kKeyChannels);
        wanted_spec.freq        = options.findInt32(kKeySampleRate);
        wanted_spec.format      = get_sdl_sample_format((eSampleFormat)options.findInt32(kKeyFormat));
        wanted_spec.silence     = 0;
        wanted_spec.samples     = 2048;
        wanted_spec.callback    = onSDLCallback;
        wanted_spec.userdata    = this;

        if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
            ERROR("SDL_OpenAudio failed. %s", SDL_GetError());
            return BAD_VALUE;
        }

        sp<Message> format      = new Message;
        format->setInt32(kKeyFormat, get_sample_format(spec.format));
        format->setInt32(kKeyChannels, spec.channels);
        format->setInt32(kKeySampleRate, spec.freq);
        //format->setInt32(Media::BufferSize, spec.size); // FIXME
        // assume double buffer
        format->setInt32(kKeyLatency, 2 * (1000000LL * spec.samples) / spec.freq);
        
        INFO("sink %s", format->string().c_str());

        mFormat     = format;
        mSampleFormat = spec.format;
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
        return *mFormat;
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
        TYPE * p[8];
        for (size_t i = 0; i < frame->a.channels; ++i)
            p[i] = (TYPE*)frame->data[i]->data();
        
        size_t n = frame->data[0]->size() * frame->a.channels;
        sp<Buffer> out = new Buffer(n);
        TYPE * dest = (TYPE*)out->data();
        
        size_t samples = frame->data[0]->size() / sizeof(TYPE);
        for (size_t i = 0; i < samples; ++i)
            for (size_t j = 0; j < frame->a.channels; ++j)
                dest[frame->a.channels * i + j] = p[j][i];
        
        out->step(n);
        
        return out;
    }

    status_t SDLAudio::write(const sp<MediaFrame>& input) {
        DEBUG("write");
        if (input == NULL) return flush();
        
        AutoLock _l(mLock);

        //DEBUG("write %s", input->string().c_str());
        if (SDL_GetAudioStatus() != SDL_AUDIO_PLAYING) {
            INFO("SDL_PauseAudio 0");
            SDL_PauseAudio(0);
            //mSilence = 1;
        }
        
        if (input->data[1] != NULL) {
            switch (mSampleFormat) {
                case AUDIO_S16SYS:
                    mPendingFrame   = interleave<int16_t>(input);
                    break;
                case AUDIO_S32SYS:
                    mPendingFrame   = interleave<int32_t>(input);
                    break;
                case AUDIO_F32SYS:
                    mPendingFrame   = interleave<float>(input);
                    break;
                default:
                    FATAL("FIXME");
                    break;
            }
        } else {
            mPendingFrame   = input->data[0];
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
};

