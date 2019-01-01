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

#define LOG_TAG "mpx"
//#define LOG_NDEBUG 0

#include <MediaToolkit/Toolkit.h>
#include <MediaFramework/MediaSession.h>
#include <MediaFramework/MediaExtractor.h>
#include <MediaFramework/MediaDecoder.h>
#include <MediaFramework/MediaOut.h>
#include <MediaFramework/MediaPlayer.h>

#include <SDL.h>

#define PROGRESS_UPDATE_RATE (1000000LL)

namespace mtdcy {
    
    struct CountedStatusEvent : public StatusEvent {
        volatile int mCount;
        sp<StatusEvent> kept;
        
        void keep(const sp<StatusEvent>& _kept) { kept = _kept; }
        
        CountedStatusEvent(const sp<Looper>& lp, size_t n) :
        StatusEvent(lp), mCount(n) { }
        
        virtual ~CountedStatusEvent() { }
        
        virtual void onEvent(const status_t& st) {
            int old = atomic_sub(&mCount, 1);
            CHECK_GT(old, 0);
            if (old == 1 || st != OK) {
                onFinished(st);
                kept.clear();
            }
        }
        
        virtual void onFinished(status_t st) = 0;
    };
    
    // for MediaSession request packet, which always run in player's looper
    struct MediaSource : public PacketRequestEvent {
        sp<MediaExtractor>  mMedia;
        const size_t        mIndex;
        
        MediaSource(const sp<Looper>& lp, const sp<MediaExtractor>& media, size_t index) :
        PacketRequestEvent(lp), mMedia(media), mIndex(index) { }
        
        virtual void onEvent(const PacketRequestPayload& v) {
            // NO lock to mMedia, as all PacketRequestEvent run in the same looper
            sp<MediaPacket> pkt = mMedia->read(mIndex, v.mode, v.ts);
            
            if (pkt == NULL) {
                INFO("%zu: eos", mIndex);
            }
            
            sp<PacketReadyEvent> event = v.event;
            event->fire(pkt);
        }
    };
    
    struct SessionContext {
        eCodecFormat        mCodec;
        sp<MediaSession>    mMediaSession;
        
        SessionContext(eCodecFormat codec, const sp<MediaSession>& ms) :
        mCodec(codec),  mMediaSession(ms) { }
    };
    
    struct MPContext : public Looper {
        // external static context
        sp<RenderPositionEvent> mRenderPositionEvent;
        sp<StatusEvent> mStatusEvent;
        
        // mutable context
        HashTable<size_t, SessionContext> mSessions;
        size_t mNextId;
        bool mHasAudio;
        
        // clock
        sp<SharedClock> mClock;
        
        MPContext(const Message& options) : Looper("mp"),
        // external static context
        mRenderPositionEvent(NULL), mStatusEvent(NULL),
        // internal static context
        // mutable context
        mNextId(0),mHasAudio(false),
        // clock
        mClock(new SharedClock)
        {
            loop();
            
            if (options.contains("RenderPositionEvent")) {
                mRenderPositionEvent = options.find<sp<RenderPositionEvent> >("RenderPositionEvent");
            }
            
            if (options.contains("StatusEvent")) {
                mStatusEvent = options.find<sp<StatusEvent> >("StatusEvent");
            }
        }
        
        virtual ~MPContext() {
            mSessions.clear();
            terminate();
        }
        
        void setStatus(status_t st) {
            if (mStatusEvent != NULL) {
                mStatusEvent->fire(st);
            }
        }
    };
    
    // update render position to client
    struct UpdateRenderPosition : public Runnable {
        UpdateRenderPosition() : Runnable() { }
        
        virtual void run() {
            sp<MPContext> mpc = Looper::getLooper();
            
            if (mpc->mClock->isPaused()) {
                INFO("stop update progress");
                return;
            }
            
            MediaTime ts = mpc->mClock->get();
            INFO("current progress %.3f(s)", ts.seconds());
            
            mpc->mRenderPositionEvent->fire(ts);
            
            mpc->post(new UpdateRenderPosition, PROGRESS_UPDATE_RATE);
        }
    };
    
    // for media session update render position
    struct OnUpdateRenderPosition : public RenderPositionEvent {
        const size_t mID;
        OnUpdateRenderPosition(const sp<Looper>& lp, size_t id) : RenderPositionEvent(lp), mID(id) { }
        
        virtual void onEvent(const MediaTime& ts) {
            sp<MPContext> mpc = Looper::getLooper();
            
            if (ts == kTimeEnd) {
                INFO("eos...");
                if (mpc->mRenderPositionEvent != NULL) {
                    mpc->mRenderPositionEvent->fire(kTimeEnd);
                }
            }
        }
    };
    
    struct MediaRunnable : public Runnable {
        String url;
        Message options;
        
        MediaRunnable(const String& _url, const Message& _options) :
        url(_url), options(_options) { }
        
        virtual void run() {
            sp<MPContext> mpc = Looper::getLooper();
            
            sp<Content> pipe = Content::Create(url);
            if (pipe == NULL) {
                ERROR("create pipe failed");
                mpc->setStatus(UNKNOWN_ERROR);
                return;
            }
            
            sp<MediaExtractor> media = MediaExtractor::Create(pipe, NULL);
            if (media == NULL || media->status() != OK) {
                ERROR("create extractor failed");
                mpc->setStatus(UNKNOWN_ERROR);
                return;
            }
            
            sp<RenderEvent> externalRenderer;
            void *nativeWindow = NULL;
            if (options.contains("RenderEvent")) {
                externalRenderer = options.find<sp<RenderEvent> >("RenderEvent");
            }
            
            if (options.contains("SDL_Window")) {
                nativeWindow = options.findPointer("SDL_Window");
            }
            
            //double startTimeUs = options.findDouble("StartTime");
            //double endTimeUs = options.findDouble("EndTime");
            
            Message fileFormats = media->formats();
            size_t numTracks = fileFormats.findInt32(kKeyCount);
            
            for (size_t i = 0; i < numTracks; ++i) {
                // PacketRequestEvent
                sp<MediaSource> mc = new MediaSource(mpc, media, i);
                
                String sessionName = String::format("track-%zu", i);
                const Message& formats = fileFormats.find<Message>(sessionName);
                
                DEBUG("session %zu: %s", i, formats.string().c_str());
                
                eCodecFormat codec = (eCodecFormat)formats.findInt32(kKeyFormat);
                eCodecType type = GetCodecType(codec);
                if (type == kCodecTypeAudio) {
                    if (mpc->mHasAudio) {
                        INFO("ignore this audio");
                        continue;
                    } else {
                        mpc->mHasAudio = true;
                    }
                }
                
                Message options;
                if (type == kCodecTypeVideo) {
                    if (externalRenderer != NULL) {
                        options.set<sp<RenderEvent> >("RenderEvent", externalRenderer);
                    } else if (nativeWindow) {
                        options.setPointer("SDL_Window", nativeWindow);
                    }
                }
                
                options.set<sp<PacketRequestEvent> >("PacketRequestEvent", mc);
                
                options.set<sp<RenderPositionEvent> >("RenderPositionEvent",
                                                      new OnUpdateRenderPosition(mpc, mpc->mNextId));
                
                if (kCodecTypeAudio == type || numTracks == 1) {
                    options.set<sp<Clock> >("Clock", mpc->mClock->getClock(kClockRoleMaster));
                } else {
                    options.set<sp<Clock> >("Clock", mpc->mClock->getClock());
                }
                
                sp<MediaSession> session = MediaSessionCreate(formats, options);
                if (session == NULL) {
                    ERROR("create session for %s[%zu] failed", url.c_str(), i);
                    continue;
                }
                
                // save this packet queue
                mpc->mSessions.insert(mpc->mNextId++, SessionContext(codec, session));
            }
        }
    };
    
    struct PrepareStatusEvent : public CountedStatusEvent {
        PrepareStatusEvent(const sp<Looper>& looper, size_t n) :
        CountedStatusEvent(looper, n) { }
        
        virtual void onFinished(status_t st) {
            sp<MPContext> mpc = Looper::getLooper();
            INFO("prepare finished with status %d, current pos %.3f(s)",
                 st, mpc->mClock->get().seconds());
            
            // notify client
            mpc->mStatusEvent->fire(st);
            
            // notify the render position
            mpc->mRenderPositionEvent->fire(mpc->mClock->get());
        }
    };
    
    struct PrepareRunnable : public Runnable {
        MediaTime ts;
        PrepareRunnable(const MediaTime& _ts) : Runnable(), ts(_ts) { }
        virtual void run() {
            sp<MPContext> mpc = Looper::getLooper();
            
            // set clock time
            mpc->mClock->set(ts);
            
            // prepare sessions
            sp<PrepareStatusEvent> event = new PrepareStatusEvent(mpc, mpc->mSessions.size());
            event->keep(event);
        
            ControlEventPayload pl = { kControlEventPrepare, ts, event };
            
            HashTable<size_t, SessionContext>::iterator it = mpc->mSessions.begin();
            for (; it != mpc->mSessions.end(); ++it) {
                SessionContext& sc = it.value();
                sc.mMediaSession->fire(pl);
            }
        }
    };
    
    struct StartRunnable : public Runnable {
        StartRunnable() : Runnable() { }
        virtual void run() {
            sp<MPContext> mpc = Looper::getLooper();
            
            mpc->mClock->start();
            mpc->post(new UpdateRenderPosition);
        }
    };
    
    struct PauseRunnable : public Runnable {
        PauseRunnable() : Runnable() { }
        virtual void run() {
            sp<MPContext> mpc = Looper::getLooper();

            mpc->mClock->pause();
        }
    };
    
    struct StopRunnable : public Runnable {
        StopRunnable() : Runnable() { }
        virtual void run() {
            sp<MPContext> mpc = Looper::getLooper();
            
            ControlEventPayload pl = { kControlEventFlush };
            
            HashTable<size_t, SessionContext>::iterator it = mpc->mSessions.begin();
            for (; it != mpc->mSessions.end(); ++it) {
                SessionContext& sc = it.value();
                sc.mMediaSession->fire(pl);
            }
            
            mpc->mClock->reset();
        }
    };
    
    
    sp<MediaPlayer> MediaPlayer::Create(const Message& options) {
        sp<MediaPlayer> mp = new MediaPlayer;
        
        mp->mContext = new MPContext(options);
        
        return mp;
    }
    
    status_t MediaPlayer::addMedia(const String &url, const Message &options) {
        mContext->post(new MediaRunnable(url, options));
        return OK;
    }
    
    status_t MediaPlayer::prepare(const MediaTime& ts) {
        mContext->post(new PrepareRunnable(ts));
        return OK;
    }
    
    status_t MediaPlayer::start() {
        if (mContext->mStatusEvent == NULL) {
            // use tmp status event.
        }
        mContext->post(new StartRunnable());
        return OK;
    }

    status_t MediaPlayer::pause() {
        mContext->post(new PauseRunnable());
        return OK;
    }
    
    status_t MediaPlayer::stop() {
        mContext->post(new StopRunnable());
        return OK;
    }
    
}
