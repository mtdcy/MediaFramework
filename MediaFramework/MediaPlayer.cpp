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

namespace mtdcy {
    
    
    struct CountedStatusEvent : public StatusEvent {
        volatile int mCount;
        sp<StatusEvent> kept;

        // FIXME:
        void keep(const sp<StatusEvent>& self) { kept = self; }

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
        int64_t             mStartTime;
        int64_t             mEndTime;

        SessionContext(eCodecFormat codec, const sp<MediaSession>& ms) :
            mCodec(codec),  mMediaSession(ms) { }
    };

    struct MPContext {
        // external static context
        sp<RenderPositionEvent> mRenderPositionEvent;
        int64_t mRenderPositionUpdateInterval;
        sp<StatusEvent> mStatusEvent;

        // mutable context
        HashTable<size_t, SessionContext> mSessions;
        size_t mNextId;
        bool mHasAudio;

        // clock
        sp<SharedClock> mClock;

        MPContext(const Message& options) :
            // external static context
            mRenderPositionEvent(NULL), mRenderPositionUpdateInterval(500000LL),
            mStatusEvent(NULL),
            // internal static context
            // mutable context
            mNextId(0),mHasAudio(false),
            // clock
            mClock(new SharedClock)
        {
            if (options.contains("RenderPositionEvent")) {
                mRenderPositionEvent = options.find<sp<RenderPositionEvent> >("RenderPositionEvent");
            }
            
            if (options.contains("RenderPositionUpdateInterval")) {
                mRenderPositionUpdateInterval = options.findInt64("RenderPositionUpdateInterval");
            }

            if (options.contains("StatusEvent")) {
                mStatusEvent = options.find<sp<StatusEvent> >("StatusEvent");
            }
        }

        virtual ~MPContext() {
            mSessions.clear();
        }

        // TODO: set status and put state machine into invalid
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
            sp<Looper> looper = Looper::getLooper();
            MPContext *mpc = static_cast<MPContext*>(looper->opaque());

            // send render position info to client
            MediaTime ts = mpc->mClock->get();
            INFO("current progress %.3f(s)", ts.seconds());
            mpc->mRenderPositionEvent->fire(ts);

            if (mpc->mClock->isPaused() == false) {
                looper->post(new UpdateRenderPosition,
                             mpc->mRenderPositionUpdateInterval);
            }
        }
    };

    // for media session update render position
    struct OnUpdateRenderPosition : public RenderPositionEvent {
        const size_t mID;
        OnUpdateRenderPosition(const sp<Looper>& lp, size_t id) : RenderPositionEvent(lp), mID(id) { }

        virtual void onEvent(const MediaTime& ts) {
            sp<Looper> looper = Looper::getLooper();
            MPContext *mpc = static_cast<MPContext*>(looper->opaque());

            if (ts == kTimeEnd) {
                INFO("eos...");
                if (mpc->mRenderPositionEvent != NULL) {
                    mpc->mRenderPositionEvent->fire(kTimeEnd);
                }
            }
        }
    };
    
    static void prepareMedia(const sp<Looper>& looper, MPContext* mpc, const Message& media) {
        
        const char * url = media.findString("url");
        INFO("prepare media %s", url);
        
        sp<Content> pipe = Content::Create(url);
        if (pipe == NULL) {
            ERROR("create pipe failed");
            mpc->setStatus(UNKNOWN_ERROR);
            return;
        }
        
        sp<MediaExtractor> extractor = MediaExtractor::Create(pipe, NULL);
        if (extractor == NULL || extractor->status() != OK) {
            ERROR("create extractor failed");
            mpc->setStatus(UNKNOWN_ERROR);
            return;
        }
        
        eModeType mode = (eModeType)media.findInt32(kKeyMode, kModeTypeNormal);
        
        sp<MediaOut> external;
        if (media.contains("MediaOut")) {
            external = media.find<sp<MediaOut> >("MediaOut");
        }
        
#if 0
        if (media.contains("SDL_Window")) {
            nativeWindow = media.findPointer("SDL_Window");
        }
#endif
        //double startTimeUs = options.findDouble("StartTime");
        //double endTimeUs = options.findDouble("EndTime");
        
        Message fileFormats = extractor->formats();
        size_t numTracks = fileFormats.findInt32(kKeyCount);
        
        uint32_t activeTracks = 0;
        for (size_t i = 0; i < numTracks; ++i) {
            // PacketRequestEvent
            sp<MediaSource> ms = new MediaSource(looper, extractor, i);
            
            String name = String::format("track-%zu", i);
            const Message& formats = fileFormats.find<Message>(name);
            
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
            options.setInt32(kKeyMode, mode);
            if (type == kCodecTypeVideo) {
                if (external != NULL) {
                    options.set<sp<MediaOut> >("MediaOut", external);
                }
            }
            
            options.set<sp<PacketRequestEvent> >("PacketRequestEvent", ms);
            
            options.set<sp<RenderPositionEvent> >("RenderPositionEvent",
                                                  new OnUpdateRenderPosition(looper, mpc->mNextId));
            
            if (kCodecTypeAudio == type || numTracks == 1) {
                options.set<sp<Clock> >("Clock", new Clock(mpc->mClock, kClockRoleMaster));
            } else {
                options.set<sp<Clock> >("Clock", new Clock(mpc->mClock));
            }
            
            sp<MediaSession> session = MediaSessionCreate(formats, options);
            if (session == NULL) {
                ERROR("create session for %s[%zu] failed", url, i);
                continue;
            }
            
            // save this packet queue
            mpc->mSessions.insert(mpc->mNextId++, SessionContext(codec, session));
            activeTracks |= (1<<i);
        }
        
        // tell extractor which tracks are enabled.
        Message config;
        config.setInt32(kKeyMask, activeTracks);
        extractor->configure(config);
    }

    static struct StateLink {
        eStateType  from;
        eStateType  to;
    } kStateLinks[] = {
        { kStateInvalid,        kStateInitial   },  // add media
        { kStateInitial,        kStateReady     },  // prepare
        { kStateReady,          kStatePlaying   },  // start
        { kStatePlaying,        kStateIdle      },  // pause
        { kStateIdle,           kStateFlushed   },  // flush
        { kStateFlushed,        kStateReleased  },  // release
        // start
        { kStateIdle,           kStatePlaying   },  // start @ paused
        // prepare
        { kStateFlushed,        kStateReady     },  // prepare @ flushed
        // flush
        { kStateReady,          kStateFlushed   },  // flush @ ready
        // release
        { kStateInitial,        kStateReleased  },  // release @ init
    };
#define NELEM(x)    (sizeof(x) / sizeof(x[0]))

    static status_t checkStateTransition(eStateType from, eStateType to) {
        for (size_t i = 0; i < NELEM(kStateLinks); ++i) {
            if (kStateLinks[i].from == from && kStateLinks[i].to == to) {
                return OK;
            }
        }
        return INVALID_OPERATION;
    }

    struct State {
        const eStateType kState;
        State(eStateType state) : kState(state) { }
        virtual ~State() { }

        virtual void onLeaveState() = 0;
        virtual void onEnterState(const Message& payload) = 0;
    };

    struct InvalidState : public State {
        InvalidState() : State(kStateInvalid) { }
        virtual void onLeaveState() {
            // NOTHING
        }
        virtual void onEnterState(const Message& payload) {
            // NOTHING
        }
    };

    struct InitialState : public State {
        InitialState() : State(kStateInitial) { }
        virtual void onLeaveState() {
            // NOTHING
        }

        virtual void onEnterState(const Message& media) {
            // -> init by add media
            sp<Looper> looper = Looper::getLooper();
            MPContext *mpc = static_cast<MPContext*>(looper->opaque());

            if (media.contains(kKeyCount)) {
                size_t count = media.findInt32(kKeyCount);
                for (size_t i = 0; i < count; ++i) {
                    String name = String::format("media-%zu", i);
                    const Message& _media = media.find<Message>(name);
                    prepareMedia(looper, mpc, _media);
                }
            } else {
                prepareMedia(looper, mpc, media);
            }
        }
    };

    struct PrepareStatusEvent : public CountedStatusEvent {
        PrepareStatusEvent(const sp<Looper>& looper, size_t n) :
            CountedStatusEvent(looper, n) { }

        virtual void onFinished(status_t st) {
            MPContext *mpc = static_cast<MPContext*>(Looper::getLooper()->opaque());

            INFO("prepare finished with status %d, current pos %.3f(s)",
                    st, mpc->mClock->get().seconds());

            // notify client
            if (mpc->mStatusEvent != NULL)
                mpc->mStatusEvent->fire(st);

            // notify the render position
            if (mpc->mRenderPositionEvent != NULL)
                mpc->mRenderPositionEvent->fire(mpc->mClock->get());
        }
    };

    struct ReadyState : public State {
        ReadyState() : State(kStateReady) { }
        virtual void onLeaveState() {
            // NOTHING
        }

        virtual void onEnterState(const Message& payload) {
            // -> ready by prepare
            sp<Looper> looper = Looper::getLooper();
            MPContext *mpc = static_cast<MPContext*>(looper->opaque());
            
            MediaTime ts = payload.find<MediaTime>("time");

            // set clock time
            mpc->mClock->set(ts);

            // prepare sessions
            sp<PrepareStatusEvent> event = new PrepareStatusEvent(looper, mpc->mSessions.size());
            event->keep(event);

            ControlEventPayload pl = { kControlEventPrepare, ts, event };

            HashTable<size_t, SessionContext>::iterator it = mpc->mSessions.begin();
            for (; it != mpc->mSessions.end(); ++it) {
                SessionContext& sc = it.value();
                sc.mMediaSession->fire(pl);
            }
        }
    };

    struct PlayingState : public State {
        PlayingState() : State(kStatePlaying) { }
        virtual void onLeaveState() {
            // NOTHINGS
        }
        virtual void onEnterState(const Message& payload) {
            // -> playing by start
            sp<Looper> looper = Looper::getLooper();
            MPContext *mpc = static_cast<MPContext*>(looper->opaque());

            mpc->mClock->start();
            looper->post(new UpdateRenderPosition);
        }
    };

    struct IdleState : public State {
        IdleState() : State(kStateIdle) { }
        virtual void onLeaveState() {
            // NOTHING
        }
        virtual void onEnterState(const Message& payload) {
            // -> paused by pause
            sp<Looper> looper = Looper::getLooper();
            MPContext *mpc = static_cast<MPContext*>(looper->opaque());

            mpc->mClock->pause();
            
            // we may have to suspend codec after some time
        }
    };
    
    struct FlushedState : public State {
        FlushedState() : State(kStateFlushed) { }
        virtual void onLeaveState() {
            
        }
        virtual void onEnterState(const Message& payload) {
            sp<Looper> looper = Looper::getLooper();
            MPContext *mpc = static_cast<MPContext*>(looper->opaque());
            
            ControlEventPayload pl = { kControlEventFlush };
            
            HashTable<size_t, SessionContext>::iterator it = mpc->mSessions.begin();
            for (; it != mpc->mSessions.end(); ++it) {
                SessionContext& sc = it.value();
                sc.mMediaSession->fire(pl);
            }
            
            mpc->mClock->reset();
        }
    };
    
    struct ReleasedState : public State {
        ReleasedState() : State(kStateReleased) { }
        
        virtual void onLeaveState() {
            
        }
        
        virtual void onEnterState(const Message& payload) {
        }
    };
    
    static sp<State> sStates[kStateMax] = {
        new InvalidState,
        new InitialState,
        new ReadyState,
        new PlayingState,
        new IdleState,
        new FlushedState,
        new ReleasedState,
    };

    struct StateMachine : public Looper {
        Mutex           mLock;
        eStateType      mState;

        StateMachine() : Looper("mp"), mState(kStateInvalid) {
            loop();
        }

        virtual ~StateMachine() {
            terminate();
        }

        eStateType state() const { return mState; }

        struct Transition : public Runnable {
            eStateType  mFrom;
            eStateType  mTo;
            Message     mPayload;
            Transition(eStateType from, eStateType to, const Message& payload) :
                mFrom(from), mTo(to), mPayload(payload) { }
            
            virtual void run() {
                sStates[mFrom]->onLeaveState();
                sStates[mTo]->onEnterState(mPayload);
            }
        };

        status_t setState(eStateType state, const Message& payload) {
            eStateType to = state;
            if (checkStateTransition(mState, state) != OK) {
                ERROR("invalid state transition %d => %d", mState, state);
                return INVALID_OPERATION;
            }
            sp<Runnable> runnable = new Transition(mState, to, payload);
            post(runnable);
            mState = state;
            return OK;
        }
    };

    MediaPlayer::MediaPlayer() {

    }

    MediaPlayer::~MediaPlayer() {
        MPContext *mpc = static_cast<MPContext*>(mSM->opaque());
        delete mpc;
    }

    sp<MediaPlayer> MediaPlayer::Create(const Message& options) {
        sp<MediaPlayer> mp = new MediaPlayer;

        mp->mSM = new StateMachine;
        mp->mSM->bind(new MPContext(options));

        return mp;
    }

    status_t MediaPlayer::addMedia(const Message& media) {
        INFO("add media @ %s", media.string().c_str());
        return mSM->setState(kStateInitial, media);
    }

    status_t MediaPlayer::prepare(const MediaTime& ts) {
        INFO("prepare @ %.3f(s)", ts.seconds());
        Message payload;
        payload.set<MediaTime>("time", ts);
        return mSM->setState(kStateReady, payload);
    }

    status_t MediaPlayer::start() {
        INFO("start");
        Message payload;
        return mSM->setState(kStatePlaying, payload);
    }

    status_t MediaPlayer::pause() {
        INFO("pause");
        Message payload;
        return mSM->setState(kStateIdle, payload);
    }
    
    status_t MediaPlayer::flush() {
        INFO("flush");
        Message payload;
        return mSM->setState(kStateFlushed, payload);
    }

    status_t MediaPlayer::release() {
        INFO("release");
        Message payload;
        return mSM->setState(kStateReleased, payload);
    }
}
