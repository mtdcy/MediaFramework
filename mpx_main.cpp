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
#if defined(__MINGW32__)
#include <GL/glew.h>   // GL_SHADING_LANGUAGE_VERSION
#endif
#include <GL/gl.h>
#endif
#include <SDL.h>

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

static sp<MediaOut> g_out;
static Message g_format;
struct PrepareRunnable : public Runnable {
    virtual void run() {
        CHECK_TRUE(g_out->prepare(g_format) == kMediaNoError);
        // have to make MediaOut accept this format
    }
};

struct DisplayRunnable : public Runnable {
    sp<MediaFrame>  frame;
    DisplayRunnable(const sp<MediaFrame>& _frame) : frame(_frame) { }
    virtual void run() {
        //glViewport(0, 0, 800, 480);
#ifdef TEST_SCREEN
        glClearColor(0, 1.0, 0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();
#endif
        if (frame == NULL) return;

        g_out->write(frame);
        
#if DOUBLE_BUFFER
        SDL_GL_SwapWindow(window);
#endif
    }
};

struct FlushRunnable : public Runnable {
    virtual void run() {
        g_out->flush();
    }
};

struct MediaOutProxy : public MediaOut {
    MediaOutProxy() : MediaOut() { }
    virtual ~MediaOutProxy() { }
    virtual MediaError status() const                       { return kMediaNoError; }
    virtual String string() const                           { return ""; }
    virtual Message formats() const                         { return g_format; }
    virtual MediaError configure(const Message& options)    { return kMediaErrorInvalidOperation; }
    virtual MediaError prepare(const Message& options)      { g_format = options; Looper::Main()->post(new PrepareRunnable); return kMediaNoError; }
    virtual MediaError write(const sp<MediaFrame>& frame)   { Looper::Main()->post(new DisplayRunnable(frame)); return kMediaNoError; }
    virtual MediaError flush()                              { Looper::Main()->post(new FlushRunnable); return kMediaNoError; }
};

struct SDLRunnable : public Runnable {
    virtual void run() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    INFO("quiting...");
                    Looper::Main()->terminate();
                    return;
                case SDL_KEYUP:
                    INFO("sdl keyup %s", SDL_GetKeyName(event.key.keysym.sym));
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
#if 0
                    switch (event.window.type) {
                        case SDL_WINDOWEVENT_RESIZED:
                        case SDL_WINDOWEVENT_EXPOSED:
                            handleReshape();
                    } break;
#endif
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEMOTION:
                case SDL_MOUSEBUTTONUP:
                default:
                    break;
            }
        }
        
        Looper::Main()->post(new SDLRunnable, 200 * 1000LL);
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
#if defined(_WIN32) || defined(__MINGW32)
#include <windows.h>
int APIENTRY WinMain(HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR     lpCmdLine,
        int       nCmdShow)
#else
int main (int argc, char **argv) 
#endif
{
#if defined(_WIN32) || defined(__MINGW32)
    INFO("%s | %d", lpCmdLine, nCmdShow);
    const String url = lpCmdLine;
#else
    const String url = argv[argc - 1];
#endif
    INFO("url: %s", url.c_str());
    
    INFO("Toolkit version %#x", TOOLKIT_VERSION);
    malloc_prepare(); {
        sp<Looper> mainLooper = Looper::Main();
        
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
        mainLooper->profile();
        mainLooper->post(new SDLRunnable);
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
        g_format.clear();
        
        // quit sdl
        SDL_Quit();
    } malloc_finalize();
    return 0;
}
