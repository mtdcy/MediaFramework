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

#define LOG_TAG "mpx.main"
//#define LOG_NDEBUG 0

#include <MediaFramework/MediaFramework.h>
#include <SDL.h>

using namespace mtdcy;

static bool started = false;
static int64_t duration = 0;
static double progress = 0;
static int mouse_x = 0;
static int mouse_y = 0;
static bool mouse_down = false;
static SDL_Window *window = NULL;
static SDL_Window *preview_window = NULL;

struct MPStatus : public StatusEvent {
    virtual void onEvent(const status_t& st) {
        INFO("==> status %d", st);
    }
};

struct Progress : public RenderPositionEvent {
    Progress() : RenderPositionEvent() { }
    virtual void onEvent(const MediaTime& v) {
        //INFO("progress %" PRId64, v);
        progress = v.seconds();
    }
};

static bool sExternalRenderer = true;
static sp<MediaFrame> sFrame;
#define EVENT_FRAME_READY (SDL_USEREVENT + 1)
struct MainThreadRenderer : public RenderEvent {
    virtual void onEvent(const sp<MediaFrame>& frame) {
        sFrame = frame;
        SDL_Event event;
        event.type = EVENT_FRAME_READY;
        SDL_PushEvent(&event);
    }
};

static sp<MediaFrame> sPreviewFrame;
#define EVENT_PREVIEW_READY (SDL_USEREVENT+2)
struct PreviewRenderer : public RenderEvent {
    virtual void onEvent(const sp<MediaFrame>& frame) {
        sPreviewFrame = frame;
        SDL_Event event;
        event.type = EVENT_PREVIEW_READY;
        SDL_PushEvent(&event);
    }
};

#include <MediaFramework/sdl2/SDLVideo.h>
static sp<SDLVideo> sRenderer;
void render() {
    if (sRenderer == NULL) {
        Message params;
        params.setInt32(kKeyFormat, sFrame->format);
        params.setInt32(kKeyWidth, sFrame->v.width);
        params.setInt32(kKeyHeight, sFrame->v.height);
        params.setPointer("SDL_Window", window);
        sRenderer = new SDLVideo(params);
        if (sRenderer->status() != OK) {
            ERROR("create renderer failed.");
        }
    }

    CHECK_TRUE(sRenderer != NULL);

    sRenderer->write(sFrame);
}

static sp<SDLVideo> sPreviewRenderer;
void renderPreview() {
    if (sPreviewRenderer == NULL) {
        Message params;
        INFO("%d x %d", sPreviewFrame->v.width, sPreviewFrame->v.height);
        params.setInt32(kKeyFormat, sFrame->format);
        params.setInt32(kKeyWidth, sPreviewFrame->v.width);
        params.setInt32(kKeyHeight, sPreviewFrame->v.height);
        params.setPointer("SDL_Window", preview_window);
        sPreviewRenderer = new SDLVideo(params);
        if (sPreviewRenderer->status() != OK) {
            ERROR("create renderer failed.");
        }
    }

    CHECK_TRUE(sPreviewRenderer != NULL);

    SDL_SetWindowPosition(preview_window, mouse_x, mouse_y);
    SDL_ShowWindow(preview_window);

    sPreviewRenderer->write(sPreviewFrame);
}

#ifdef DEBUG_MALLOC
extern "C" {
    void malloc_debug_begin();
    void malloc_debug_end();
}
#endif
int main (int argc, char **argv) {
    
    INFO("Toolkit version %#x", TOOLKIT_VERSION);
    
#ifdef DEBUG_MALLOC
    malloc_debug_begin(); {
#endif
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    window = SDL_CreateWindow("sdl video player",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            800,
            480,
            SDL_WINDOW_OPENGL
            |SDL_WINDOW_RESIZABLE
            |SDL_WINDOW_SHOWN
            |SDL_WINDOW_ALLOW_HIGHDPI);

    preview_window = SDL_CreateWindow("preview",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            160,
            96,
            SDL_WINDOW_OPENGL
            |SDL_WINDOW_HIDDEN
            |SDL_WINDOW_BORDERLESS
            |SDL_WINDOW_ALWAYS_ON_TOP
            |SDL_WINDOW_POPUP_MENU
            |SDL_WINDOW_ALLOW_HIGHDPI);

    const String filename = argv[1];
    SDL_SetWindowTitle(window, filename.basename().c_str());

    sp<RenderPositionEvent> callback = new Progress;
    Message option0;
    option0.set<sp<RenderPositionEvent> >("RenderPositionEvent", callback);
    option0.set<sp<StatusEvent> >("StatusEvent", new MPStatus);
        
    sp<MediaPlayer> engine = MediaPlayer::Create(option0);

    sp<MainThreadRenderer> renderer = new MainThreadRenderer;
    sp<PreviewRenderer> renderer1 = new PreviewRenderer;

    Message options;
    options.setString("url", filename);
    options.set<sp<RenderPositionEvent> >("ProgressEvent", callback);
    if (sExternalRenderer) {
        options.set<sp<RenderEvent> >("RenderEvent", renderer);
    } else {
        options.setPointer("SDL_Window", window);
    }
    options.set<sp<RenderEvent> >("PreviewRenderEvent", renderer1);
    engine->addMedia(options);

    //if (engine->status() == OK) {
        engine->prepare(kTimeBegin);
    //}

    SDL_Event event;
    bool quit = false;
    while (!quit && SDL_WaitEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                // quit without stop, for testing purpose
                quit = true;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_SPACE:
                        if (!started) {
                            //if (engine->state() != MediaPlayer::kStateReady &&
                            //    engine->state() != MediaPlayer::kStatePaused)
                            //    engine->prepare(kTimeBegin);
                            engine->start();
                            started = true;
                        } else {
                            engine->pause();
                            started = false;
                        }
                        break;
                    case SDLK_q:
                    case SDLK_ESCAPE:
                        engine->flush();
                        started = false;
                        break;
                    case SDLK_RIGHT:
                        engine->prepare(progress * 1000000LL + 5000000LL);
                        break;
                    case SDLK_LEFT:
                        engine->prepare(progress * 1000000ll - 5000000LL);
                        break;
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                mouse_x = event.button.x;
                mouse_y = event.button.y;
                mouse_down = true;

                //engine->pause();
                //engine->peek(progress, renderer1);
                break;
            case SDL_MOUSEMOTION:
                if (mouse_down) {
                    //int delta = event.motion.x - mouse_x;
                    //engine->preview(progress + 5000000LL);
                    int w,h;
                    SDL_GetWindowSize(window, &w, &h);
                    static int64_t lastpos = 0;
                    int64_t pos = (duration * event.motion.x) / w;
                    // usally 1s has only one sync frame.
                    if (abs(pos - lastpos) > 1000000LL) {
                        lastpos = pos;
                    }
                } break;
            case SDL_MOUSEBUTTONUP:
                mouse_down = false;
                //engine->start();
                break;
            case SDL_WINDOWEVENT:
                switch (event.window.type) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_EXPOSED:
                        INFO("window changed, refresh");
                        render();
                        break;
                } break;
            case EVENT_FRAME_READY:
                render();
                break;
            case EVENT_PREVIEW_READY:
                renderPreview();
                break;
            default:
                break;
        }
    }

    engine.clear();

    sRenderer.clear();
    sFrame.clear();
    callback.clear();
    renderer.clear();

    SDL_DestroyWindow(window);
    window = NULL;
#ifdef DEBUG_MALLOC
    } malloc_debug_end();
#endif
    return 0;
}
