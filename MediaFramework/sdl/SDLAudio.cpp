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
#define LOG_NDEBUG 0
#include "SDLAudio.h"

#include <SDL.h>

namespace mtdcy {

    SDLAudio::SDLAudio(const Message* wanted_format) :
        MediaOut(),
        mInitByUs(false), 
        mPendingFrame(0), mFlushing(false)
    {
        if (SDL_WasInit(SDL_INIT_AUDIO) == SDL_INIT_AUDIO) {
            INFO("sdl audio has been initialized");
        } else {
            mInitByUs = true;
            SDL_InitSubSystem(SDL_INIT_AUDIO);
        }

        SDL_AudioSpec wanted_spec, spec;

        // sdl only support s16.
        wanted_spec.channels    = wanted_format->findInt32(Media::Channels);
        wanted_spec.freq        = wanted_format->findInt32(Media::SampleRate);
        wanted_spec.format      = AUDIO_S16SYS;
        wanted_spec.silence     = 0;
        wanted_spec.samples     = 4096;
        wanted_spec.callback    = onSDLCallback;
        wanted_spec.userdata    = this;

        if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
            ERROR("SDL_OpenAudio failed. %s", SDL_GetError());
            return;
        }

        sp<Message> format      = new Message;
        format->setString(Media::Format,        Media::Codec::PCM);
        format->setInt32(Media::BitsPerSample,  16);
        format->setInt32(Media::Channels,       spec.channels);
        format->setInt32(Media::SampleRate,     spec.freq);
        format->setInt32(Media::BufferSize,
                spec.channels * spec.samples * sizeof(int16_t));

        mFormat     = format;
        INFO("SDL audio init done.");
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

    const Message& SDLAudio::formats() const {
        return *mFormat;
    }

    status_t SDLAudio::configure(const Message& options) {
        return INVALID_OPERATION;
    }

    String SDLAudio::string() const {
        return mFormat != NULL ? mFormat->string() : "SDLAudio";
    }

    status_t SDLAudio::flush() {
        return OK;
    }

    status_t SDLAudio::write(const sp<Buffer>& input) {
        AutoLock _l(mLock);
        if (input == NULL) {
            DEBUG("flushing...");
            mFlushing       = true;
            mWait.signal();
        } else {
            DEBUG("write %s", input->string().c_str());
            if (SDL_GetAudioStatus() != SDL_AUDIO_PLAYING) {
                SDL_PauseAudio(0);
            }
            mPendingFrame   = input;
            mWait.signal();
            while (mPendingFrame->ready() > 0) {
                DEBUG("wait for eat frame");
                mWait.wait(mLock);
            }
            mPendingFrame.clear();
        }
        return OK;
    }

    void SDLAudio::onSDLCallback(void *opaque, uint8_t *stream, int len) {
        SDLAudio *sdl = static_cast<SDLAudio*>(opaque);
        sdl->eatFrame(stream, len);
    }

    void SDLAudio::eatFrame(uint8_t *buffer, size_t len) {
        DEBUG("eatFrame %p %d", buffer, len);

        AutoLock _l(mLock);
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
            memset(buffer, 0, len);
            SDL_PauseAudio(1);
        }
        mWait.signal();
    }
};

