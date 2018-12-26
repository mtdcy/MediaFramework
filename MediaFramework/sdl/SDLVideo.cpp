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


// File:    SDLVideo.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//


#define LOG_TAG "SDLVideo"
#define LOG_NDEBUG 0
#include "SDLVideo.h"

#include <SDL.h>

namespace mtdcy {

    SDLVideo::SDLVideo(const Message* wanted_format) :
        MediaOut(),
        mInitByUs(false), 
        mWidth(0), mHeight(0)
    {
        if (SDL_WasInit(SDL_INIT_VIDEO) == SDL_INIT_VIDEO) {
            INFO("sdl audio has been initialized");
        } else {
            mInitByUs = true;
            SDL_InitSubSystem(SDL_INIT_VIDEO);
        }

        mWidth  = wanted_format->findInt32(Media::Width);
        mHeight = wanted_format->findInt32(Media::Height);

        mSurface = SDL_SetVideoMode(mWidth, mHeight, 24, SDL_ANYFORMAT);
        if (mSurface == NULL) {
            ERROR("SDL_SetVideoMode failed %s", SDL_GetError());
            return;
        }

        // YUV420P
        mOverlay = SDL_CreateYUVOverlay(mWidth, mHeight, SDL_YV12_OVERLAY, mSurface);
        if (mOverlay == NULL) {
            ERROR("SDL_CreateYUVOverlay failed %s", SDL_GetError());
            return;
        }

        sp<Message> format      = new Message;
        format->setString(Media::Format,        Media::Codec::YUV420);

        mFormat     = format;
        INFO("SDL audio init done.");
    }

    SDLVideo::~SDLVideo() {
        INFO("SDL audio release.");

        if (mOverlay != NULL)
            SDL_FreeYUVOverlay(mOverlay);

        if (mSurface != NULL)
            SDL_FreeSurface(mSurface);

        if (mInitByUs) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
    }

    status_t SDLVideo::status() const {
        return mFormat != NULL ? OK : NO_INIT;
    }

    const Message& SDLVideo::formats() const {
        return *mFormat;
    }

    status_t SDLVideo::configure(const Message& options) {
        return INVALID_OPERATION;
    }

    String SDLVideo::string() const {
        return mFormat != NULL ? mFormat->string() : "SDLVideo";
    }

    status_t SDLVideo::flush() {
        return OK;
    }

    status_t SDLVideo::write(const sp<Buffer>& input) {
        if (input == NULL) {
            DEBUG("flushing...");
            return OK;
        } 

        DEBUG("write %s", input->string().c_str());

        SDL_LockYUVOverlay(mOverlay);

        uint32_t Ysize = mWidth * mHeight;
        uint32_t UVsize = (mWidth * mHeight) / 4;
        memcpy(mOverlay->pixels[0], input->data(), Ysize);
        memcpy(mOverlay->pixels[2], input->data() + Ysize, UVsize);
        memcpy(mOverlay->pixels[1], input->data() + Ysize + UVsize, UVsize);

        SDL_UnlockYUVOverlay(mOverlay);

        SDL_Rect rect;
        rect.x  = 0;
        rect.y  = 0;
        rect.w  = mWidth;
        rect.h  = mHeight;
        SDL_DisplayYUVOverlay(mOverlay, &rect);
        return OK;
    }
};

