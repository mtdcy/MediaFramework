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
#define LOG_TAG "mpx.main"
//#define LOG_NDEBUG 0

#include <MediaFramework/MediaFramework.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include <SDL.h>

#define EVENT_PREPARE       (SDL_USEREVENT + 1)
#define EVENT_FRAME_READY   (SDL_USEREVENT + 2)
#define EVENT_FLUSH         (SDL_USEREVENT + 3)

#define DOUBLE_BUFFER 1
//#define TEST_SCREEN
#define MAIN_THREAD_RENDER
#define PREFER_MODE kModeTypeDefault

#ifdef DEBUG_MALLOC
extern "C" {
    void malloc_debug_begin();
    void malloc_debug_end();
}
#endif

__USING_NAMESPACE_MPX

static SDL_Window * window = NULL;
static sp<IMediaPlayer> mp;
static double position = 0;

struct MPStatus : public StatusEvent {
    virtual void onEvent(const MediaError& st) {
        INFO("==> status %d", st);
    }
    virtual String string() const { return "MPStatus"; }
};

struct MPRenderPosition : public RenderPositionEvent {
    MPRenderPosition() : RenderPositionEvent() { }
    virtual void onEvent(const MediaTime& v) {
        //INFO("progress %" PRId64, v);
        position = v.seconds();
    }
    virtual String string() const { return "MPRenderPosition"; }
};

static void sendEvent(Uint32 type) {
    SDL_Event event;
    event.type = type;
    SDL_PushEvent(&event);
}

static sp<MediaOut> g_out;
static sp<MediaFrame> g_frame;
static Message g_format;
struct MediaOutProxy : public MediaOut {
    MediaOutProxy() : MediaOut() { }
    virtual ~MediaOutProxy() { }
    virtual status_t status() const { return OK; }
    virtual String string() const { return ""; }
    virtual Message formats() const { return g_format; }
    virtual status_t configure(const Message& options) { return INVALID_OPERATION; }
    virtual status_t prepare(const Message& options) {
        INFO("prepare => %s", options.string().c_str());
        g_format = options;
        sendEvent(EVENT_PREPARE);
        return OK;
    }
    virtual status_t write(const sp<MediaFrame>& frame) {
        g_frame = frame;
        sendEvent(EVENT_FRAME_READY);
        return OK;
    }
    virtual status_t flush() {
        INFO("flush");
        g_frame = NULL;
        sendEvent(EVENT_FLUSH);
        return OK;
    }
};

static void handleReshape() {
    sendEvent(EVENT_FRAME_READY);
}

static void handleDisplay() {
    //INFO("display");
    if (g_frame == NULL) return;
    
    //glViewport(0, 0, 800, 480);
#ifdef TEST_SCREEN
    glClearColor(0, 1.0, 0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();
#endif
    
    g_out->write(g_frame);
    
#if DOUBLE_BUFFER
    SDL_GL_SwapWindow(window);
#endif
}

static bool loop() {
    SDL_Event event;
    while (SDL_WaitEvent(&event)) {
        switch (event.type) {
            case EVENT_FRAME_READY:
                handleDisplay();
                break;
            case EVENT_PREPARE:
                CHECK_TRUE(g_out->prepare(g_format) == OK);
                break;
            case EVENT_FLUSH:
                CHECK_TRUE(g_out->flush() == OK);
                break;
            case SDL_QUIT:
                INFO("quiting...");
                return false;
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_SPACE:
                        if (mp->state() == kStatePlaying)
                            mp->pause();
                        else {
                            if (mp->state() != kStateReady)
                                mp->prepare(kTimeBegin);
                            mp->start();
                        }
                        break;
                    case SDLK_q:
                    case SDLK_ESCAPE:
                        mp->flush();
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
    return true;
}

struct MainRunnable : public Runnable {
    virtual void run() {
        if (loop()) {
            Looper::Main()->post(new MainRunnable);
        }
    }
};

static Uint32 window_flags() {
    Uint32 flags = SDL_WINDOW_RESIZABLE|SDL_WINDOW_SHOWN;
    flags |= SDL_WINDOW_OPENGL;
#ifdef __APPLE__
    flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
    return flags;
}

extern "C" void malloc_prepare();
extern "C" void malloc_bypass();
extern "C" void malloc_finalize();
int main (int argc, char **argv) {
    
    INFO("Toolkit version %#x", TOOLKIT_VERSION);
    const String url = argv[argc - 1];
    INFO("url: %s", url.c_str());
    
    malloc_prepare(); {
        
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
        SDL_GLContext glc = SDL_GL_CreateContext(window);
        
        INFO("gl version: %s", glGetString(GL_VERSION));
        INFO("glsl version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
        
        // setup local context
        g_out = MediaOut::Create(kCodecTypeVideo);
        
        // create the mp
        Message options;
        options.setObject("RenderPositionEvent", new MPRenderPosition);
        options.setObject("StatusEvent", new MPStatus);
        mp = IMediaPlayer::Create(options);
        
        // add media to the mp
        Message media;
        media.setString("url", url);
        media.setInt32(kKeyMode, PREFER_MODE);
#ifdef MAIN_THREAD_RENDER
        media.setObject("MediaOut", new MediaOutProxy);
#endif
        mp->init(media);
        
        // prepare the mp
        mp->prepare(kTimeBegin);
        
        // loop
        //loop();
        sp<Looper> mainLooper = Looper::Main();
        mainLooper->post(new MainRunnable);
        mainLooper->loop();
        
        // clearup
        if (mp->state() != kStateFlushed) mp->flush();
        mp->release();
        mp.clear();
        
        // terminate threads
        mainLooper->terminate();
        
        SDL_GL_DeleteContext(glc);
        SDL_DestroyWindow(window);
        window = NULL;
        
        // clear static context
        g_out.clear();
        g_frame.clear();
        g_format.clear();
        
        // quit sdl
        SDL_Quit();
    } malloc_finalize();
    return 0;
}
