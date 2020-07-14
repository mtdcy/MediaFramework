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
 * File:    cc_main.cpp
 * Author:  mtdcy.chen
 * Changes:
 *          1. 20200705     initial version
 *
 */

#define LOG_TAG "cc.main"
#include <MediaFramework/MediaFramework.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>
#else
#if defined(__MINGW32__)
#include <GL/glew.h>   // GL_SHADING_LANGUAGE_VERSION
#endif
#include <GL/gl.h>
#endif
#include <SDL.h>

USING_NAMESPACE_MFWK

static Uint32 window_flags() {
    Uint32 flags = SDL_WINDOW_RESIZABLE|SDL_WINDOW_SHOWN;
    flags |= SDL_WINDOW_OPENGL;
#ifdef __APPLE__
    flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
    return flags;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        printf("usage: cc <source> <width> <height> <source pixel> [target pixel]\n");
        return 1;
    }
    const String url = argv[1];
    const int32_t width = String(argv[2]).toInt32();
    const int32_t height = String(argv[3]).toInt32();
    
    const PixelDescriptor * ipd = GetPixelFormatDescriptorByName(argv[4]);
    ImageFormat iformat = {
        .format = ipd->format,
        .width  = width,
        .height = height,
        .rect   = { 0, 0, width, height }
    };
    ImageFormat oformat = iformat;
    oformat.format = kPixelFormatRGB32;
    if (argc > 5) {
        const PixelDescriptor * opd = GetPixelFormatDescriptorByName(argv[5]);
        oformat.format = opd->format;
    }
    
    const size_t imageLength = (width * height * ipd->bpp) / 8;
    sp<ABuffer> imageBuffer = Content::Create(url);
    sp<Buffer> imageData = imageBuffer->readBytes(imageLength);
    sp<MediaFrame> imageFrame = MediaFrame::Create(iformat, imageData);
    
    sp<MediaFrame> output = imageFrame;
    if (iformat.format != oformat.format) {
        sp<MediaDevice> cc = CreateColorConverter(iformat, oformat, NULL);
        if (cc.isNil()) {
            ERROR("create color converter failed");
            return 1;
        }
        
        cc->push(imageFrame);
        output = cc->pull();
    }
    
    // create window using SDL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window * window = SDL_CreateWindow(url.basename().c_str(),
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                800,
                                480,
                                window_flags());
    SDL_GLContext glc = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, glc);
    
    sp<Message> formats = new Message;
    formats->setInt32(kKeyFormat, oformat.format);
    formats->setInt32(kKeyWidth, oformat.width);
    formats->setInt32(kKeyHeight, oformat.height);
    sp<Message> options = new Message;
    sp<MediaDevice> out = MediaDevice::create(formats, options);
    if (out.isNil()) {
        ERROR("create output device failed.");
        return 1;
    }
    
    out->push(output);

    bool loop = true;
    SDL_Event event;
    for (;;) {
        while (loop && SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    INFO("quit...");
                    loop = false;
                    break;
                default: break;
            }
        }
        if (loop == false) break;
        Timer().sleep(Time::MilliSeconds(200));
    }
    
    SDL_DestroyWindow(window);
    window = NULL;
    
    SDL_Quit();
}
