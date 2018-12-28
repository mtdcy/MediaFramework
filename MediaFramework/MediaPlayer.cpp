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
        sp<StatusEvent> mStatusEvent;
        volatile int mCount;
        Mutex mLock;
        Condition mWait;
        status_t mStatus;
        
        CountedStatusEvent(const sp<StatusEvent>& event, size_t n) :
        StatusEvent(), mStatusEvent(event), mCount(n), mStatus(OK) { }
        
        virtual void onEvent(const status_t& st) {
            int old = atomic_sub(&mCount, 1);
            CHECK_GE(old, 1);
            if (old == 1 || st != OK) {
                if (mStatusEvent != NULL) {
                    mStatusEvent->fire(st);
                } else {
                    AutoLock _l(mLock);
                    mWait.signal();
                    mStatus = st;
                }
            }
        }
        
        status_t wait() {
            AutoLock _l(mLock);
            mWait.wait(mLock);
            return mStatus;
        }
    };

    struct OnRequestPacket : public PacketRequestEvent {
        sp<MediaExtractor>  mMedia;
        const size_t        mIndex;
    
        OnRequestPacket(const sp<MediaExtractor>& media, size_t index, const sp<Looper>& lp) :
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

    struct OnUpdateRenderPosition : public RenderPositionEvent {
        MediaPlayer *mTgt;
        const size_t mID;
    
        OnUpdateRenderPosition(MediaPlayer *tgt, size_t id, const sp<Looper>& lp) :
            RenderPositionEvent(lp), mTgt(tgt), mID(id) { }
        virtual void onEvent(const MediaTime& v)
        { mTgt->onUpdateRenderPosition(mID, v); }
    };
    
    struct UpdateRenderPosition : public Runnable {
        MediaPlayer *mTgt;
    
        UpdateRenderPosition(MediaPlayer *tgt) :
            Runnable(), mTgt(tgt) { }
        virtual void run() { mTgt->updateRenderPosition(); }
    };

    struct MediaContext {
        MediaContext(const sp<MediaExtractor>& me) :
            mExtractor(me) { }

        sp<MediaExtractor>      mExtractor;
    };

    enum eSessionState {
        kSessionStateInit,
        kSessionStateReady,
        kSessionStateRender,
        kSessionStatePaused,
    };
    struct MediaPlayer::SessionContext {
        SessionContext(const sp<MediaContext>& mc,
                eCodecFormat codec,
                eCodecType type,
                size_t index,
                const sp<MediaSession>& ms) :
            mCodec(codec), mType(type),
            mMedia(mc), mIndex(index),
            mMediaSession(ms),
            mState(kSessionStateInit)
        { }

        eCodecFormat        mCodec;
        eCodecType          mType;
        sp<MediaContext>    mMedia;
        size_t              mIndex;
        sp<MediaSession>    mMediaSession;
        eSessionState       mState;
    };


    MediaPlayer::MediaPlayer(const Message& options) :
        mLooper(new Looper("mpx")), mStatus(NO_INIT),
        mState(kStateInvalid), mNextId(0),
        mHasAudio(false), mClock(new SharedClock),
        mUpdateRenderPosition(new UpdateRenderPosition(this))
    {
        mLooper->loop();
        
        if (options.contains("RenderPositionEvent")) {
            mPositionEvent = options.find<sp<RenderPositionEvent> >("RenderPositionEvent");
        }
        
        mStatus = OK;
    }

    MediaPlayer::~MediaPlayer() {
        AutoLock _l(mLock);
        mLooper->terminate();
        mContext.clear();
    }
    
    MediaPlayer::eStateType MediaPlayer::state() const {
        AutoLock _l(mLock);
        return mState;
    }

    status_t MediaPlayer::addMedia(const String& url, const Message& options) {
        AutoLock _l(mLock);
        sp<Content> pipe = Content::Create(url);
        if (pipe == NULL) {
            ERROR("create pipe failed");
            return UNKNOWN_ERROR;
        }
        sp<MediaExtractor> extractor = MediaExtractor::Create(pipe, NULL);
        if (extractor == NULL || extractor->status() != OK) {
            ERROR("create extractor failed");
            return UNKNOWN_ERROR;
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

        Message fileFormats = extractor->formats();
        size_t numTracks = fileFormats.findInt32(kKeyCount);

        sp<MediaContext> mc = new MediaContext(extractor);

        for (size_t i = 0; i < numTracks; ++i) {
            String sessionName = String::format("track-%zu", i);
            const Message& formats = fileFormats.find<Message>(sessionName);

            DEBUG("session %zu: %s", i, formats.string().c_str());

            eCodecFormat codec = (eCodecFormat)formats.findInt32(kKeyFormat);
            eCodecType type = GetCodecType(codec);
            if (type == kCodecTypeAudio) {
                if (mHasAudio) {
                    INFO("ignore this audio");
                    continue;
                } else {
                    mHasAudio = true;
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

            options.set<sp<PacketRequestEvent> >("PacketRequestEvent",
                    new OnRequestPacket(extractor, i, mLooper));
            
            options.set<sp<RenderPositionEvent> >("RenderPositionEvent",
                    new OnUpdateRenderPosition(this, mNextId, mLooper));

            if (kCodecTypeAudio == type || numTracks == 1) {
                options.set<sp<Clock> >("Clock", mClock->getClock(kClockRoleMaster));
            } else {
                options.set<sp<Clock> >("Clock", mClock->getClock());
            }

            sp<MediaSession> session = MediaSessionCreate(formats, options);
            if (session == NULL) {
                ERROR("create session for %s[%zu] failed", url.c_str(), i);
                continue;
            }
            
            // save this packet queue
            sp<SessionContext> sc = new SessionContext(mc, codec, type, i, session);
            
            mContext.insert(mNextId++, sc);
        }

        if (mContext.size()) {
            mState = kStateInit;
        }
        return OK;
    }
    
    status_t MediaPlayer::prepare(const MediaTime& ts) {
        mLock.lock();
        if (mState != kStateInit &&
            mState != kStateStopped) {
            ERROR("prepare in invalid state");
            mLock.unlock();
            return INVALID_OPERATION;
        }
        
        sp<CountedStatusEvent> event = new CountedStatusEvent(NULL, mContext.size());
        ControlEventPayload pl = { kControlEventPrepare, ts, event };
        
        HashTable<size_t, sp<SessionContext> >::iterator it = mContext.begin();
        for (; it != mContext.end(); ++it) {
            sp<SessionContext>& sc = it.value();
            sc->mMediaSession->fire(pl);
        }

        mState = kStateReady;
        
        mLock.unlock();
        return event->wait();
        
        //return OK;
    }

    status_t MediaPlayer::start() {
        AutoLock _l(mLock);
        if (mState != kStateReady &&
            mState != kStatePaused) {
            ERROR("start in invalid state");
            return INVALID_OPERATION;
        }
        
        INFO("start...");
        HashTable<size_t, sp<SessionContext> >::iterator it = mContext.begin();
        for (; it != mContext.end(); ++it) {
            sp<SessionContext>& sc = it.value();
            //sc->mMediaSession->start();
        }

        mClock->start();

        mLooper->post(mUpdateRenderPosition, PROGRESS_UPDATE_RATE);
        
        mState = kStatePlaying;

        return OK;
    }

    status_t MediaPlayer::pause() {
        AutoLock _l(mLock);
        if (mState != kStatePlaying) {
            ERROR("pause in invalid state");
            return INVALID_OPERATION;
        }
        
        INFO("pause...");

        HashTable<size_t, sp<SessionContext> >::iterator it = mContext.begin();
        for (; it != mContext.end(); ++it) {
            sp<SessionContext>& sc = it.value();
            //sc->mMediaSession->stop();
        }

        mClock->pause();
        
        mLooper->remove(mUpdateRenderPosition);
        
        mState = kStatePaused;

        return OK;
    }

    status_t MediaPlayer::stop() {
        AutoLock _l(mLock);
        if (mState != kStatePlaying) {
            ERROR("stop in invalid state");
            return INVALID_OPERATION;
        }
        INFO("stop...");

        ControlEventPayload pl = { kControlEventFlush };
        
        HashTable<size_t, sp<SessionContext> >::iterator it = mContext.begin();
        for (; it != mContext.end(); ++it) {
            sp<SessionContext>& sc = it.value();
            sc->mMediaSession->fire(pl);
        }

        mClock->reset();
        
        mLooper->remove(mUpdateRenderPosition);
        
        mState = kStateStopped;

        return OK;
    }

    status_t MediaPlayer::seek(int64_t ts) {
        AutoLock _l(mLock);
        if (mState != kStateReady &&
            mState != kStatePlaying &&
            mState != kStatePaused) {
            ERROR("seek in invalid state");
            return INVALID_OPERATION;
        }
        
        mClock->pause();
        
        INFO("seek %.3f(s)...", ts / 1E6);

        HashTable<size_t, sp<SessionContext> >::iterator it = mContext.begin();
        for (; it != mContext.end(); ++it) {
            sp<SessionContext>& sc = it.value();
            // flush then prepare at new position
            //sc->mMediaSession->flush();
            //sc->mMediaSession->prepare(ts);
        }
        
        mClock->update(ts - 500000LL);  // give 500ms to let session prepare

        return OK;
    }

    void MediaPlayer::onUpdateRenderPosition(size_t id, const MediaTime& ts) {
        DEBUG("%zu: update render position %.3f(s)", id, ts/1E6);
        if (ts == kTimeEnd) {
            INFO("eos...");
            mLooper->remove(mUpdateRenderPosition);
            if (mPositionEvent != NULL) {
                mPositionEvent->fire(kTimeEnd);
            }
        }
        
#if 0
        // change context
        // handle this with another looper to avoid dead lock
        AutoLock _l(mLock);
        sp<SessionContext>& sc = mContext[id];
        if (ts == kTimeEnd) {
            sc->mMediaSession->flush();
            sc->mState = kSessionStateInit;
        }
        
        // TODO
#endif
    }

    void MediaPlayer::updateRenderPosition() {
        if (mClock->isPaused()) {
            INFO("stop update progress");
            return;
        }

        MediaTime ts = mClock->get();
        INFO("current progress %.3f(s)", ts.seconds());
        mPositionEvent->fire(ts);

        mLooper->post(mUpdateRenderPosition, PROGRESS_UPDATE_RATE);
    }

}
