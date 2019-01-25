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


// File:    mpx.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20181126     initial version
//

#define GL_SILENCE_DEPRECATION
#define LOG_TAG "mpx.gl.main"
//#define LOG_NDEBUG 0

#include <MediaFramework/MediaFramework.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <SDL.h>

#define EVENT_FRAME_READY (SDL_USEREVENT + 1)
#define DOUBLE_BUFFER 1
//#define TEST_SCREEN

#ifdef DEBUG_MALLOC
extern "C" {
    void malloc_debug_begin();
    void malloc_debug_end();
}
#endif

using namespace mtdcy;

static SDL_Window * window = NULL;
static sp<MediaPlayer> mp;
static double position = 0;

struct MPStatus : public StatusEvent {
    virtual void onEvent(const status_t& st) {
        INFO("==> status %d", st);
    }
};

struct MPRenderPosition : public RenderPositionEvent {
    MPRenderPosition() : RenderPositionEvent() { }
    virtual void onEvent(const MediaTime& v) {
        //INFO("progress %" PRId64, v);
        position = v.seconds();
    }
};

static void display() {
    SDL_Event event;
    event.type = EVENT_FRAME_READY;
    SDL_PushEvent(&event);
}

static sp<MediaFrame> sCurrentFrame;
struct MPRender : public RenderEvent {
    MPRender() : RenderEvent() { }
    virtual void onEvent(const sp<MediaFrame>& frame) {
        sCurrentFrame = frame;
        display();
    }
};

static void handleReshape() {
    display();
}

static sp<MediaOut> out;
static void handleDisplay() {
    INFO("display");
    if (sCurrentFrame == NULL) return;
    
    if (out == NULL) {
        INFO("creating media out device");
        Uint32 flags = SDL_GetWindowFlags(window);
        if (flags & SDL_WINDOW_ALLOW_HIGHDPI) {
            // get scale factor of high resolution
            SDL_SetWindowSize(window, sCurrentFrame->v.width/2, sCurrentFrame->v.height/2);
        } else {
            SDL_SetWindowSize(window, sCurrentFrame->v.width, sCurrentFrame->v.height);
        }
        
        Message options;
        options.setInt32(kKeyWidth, sCurrentFrame->v.width);
        options.setInt32(kKeyHeight, sCurrentFrame->v.height);
        options.setInt32(kKeyFormat, sCurrentFrame->v.format);
        options.setPointer("SDL_Window", window);
        out = MediaOut::Create(kCodecTypeVideo, options);
        CHECK_TRUE(out != NULL);
    }
    
    //glViewport(0, 0, 800, 480);
#ifdef TEST_SCREEN
    glClearColor(0, 1.0, 0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();
#endif
    
    out->write(sCurrentFrame);
    
#if DOUBLE_BUFFER
    SDL_GL_SwapWindow(window);
#endif
}

static void loop() {
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
            case EVENT_FRAME_READY:
                handleDisplay();
                break;
            case SDL_QUIT:
                return;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_SPACE:
                        mp->start();
                        break;
                    case SDLK_q:
                    case SDLK_ESCAPE:
                        break;
                }
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.type) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_EXPOSED:
                        handleReshape();
                } break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONUP:
            default:
                break;
        }
    }
}

static Uint32 window_flags() {
    Uint32 flags = SDL_WINDOW_RESIZABLE|SDL_WINDOW_SHOWN;
    flags |= SDL_WINDOW_OPENGL;
#ifdef __APPLE__
    flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
    return flags;
}

int main (int argc, char **argv) {
    
    INFO("Toolkit version %#x", TOOLKIT_VERSION);
    const String url = argv[argc - 1];
    INFO("url: %s", url.c_str());
    
#ifdef DEBUG_MALLOC
    malloc_debug_begin(); {
#endif
        
        // init window
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
        
        window = SDL_CreateWindow(url.basename().c_str(),
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  800,
                                  480,
                                  window_flags());
        
        // create gl context
#if DOUBLE_BUFFER
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#else
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
#endif
        SDL_GLContext glcontext = SDL_GL_CreateContext(window);
        
        INFO("gl version: %s", glGetString(GL_VERSION));
        INFO("glsl version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
        
        // create the mp
        Message options;
        options.set<sp<RenderPositionEvent> >("RenderPositionEvent", new MPRenderPosition);
        options.set<sp<StatusEvent> >("StatusEvent", new MPStatus);
        mp = MediaPlayer::Create(options);
        
        // add media to the mp
        Message media;
        media.setString("url", url);
        media.set<sp<RenderEvent> >("RenderEvent", new MPRender);
        mp->addMedia(media);
        
        // prepare the mp
        mp->prepare(kTimeBegin);
        
        // loop
        loop();
        
        // clearup
        mp->release();
        mp.clear();
        
        SDL_GL_DeleteContext(glcontext);
        SDL_DestroyWindow(window);
        window = NULL;
        
        // clear static context
        sCurrentFrame.clear();
        
        // quit sdl
        SDL_Quit();
#ifdef DEBUG_MALLOC
    } malloc_debug_end();
#endif
    return 0;
}
