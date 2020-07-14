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
#include <OpenGL/OpenGL.h>
#else
#if defined(__MINGW32__)
#include <GL/glew.h>   // GL_SHADING_LANGUAGE_VERSION
#endif
#include <GL/gl.h>
#endif
#include <SDL.h>

//#define DOUBLE_BUFFER 1
//#define TEST_SCREEN
#define MAIN_THREAD_RENDER
#define PREFER_MODE kModeTypeDefault

#ifdef DEBUG_MALLOC
extern "C" {
    void malloc_debug_begin();
    void malloc_debug_end();
}
#endif

USING_NAMESPACE_MFWK

static SDL_Window * window = NULL;
static sp<IMediaPlayer> mp;
static double position = 0;

static sp<MediaDevice> g_out;
static ImageFormat g_format;
static sp<Clock> g_clock;
static bool g_paused;
static sp<MediaFrame> g_frame;  // keep ref current frame

static void dumpCurrentFrameIntoFile() {
    if (g_frame == NULL) {
        ERROR("current frame is nil");
        return;
    }
#if 0
    String desc = GetImageFrameString(g_frame);
    INFO("%s", desc.c_str());
    
    const PixelDescriptor * desc = GetPixelFormatDescriptor(g_frame->v.format);
    
    String filename = String::format("%s/%s_%dx%d_%.3f.raw",
                                     getenv("HOME"),
                                     desc->name,
                                     g_frame->v.width, g_frame->v.height,
                                     g_frame->pts.seconds());
    sp<Content> pipe = Content::Create(filename, abe::Content::Protocol::Write);
    
    size_t written = 0;
    for (size_t i = 0; ; ++i) {
        sp<Buffer> plane = g_frame->getData(i);
        if (plane == NULL) break;
        written += pipe->write(*plane);
    }
    INFO("written bytes %zu", written);
#endif
}

struct OnPlayerInfo : public PlayerInfoEvent {
    OnPlayerInfo() : PlayerInfoEvent(Looper::Current()) { }
    
    virtual void onEvent(const ePlayerInfoType& info, const sp<Message>& payload) {
        switch (info) {
            case kInfoPlayerReady:
                INFO("player is ready...");
                g_paused = true;
                break;
            case kInfoPlayerPlaying:
                g_paused = false;
                break;
            case kInfoPlayerPaused:
                g_paused = true;
                break;
            case kInfoPlayerError:
                ERROR("player report error");
                Looper::Main()->flush();
                Looper::Main()->terminate();
                break;
            default:
                break;
        }
    }
};

struct OnFrameUpdate : public MediaFrameEvent {
    OnFrameUpdate() : MediaFrameEvent(Looper::Main()) { }
    
    virtual void onEvent(const sp<MediaFrame>& frame) {
        if (frame == NULL) {
            if (g_out != NULL) g_out->reset();
            return;
        }
        
        if (g_out == NULL || frame->video != g_format) {
            g_format = frame->video;
            
            // setup local context
            sp<Message> format = new Message;
            sp<Message> options = new Message;
            format->setInt32(kKeyFormat, g_format.format);
            format->setInt32(kKeyWidth,  g_format.width);
            format->setInt32(kKeyHeight, g_format.height);
            format->setInt32(kKeyType, kCodecTypeVideo);
            
            g_out = MediaDevice::create(format, options);
            CHECK_FALSE(g_out.isNil());
        }
        
        g_frame = frame;
        
        //glViewport(0, 0, 800, 480);
#ifdef TEST_SCREEN
        glClearColor(0, 1.0, 0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();
#endif
        if (frame == NULL) return;
        
        g_out->push(frame);
        
#if DOUBLE_BUFFER
        //SDL_GL_SwapWindow(window);
#endif
    }
};

struct SDLJob : public Job {
    virtual void onJob() {
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
                            if (g_paused) mp->start();
                            else mp->pause();
                            break;
                        case SDLK_q:
                        case SDLK_ESCAPE:
                            break;
                        case SDLK_LEFT: {
                            Time pos = g_clock->get() - Time::Seconds(5);
                            if (pos < 0) pos = 0;
                            mp->prepare(pos.useconds());
                        } break;
                        case SDLK_RIGHT: {
                            Time pos = g_clock->get() + Time::Seconds(5);
                            mp->prepare(pos.useconds());
                        } break;
                        case SDLK_s:
                            dumpCurrentFrameIntoFile();
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
        
        Looper::Main()->dispatch(new SDLJob, Time::MilliSeconds(200));
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
    
    INFO("Toolkit version %#x", ABE_VERSION);
    MemoryAnalyzerPrepare(); {
        sp<Looper> mainLooper = Looper::Main();
        
        // init window
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
        
        // TODO: create window only when video exists
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
        
        
        // add media to the mp
        sp<Message> media = new Message;
        media->setString(kKeyURL, url);
#ifdef MAIN_THREAD_RENDER
        media->setObject(kKeyVideoFrameEvent, new OnFrameUpdate);
#endif
        
        sp<Message> options = new Message;
        options->setObject(kKeyPlayerInfoEvent, new OnPlayerInfo);
        options->setInt32(kKeyMode, PREFER_MODE);
        
        // create the mp
        mp = IMediaPlayer::Create(media, options);
        
        g_clock = mp->clock();
                
        // loop
        mainLooper->dispatch(new SDLJob);
        mainLooper->loop();
        
        // clearup
        mp.clear();
        INFO("finished...");
        g_clock = NULL;
        
        SDL_GL_DeleteContext(glc);
        SDL_DestroyWindow(window);
        window = NULL;
        
        // clear static context
        g_out.clear();
        
        // quit sdl
        SDL_Quit();
    } MemoryAnalyzerFinalize();
    return 0;
}
