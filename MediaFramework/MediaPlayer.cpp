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

    struct MPContext {
        // external static context
        sp<RenderPositionEvent> mRenderPositionEvent;
        sp<StatusEvent> mStatusEvent;

        // mutable context
        HashTable<size_t, SessionContext> mSessions;
        size_t mNextId;
        bool mHasAudio;

        // clock
        sp<SharedClock> mClock;

        MPContext(const Message& options) :
            // external static context
            mRenderPositionEvent(NULL), mStatusEvent(NULL),
            // internal static context
            // mutable context
            mNextId(0),mHasAudio(false),
            // clock
            mClock(new SharedClock)
        {
            if (options.contains("RenderPositionEvent")) {
                mRenderPositionEvent = options.find<sp<RenderPositionEvent> >("RenderPositionEvent");
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

            if (mpc->mClock->isPaused()) {
                INFO("stop update progress");
                return;
            }

            MediaTime ts = mpc->mClock->get();
            INFO("current progress %.3f(s)", ts.seconds());

            mpc->mRenderPositionEvent->fire(ts);

            looper->post(new UpdateRenderPosition, PROGRESS_UPDATE_RATE);
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

    enum eStateType {
        kStateInvalid,
        kStateInit,
        kStateReady,
        kStatePlaying,
        kStatePaused,
        kStateStopped,
        kStateFlushed,
        kStateMax,
    };

    struct StateLink {
        eStateType  from;
        eStateType  to;
    } kStateLinks[] = {
        { kStateInvalid,        kStateInit      },  // add media
        { kStateInit,           kStateReady     },  // prepare
        { kStateReady,          kStatePlaying   },  // start
        { kStatePlaying,        kStateStopped   },  // stop
        { kStateStopped,        kStateFlushed   },  // flush
        { kStateFlushed,        kStateInvalid   },  // release
        // pause
        { kStatePlaying,        kStatePaused    },  // pause
        { kStatePaused,         kStatePlaying   },  // start @ paused
        { kStatePaused,         kStateStopped   },  // stop @ paused
        // prepare
        { kStateFlushed,        kStateReady     },  // prepare @ flushed
        // flush
        { kStateReady,          kStateFlushed   },  // flush @ ready
        // release
        { kStateInit,           kStateInvalid   },  // release @ init
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

    struct InitState : public State {
        InitState() : State(kStateInit) { }
        virtual void onLeaveState() {
            // NOTHING
        }

        virtual void onEnterState(const Message& media) {
            // -> init by add media
            sp<Looper> looper = Looper::getLooper();
            MPContext *mpc = static_cast<MPContext*>(looper->opaque());

            const char * url = media.findString("url");
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

            sp<RenderEvent> externalRenderer;
            void *nativeWindow = NULL;
            if (media.contains("RenderEvent")) {
                externalRenderer = media.find<sp<RenderEvent> >("RenderEvent");
            }

            if (media.contains("SDL_Window")) {
                nativeWindow = media.findPointer("SDL_Window");
            }

            //double startTimeUs = options.findDouble("StartTime");
            //double endTimeUs = options.findDouble("EndTime");

            Message fileFormats = extractor->formats();
            size_t numTracks = fileFormats.findInt32(kKeyCount);

            for (size_t i = 0; i < numTracks; ++i) {
                // PacketRequestEvent
                sp<MediaSource> ms = new MediaSource(looper, extractor, i);

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

                options.set<sp<PacketRequestEvent> >("PacketRequestEvent", ms);

                options.set<sp<RenderPositionEvent> >("RenderPositionEvent",
                        new OnUpdateRenderPosition(looper, mpc->mNextId));

                if (kCodecTypeAudio == type || numTracks == 1) {
                    options.set<sp<Clock> >("Clock", mpc->mClock->getClock(kClockRoleMaster));
                } else {
                    options.set<sp<Clock> >("Clock", mpc->mClock->getClock());
                }

                sp<MediaSession> session = MediaSessionCreate(formats, options);
                if (session == NULL) {
                    ERROR("create session for %s[%zu] failed", url, i);
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
            MPContext *mpc = static_cast<MPContext*>(Looper::getLooper()->opaque());

            INFO("prepare finished with status %d, current pos %.3f(s)",
                    st, mpc->mClock->get().seconds());

            // notify client
            mpc->mStatusEvent->fire(st);

            // notify the render position
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
            MediaTime ts = payload.find<MediaTime>("time");

            sp<Looper> looper = Looper::getLooper();
            MPContext *mpc = static_cast<MPContext*>(looper->opaque());

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

    struct PausedState : public State {
        PausedState() : State(kStatePaused) { }
        virtual void onLeaveState() {
            // NOTHING
        }
        virtual void onEnterState(const Message& payload) {
            // -> paused by pause
            sp<Looper> looper = Looper::getLooper();
            MPContext *mpc = static_cast<MPContext*>(looper->opaque());

            mpc->mClock->pause();
        }
    };

    struct StoppedState : public State {
        StoppedState() : State(kStateStopped) { }
        virtual void onLeaveState() {

        }
        virtual void onEnterState(const Message& payload) {
            // -> stopped by stop
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
    
    struct FlushedState : public State {
        FlushedState() : State(kStateFlushed) { }
        virtual void onLeaveState() {
            
        }
        virtual void onEnterState(const Message& payload) {
            
        }
    };

    struct StateMachine : public Looper {
        Mutex           mLock;
        eStateType      mState;
        sp<State>       mStates[kStateMax];

        StateMachine() : Looper("mp"), mState(kStateInvalid) {
            mStates[kStateInvalid]  = new InvalidState;
            mStates[kStateInit]     = new InitState;
            mStates[kStateReady]    = new ReadyState;
            mStates[kStatePlaying]  = new PlayingState;
            mStates[kStatePaused]   = new PausedState;
            mStates[kStateStopped]  = new StoppedState;
            mStates[kStateFlushed]  = new FlushedState;

            loop();
        }

        virtual ~StateMachine() {
            terminate();
        }

        eStateType state() const { return mState; }

        struct StateRunnable : public Runnable {
            sp<State>   mFrom;
            sp<State>   mTo;
            Message     mPayload;
            StateRunnable(const sp<State>& from, const sp<State>& to, const Message& payload) :
                mFrom(from), mTo(to), mPayload(payload) { }
            virtual void run() {
                mFrom->onLeaveState();
                mTo->onEnterState(mPayload);
            }
        };

        void setState(eStateType state, const Message& payload) {
            eStateType to = state;
            if (checkStateTransition(mState, state) != OK) {
                ERROR("invalid state transition %d => %d", mState, state);
                to = kStateInvalid;
            }
            sp<Runnable> runnable = new StateRunnable(mStates[mState], mStates[to], payload);
            post(runnable);
            mState = state;
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
        mSM->setState(kStateInit, media);
        return OK;
    }

    status_t MediaPlayer::prepare(const MediaTime& ts) {
        INFO("prepare @ %.3f(s)", ts.seconds());
        Message payload;
        payload.set<MediaTime>("time", ts);
        mSM->setState(kStateReady, payload);
        return OK;
    }

    status_t MediaPlayer::start() {
        INFO("start");
        Message payload;
        mSM->setState(kStatePlaying, payload);
        return OK;
    }

    status_t MediaPlayer::pause() {
        INFO("pause");
        Message payload;
        mSM->setState(kStatePaused, payload);
        return OK;
    }

    status_t MediaPlayer::stop() {
        INFO("stop");
        Message payload;
        mSM->setState(kStateStopped, payload);
        return OK;
    }

}
