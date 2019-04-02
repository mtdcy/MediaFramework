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

#define LOG_TAG "Player"
//#define LOG_NDEBUG 0

#include <ABE/ABE.h>
#include <MediaFramework/MediaSession.h>
#include <MediaFramework/MediaExtractor.h>
#include <MediaFramework/MediaDecoder.h>
#include <MediaFramework/MediaOut.h>
#include <MediaFramework/MediaPlayer.h>

#include <SDL.h>

__USING_NAMESPACE_MPX

struct __ABE_HIDDEN CountedStatusEvent : public StatusEvent {
    volatile int mCount;
    sp<StatusEvent> kept;

    // FIXME:
    void keep(const sp<StatusEvent>& self) { kept = self; }

    CountedStatusEvent(const sp<Looper>& lp, size_t n) :
        StatusEvent(lp), mCount(n) { }

    virtual ~CountedStatusEvent() { }

    virtual void onEvent(const MediaError& st) {
        int old = atomic_sub(&mCount, 1);
        CHECK_GT(old, 0);
        if (old == 1 || st != kMediaNoError) {
            onFinished(st);
            kept.clear();
        }
    }

    virtual void onFinished(MediaError st) = 0;
};

// for MediaSession request packet, which always run in player's looper
struct __ABE_HIDDEN MediaSource : public PacketRequestEvent {
    sp<MediaExtractor>  mMedia;
    const size_t        mIndex;

    MediaSource(const sp<Looper>& lp, const sp<MediaExtractor>& media, size_t index) :
        PacketRequestEvent(lp), mMedia(media), mIndex(index) { }

    virtual void onEvent(const PacketRequest& v) {
        // NO lock to mMedia, as all PacketRequestEvent run in the same looper
        sp<MediaPacket> pkt = mMedia->read(mIndex, v.mode, v.ts);

        if (pkt == NULL) {
            INFO("%zu: eos", mIndex);
        }

        sp<PacketReadyEvent> event = v.event;
        event->fire(pkt);
    }
};

struct __ABE_HIDDEN SessionContext {
    eCodecFormat        mCodec;
    sp<IMediaSession>   mMediaSession;
    int64_t             mStartTime;
    int64_t             mEndTime;

    SessionContext(eCodecFormat codec, const sp<IMediaSession>& ms) :
        mCodec(codec),  mMediaSession(ms) { }

    ~SessionContext() { }
};

struct __ABE_HIDDEN MPContext {
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
            mRenderPositionEvent = options.findObject("RenderPositionEvent");
        }

        if (options.contains("RenderPositionUpdateInterval")) {
            mRenderPositionUpdateInterval = options.findInt64("RenderPositionUpdateInterval");
        }

        if (options.contains("StatusEvent")) {
            mStatusEvent = options.findObject("StatusEvent");
        }
    }

    virtual ~MPContext() {
        mSessions.clear();
    }
};

// update render position to client
struct UpdateRenderPosition : public Runnable {
    UpdateRenderPosition() : Runnable() { }

    virtual void run() {
        sp<Looper> looper = Looper::Current();
        MPContext *mpc = static_cast<MPContext*>(looper->user(0));

        // send render position info to client
        MediaTime ts = mpc->mClock->get();
        DEBUG("current progress %.3f(s)", ts.seconds());
        mpc->mRenderPositionEvent->fire(ts);

        if (mpc->mClock->isPaused() == false) {
            looper->post(new UpdateRenderPosition,
                    mpc->mRenderPositionUpdateInterval);
        }
    }
};

// for media session update render position
struct __ABE_HIDDEN OnUpdateRenderPosition : public RenderPositionEvent {
    const size_t mID;
    OnUpdateRenderPosition(const sp<Looper>& lp, size_t id) : RenderPositionEvent(lp), mID(id) { }

    virtual void onEvent(const MediaTime& ts) {
        sp<Looper> looper = Looper::Current();
        MPContext *mpc = static_cast<MPContext*>(looper->user(0));

        if (ts == kTimeEnd) {
            INFO("eos...");
            if (mpc->mRenderPositionEvent != NULL) {
                mpc->mRenderPositionEvent->fire(kTimeEnd);
            }
        }
    }
};

static MediaError prepareMedia(const sp<Looper>& looper, MPContext* mpc, const Message& media) {

    const char * url = media.findString("url");
    INFO("prepare media %s", url);

    sp<Content> pipe = Content::Create(url);
    if (pipe == NULL) {
        ERROR("create pipe failed");
        return kMediaErrorBadFormat;
    }

    sp<MediaExtractor> extractor = MediaExtractor::Create(MediaFormatDetect(*pipe));

    Message options;
    if (extractor == NULL || extractor->init(pipe, options) != kMediaNoError) {
        ERROR("create extractor failed");
        return kMediaErrorBadFormat;
    }

    eModeType mode = (eModeType)media.findInt32(kKeyMode, kModeTypeNormal);

    sp<MediaOut> external;
    if (media.contains("MediaOut")) {
        external = media.findObject("MediaOut");
    }

#if 0
    if (media.contains("SDL_Window")) {
        nativeWindow = media.findPointer("SDL_Window");
    }
#endif
    //double startTimeUs = options.findDouble("StartTime");
    //double endTimeUs = options.findDouble("EndTime");

    Message fileFormats = extractor->formats();
    size_t numTracks = fileFormats.findInt32(kKeyCount, 1);

    uint32_t activeTracks = 0;
    for (size_t i = 0; i < numTracks; ++i) {
        // PacketRequestEvent
        sp<MediaSource> ms = new MediaSource(looper, extractor, i);

        String name = String::format("track-%zu", i);
        const Message& formats = fileFormats.find<Message>(name);

        DEBUG("session %zu: %s", i, formats.string().c_str());

        eCodecFormat codec = (eCodecFormat)formats.findInt32(kKeyFormat);
        if (codec == kCodecFormatUnknown) {
            ERROR("ignore unknown codec");
            continue;
        }

        eCodecType type = GetCodecType(codec);
        if (type == kCodecTypeAudio) {
            if (formats.findInt32(kKeySampleRate) == 0 ||
                    formats.findInt32(kKeyChannels) == 0) {
                ERROR("missing mandatory format infomation, playback may be broken");
            }

            if (mpc->mHasAudio) {
                INFO("ignore this audio");
                continue;
            } else {
                mpc->mHasAudio = true;
            }
        } else if (type == kCodecTypeVideo) {
            if (formats.findInt32(kKeyWidth) == 0 ||
                    formats.findInt32(kKeyHeight) == 0) {
                ERROR("missing mandatory format infomation, playback may be broken");
            }
        }

        Message options;
        options.setInt32(kKeyMode, mode);
        if (type == kCodecTypeVideo) {
            if (external != NULL) {
                options.setObject("MediaOut", external);
            }
        }
        options.setInt32(kKeyRequestFormat, kPixelFormatNV12);
        options.setInt32(kKeyOpenGLCompatible, true);
        options.setObject("PacketRequestEvent", ms);

        options.setObject("RenderPositionEvent",
                new OnUpdateRenderPosition(looper, mpc->mNextId));

        if (kCodecTypeAudio == type || numTracks == 1) {
            options.setObject("Clock", new Clock(mpc->mClock, kClockRoleMaster));
        } else {
            options.setObject("Clock", new Clock(mpc->mClock));
        }

        sp<IMediaSession> session = IMediaSession::Create(formats, options);
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
    if (extractor->configure(config) != kMediaNoError) {
        WARN("extractor not support configure active tracks");
    }
    return kMediaNoError;
}

struct __ABE_HIDDEN State : public SharedObject {
    const eStateType kState;
    State(eStateType state) : kState(state) { }
    virtual ~State() { }

    virtual MediaError onLeaveState() = 0;
    virtual MediaError onEnterState(const Message& payload) = 0;
};

struct __ABE_HIDDEN InvalidState : public State {
    InvalidState() : State(kStateInvalid) { }
    virtual MediaError onLeaveState() {
        // NOTHING
        return kMediaNoError;
    }
    virtual MediaError onEnterState(const Message& payload) {
        // NOTHING
        return kMediaNoError;
    }
};

struct __ABE_HIDDEN InitialState : public State {
    InitialState() : State(kStateInitial) { }
    virtual MediaError onLeaveState() {
        // NOTHING
        return kMediaNoError;
    }

    virtual MediaError onEnterState(const Message& media) {
        // -> init by add media
        sp<Looper> looper = Looper::Current();
        MPContext *mpc = static_cast<MPContext*>(looper->user(0));

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
        return kMediaNoError;
    }
};

struct __ABE_HIDDEN PrepareStatusEvent : public CountedStatusEvent {
    PrepareStatusEvent(const sp<Looper>& looper, size_t n) :
        CountedStatusEvent(looper, n) { }

    virtual void onFinished(MediaError st) {
        MPContext *mpc = static_cast<MPContext*>(Looper::Current()->user(0));

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

struct __ABE_HIDDEN ReadyState : public State {
    ReadyState() : State(kStateReady) { }
    virtual MediaError onLeaveState() {
        // NOTHING
        return kMediaNoError;
    }

    virtual MediaError onEnterState(const Message& payload) {
        // -> ready by prepare
        sp<Looper> looper = Looper::Current();
        MPContext *mpc = static_cast<MPContext*>(looper->user(0));

        MediaTime ts = payload.find<MediaTime>("time");

        // set clock time
        mpc->mClock->set(ts);

        // prepare sessions
        sp<PrepareStatusEvent> event = new PrepareStatusEvent(looper, mpc->mSessions.size());
        event->keep(event);

        Message options;
        options.set<MediaTime>("time", ts);
        options.setObject("StatusEvent", event);

        HashTable<size_t, SessionContext>::iterator it = mpc->mSessions.begin();
        for (; it != mpc->mSessions.end(); ++it) {
            SessionContext& sc = it.value();
            sc.mMediaSession->prepare(options);
        }
        return kMediaNoError;
    }
};

struct __ABE_HIDDEN PlayingState : public State {
    PlayingState() : State(kStatePlaying) { }
    virtual MediaError onLeaveState() {
        // NOTHINGS
        return kMediaNoError;
    }
    virtual MediaError onEnterState(const Message& payload) {
        // -> playing by start
        sp<Looper> looper = Looper::Current();
        MPContext *mpc = static_cast<MPContext*>(looper->user(0));

        mpc->mClock->start();
        looper->post(new UpdateRenderPosition);
        return kMediaNoError;
    }
};

struct __ABE_HIDDEN IdleState : public State {
    IdleState() : State(kStateIdle) { }
    virtual MediaError onLeaveState() {
        // NOTHING
        return kMediaNoError;
    }
    virtual MediaError onEnterState(const Message& payload) {
        // -> paused by pause
        sp<Looper> looper = Looper::Current();
        MPContext *mpc = static_cast<MPContext*>(looper->user(0));

        mpc->mClock->pause();

        // we may have to suspend codec after some time
        return kMediaNoError;
    }
};

struct __ABE_HIDDEN FlushedState : public State {
    FlushedState() : State(kStateFlushed) { }
    virtual MediaError onLeaveState() {
        return kMediaNoError;
    }
    virtual MediaError onEnterState(const Message& payload) {
        sp<Looper> looper = Looper::Current();
        MPContext *mpc = static_cast<MPContext*>(looper->user(0));

        HashTable<size_t, SessionContext>::iterator it = mpc->mSessions.begin();
        for (; it != mpc->mSessions.end(); ++it) {
            SessionContext& sc = it.value();
            sc.mMediaSession->flush();
        }

        mpc->mClock->reset();
        return kMediaNoError;
    }
};

struct __ABE_HIDDEN ReleasedState : public State {
    ReleasedState() : State(kStateReleased) { }

    virtual MediaError onLeaveState() {
        return kMediaNoError;
    }

    virtual MediaError onEnterState(const Message& payload) {
        sp<Looper> looper = Looper::Current();
        MPContext *mpc = static_cast<MPContext*>(looper->user(0));
        mpc->mSessions.clear();
        return kMediaNoError;
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

static const char * kNames[kStateMax] = {
    "Invalid",
    "Initial",
    "Ready",
    "Playing",
    "Idle",
    "Flushed",
    "Released"
};

static struct StateLink {
    eStateType  from;
    eStateType  to;
} kStateLinks[] = {
    { kStateInvalid,        kStateInitial   },  // init
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
    { kStatePlaying,        kStateFlushed   },  // flush @ playing
    // release
    { kStateInitial,        kStateReleased  },  // release @ init
};
#define NELEM(x)    (sizeof(x) / sizeof(x[0]))

static inline MediaError checkStateTransition(eStateType from, eStateType to) {
    for (size_t i = 0; i < NELEM(kStateLinks); ++i) {
        if (kStateLinks[i].from == from && kStateLinks[i].to == to) {
            return kMediaNoError;
        }
    }
    return kMediaErrorInvalidOperation;
}

struct __ABE_HIDDEN AVPlayer : public IMediaPlayer {
    sp<Looper>      mLooper;
    mutable Mutex   mLock;
    eStateType      mLastState;
    eStateType      mState;

    struct Transition : public Runnable {
        eStateType  mFrom;
        eStateType  mTo;
        Message     mOptions;
        Transition(eStateType from, eStateType to, const Message& options) :
            mFrom(from), mTo(to), mOptions(options) { }

        virtual void run() {
            INFO("transition %s => %s", kNames[mFrom], kNames[mTo]);
            sStates[mFrom]->onLeaveState();
            sStates[mTo]->onEnterState(mOptions);
        }
    };

    MediaError setState_l(eStateType state, const Message& options) {
        if (checkStateTransition(mState, state) != kMediaNoError) {
            ERROR("invalid state transition %s => %s", kNames[mState], kNames[state]);
            return kMediaErrorInvalidOperation;
        }
        mLooper->post(new Transition(mState, state, options));
        mLastState  = mState;
        mState      = state;
        return kMediaNoError;
    }

    AVPlayer(const Message& options) : IMediaPlayer(), mLooper(Looper::Create("mp")), mState(kStateInvalid) {
        mLooper->profile();
        mLooper->loop();
        MPContext *mpc = new MPContext(options);
        mLooper->bind(mpc);
    }

    virtual ~AVPlayer() {
        INFO("release AVPlayer");
        MPContext *mpc = static_cast<MPContext*>(mLooper->user(0));
        mLooper->terminate(true);
        delete mpc;
    }

    virtual eStateType state() const {
        AutoLock _l(mLock);
        return mState;
    }

    virtual MediaError init(const Message& media) {
        AutoLock _l(mLock);
        return setState_l(kStateInitial, media);
    }

    virtual MediaError prepare(const MediaTime& ts) {
        AutoLock _l(mLock);
        INFO("prepare @ %.3f(s)", ts.seconds());
        Message payload;
        payload.set<MediaTime>("time", ts);
        return setState_l(kStateReady, payload);
    }

    virtual MediaError start() {
        AutoLock _l(mLock);
        INFO("start");
        return setState_l(kStatePlaying, Message());
    }

    virtual MediaError pause() {
        AutoLock _l(mLock);
        INFO("pause");
        return setState_l(kStateIdle, Message());
    }

    virtual MediaError flush() {
        AutoLock _l(mLock);
        INFO("flush");
        return setState_l(kStateFlushed, Message());
    }

    virtual MediaError release() {
        AutoLock _l(mLock);
        INFO("release");
        return setState_l(kStateReleased, Message());
        // FIXME: release should work as blocked
    }
};

sp<IMediaPlayer> IMediaPlayer::Create(const Message &options) {
    return new AVPlayer(options);
}

