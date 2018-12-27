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
//#define LOG_NDEBUG 0
#include "SDLVideo.h"

#include <SDL.h>

#ifdef __APPLE__
#define PREFERRED_PIXEL_FORMAT  kPixelFormatNV12
#else
#define PREFERRED_PIXEL_FORMAT  kPixelFormatYUV420P
#endif

namespace mtdcy {
    
    static uint32_t SDL_PIXELFORMAT(ePixelFormat format) {
        if (format == kPixelFormatYUV420P) {
            INFO("yuv420P");
            return SDL_PIXELFORMAT_IYUV;
        } else if (format == kPixelFormatNV12) {
            INFO("NV12");
            return SDL_PIXELFORMAT_NV12;
        }
        WARN("unsupported format %#x", format);
        return SDL_PIXELFORMAT_UNKNOWN;
    }
    
    struct SDLVideo::SDLContext {
        SDLContext() : mWindow(NULL), mRenderer(NULL), mTexture(NULL) { }
        SDL_Window      *mWindow;   // SDL_Window
        SDL_Renderer    *mRenderer; // SDL_Renderer
        SDL_Texture     *mTexture;  // SDL_Texture
    };

    SDLVideo::SDLVideo(const Message& wanted_format) :
        MediaOut(),
        mInitByUs(false),
        mSDLContext(new SDLContext),
        mPixFmt(PREFERRED_PIXEL_FORMAT),
        mWidth(0), mHeight(0)
    {
        INFO("SDL2 video <= %s", wanted_format.string().c_str());
        mSDLContext->mWindow = static_cast<SDL_Window*>(
                wanted_format.findPointer("SDL_Window"));
        CHECK_NULL(mSDLContext->mWindow);

        uint32_t winFlags = SDL_GetWindowFlags(mSDLContext->mWindow);

        if (SDL_WasInit(SDL_INIT_VIDEO) == SDL_INIT_VIDEO) {
            INFO("sdl video has been initialized");
        } else {
            mInitByUs = true;
            SDL_InitSubSystem(SDL_INIT_VIDEO);
        }

        mWidth  = wanted_format.findInt32(kKeyWidth);
        mHeight = wanted_format.findInt32(kKeyHeight);
        mPixFmt = (ePixelFormat)wanted_format.findInt32(kKeyFormat, PREFERRED_PIXEL_FORMAT);
        if (SDL_PIXELFORMAT(mPixFmt) == kPixelFormatUnknown) {
            INFO("unsupported pixel format %#x", mPixFmt);
            mPixFmt = PREFERRED_PIXEL_FORMAT;
        }
#if 0
        // resize window
        if (winFlags & SDL_WINDOW_RESIZABLE) {
            int w, h;
            SDL_GetWindowSize(mWindow, &w, &h);
            if (mWidth < w || mHeight < h) {
                SDL_SetWindowSize(mWindow, mWidth, mHeight);
            }
        }
#endif

        if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best")) {
            ERROR("SDL_SetHint failed. %s", SDL_GetError());
        }
        SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

#if 1
        uint32_t renderFlags = SDL_RENDERER_PRESENTVSYNC;
        if (winFlags & SDL_WINDOW_OPENGL) {
            INFO("OpenGL window");
            renderFlags |= SDL_RENDERER_ACCELERATED;
        }
#endif
        mSDLContext->mRenderer = SDL_CreateRenderer(mSDLContext->mWindow, -1, renderFlags);
        if (mSDLContext->mRenderer == NULL) {
            ERROR("SDL_CreateRenderer failed. %s", SDL_GetError());
            return;
        }
#if LOG_NDEBUG == 0
        {
            SDL_RendererInfo info;
            SDL_GetRendererInfo(mRenderer, &info);
            DEBUG("%s, flags %#x, %" PRIu32 ", %" PRId32 ", %" PRId32,
                    info.name, info.flags, info.num_texture_formats,
                    info.max_texture_width, info.max_texture_height);

            int w, h;
            SDL_GetRendererOutputSize(mRenderer, &w, &h);
            DEBUG("%d x %d", w, h);
        }
#endif

        
#if LOG_NDEBUG == 0
        mTexture = SDL_CreateTexture(mRenderer, 
                SDL_PIXELFORMAT_IYUV, 
                SDL_TEXTUREACCESS_STREAMING, 
                mWidth,
                mHeight);
        if (mTexture == NULL) {
            ERROR("SDL_CreateTexture failed. %s", SDL_GetError());
            return;
        }


        uint32_t fmt;
        int access, w, h;
        SDL_QueryTexture(mTexture, &fmt, &access, &w, &h);
        CHECK_EQ(fmt, (uint32_t)SDL_PIXELFORMAT_IYUV);
        CHECK_EQ(access, (int)SDL_TEXTUREACCESS_STREAMING);
        CHECK_EQ(w, (int)mWidth);
        CHECK_EQ(h, (int)mHeight);
#endif 

        sp<Message> format      = new Message;
        format->setInt32(kKeyFormat, mPixFmt);
        // FIXME:
        format->setInt32(kKeyLatency, 0);

        mFormat     = format;
        INFO("SDL video init done. %s", format->string().c_str());
    }

    SDLVideo::~SDLVideo() {
        INFO("SDL video release.");

        if (mSDLContext->mTexture != NULL) {
            SDL_DestroyTexture(mSDLContext->mTexture);
        }

        if (mSDLContext->mRenderer != NULL) {
            SDL_DestroyRenderer(mSDLContext->mRenderer);
        }

        if (mInitByUs) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
    }

    status_t SDLVideo::status() const {
        return mFormat != NULL ? OK : NO_INIT;
    }

    Message SDLVideo::formats() const {
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

    status_t SDLVideo::write(const sp<MediaFrame>& input) {
        if (input == NULL) {
            DEBUG("flushing...");
            return OK;
        }
        
        int32_t w = input->v.width;
        int32_t h = input->v.height;
        int32_t sw = input->v.strideWidth;
        int32_t sh = input->v.sliceHeight;
        
        
        if (mSDLContext->mTexture == NULL || mPixFmt != input->format) {
            INFO("create texture: %d x %d => %d x %d", w, h, sw, sh);
            if (mSDLContext->mTexture)
                SDL_DestroyTexture(mSDLContext->mTexture);
            
            mSDLContext->mTexture = SDL_CreateTexture(mSDLContext->mRenderer,
                                         SDL_PIXELFORMAT((ePixelFormat)input->format),
                                         SDL_TEXTUREACCESS_STREAMING,
                                         sw,
                                         sh);
            if (mSDLContext->mTexture == NULL) {
                ERROR("SDL_CreateTexture failed. %s", SDL_GetError());
                return UNKNOWN_ERROR;
            }
            uint32_t fmt;
            int access, w, h;
            SDL_QueryTexture(mSDLContext->mTexture, &fmt, &access, &w, &h);
            CHECK_EQ(fmt, (uint32_t)SDL_PIXELFORMAT((ePixelFormat)input->format));
            CHECK_EQ(access, (int)SDL_TEXTUREACCESS_STREAMING);
            CHECK_EQ(w, (int)sw);
            CHECK_EQ(h, (int)sh);
            
            mPixFmt = (ePixelFormat)input->format;
        }
        
        
        SDL_Rect rect;
        rect.x  = 0;
        rect.y  = 0;
        rect.w  = sw;
        rect.h  = sh;

        // seperate yuv space
        if (input->data[1] != NULL) {
            //DEBUG("update yuv texture");

            if (SDL_UpdateYUVTexture(mSDLContext->mTexture, &rect,
                        (const uint8_t*)input->data[0]->data(), sw,
                        (const uint8_t*)input->data[1]->data(), sw / 2,
                        (const uint8_t*)input->data[2]->data(), sw / 2) != 0) {
                ERROR("SDL_UpdateYUVTexture failed. %s", SDL_GetError());
            }
        } else {
            if (SDL_UpdateTexture(mSDLContext->mTexture, &rect, input->data[0]->data(), sw) != 0) {
                ERROR("SDL_UpdateTexture failed. %s", SDL_GetError());
            }
        }

        SDL_Rect rect0, rect1;
        rect0.x  = 0;
        rect0.y  = 0;
        rect0.w  = w;
        rect0.h  = h;

        SDL_GetRendererOutputSize(mSDLContext->mRenderer, &w, &h);
        rect1.x  = 0;
        rect1.y  = 0;
        rect1.w  = w;
        rect1.h  = h;

        SDL_RenderClear(mSDLContext->mRenderer);
        SDL_RenderCopy(mSDLContext->mRenderer, mSDLContext->mTexture, &rect0, &rect1);
        SDL_RenderPresent(mSDLContext->mRenderer);

        return OK;
    }
};

