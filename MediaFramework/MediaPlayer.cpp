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
#include "MediaPlayer.h"
#include "MediaSession.h"
#include "MediaExtractor.h"
#include "MediaClock.h"
#include "MediaDecoder.h"
#include "MediaOut.h"

__USING_NAMESPACE_MPX

struct CountedStatusEvent : public StatusEvent {
    Atomic<int>     mCount;
    sp<StatusEvent> kept;

    // FIXME:
    void keep(const sp<StatusEvent>& self) { kept = self; }

    CountedStatusEvent(const sp<Looper>& lp, size_t n) :
        StatusEvent(lp), mCount(n) { }

    virtual ~CountedStatusEvent() { }

    virtual void onEvent(const MediaError& st) {
        int count = --mCount;
        CHECK_GE(count, 0);
        if (count == 0 || st != kMediaNoError) {
            onFinished(st);
            kept.clear();
        }
    }

    virtual void onFinished(MediaError st) = 0;
};

// for MediaSession request packet, which always run in player's looper
struct MediaSource : public PacketRequestEvent {
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

struct SessionContext : public SharedObject {
    eCodecFormat        mCodec;
    sp<IMediaSession>   mMediaSession;
    int64_t             mStartTime;
    int64_t             mEndTime;

    SessionContext(eCodecFormat codec, const sp<IMediaSession>& ms) :
        mCodec(codec),  mMediaSession(ms) { }

    ~SessionContext() {
        if (mMediaSession != NULL) mMediaSession->release();
        mMediaSession.clear();
    }
};

struct MPContext : public SharedObject {
    // external static context
    sp<InfomationEvent>     mInfomationEvent;
    sp<StatusEvent>         mStatusEvent;
    sp<Message>             mInfo;
    sp<MediaFrameEvent>     mVideoFrameEvent;
    sp<MediaFrameEvent>     mAudioFrameEvent;

    // mutable context
    HashTable<size_t, sp<SessionContext> > mSessions;
    size_t          mNextId;
    bool            mHasAudio;
    Bitmask         mMask;

    // clock
    sp<SharedClock> mClock;

    MPContext(const sp<Message>& options) : SharedObject(),
        // external static context
        mInfomationEvent(NULL),
        mStatusEvent(NULL),
        // internal static context
        // mutable context
        mNextId(0),mHasAudio(false),
        // clock
        mClock(new SharedClock)
    {
        INFO("init MPContext with %s", options->string().c_str());
        if (options->contains("InfomationEvent")) {
            mInfomationEvent = options->findObject("InfomationEvent");
        }

        if (options->contains("StatusEvent")) {
            mStatusEvent = options->findObject("StatusEvent");
        }
        
        if (options->contains("MediaFrameEvent")) {
            mVideoFrameEvent = options->findObject("MediaFrameEvent");
        }
    }

    ~MPContext() {
        mSessions.clear();
    }
    
    void notify(eInfoType info) {
        if (mInfomationEvent != NULL) {
            mInfomationEvent->fire(info);
        }
    }
};

struct OnInfomation : public InfomationEvent {
    OnInfomation() : InfomationEvent(Looper::Current()) { }
    virtual void onEvent(const eInfoType& info) {
        // TODO
    }
};

static MediaError prepareMedia(sp<MPContext>& mpc, const sp<Message>& media) {

    const char * url = media->findString("url");
    INFO("prepare media %s", url);

    sp<Content> pipe = Content::Create(url);
    if (pipe == NULL) {
        ERROR("create pipe failed");
        return kMediaErrorBadFormat;
    }

    sp<MediaExtractor> extractor = MediaExtractor::Create(MediaFormatDetect(*pipe));

    sp<Message> options = new Message;
    if (extractor == NULL || extractor->init(pipe, options) != kMediaNoError) {
        ERROR("create extractor failed");
        return kMediaErrorBadFormat;
    }

    eModeType mode = (eModeType)media->findInt32(kKeyMode, kModeTypeNormal);

    //double startTimeUs = options.findDouble("StartTime");
    //double endTimeUs = options.findDouble("EndTime");

    sp<Message> fileFormats = extractor->formats();
    size_t numTracks = fileFormats->findInt32(kKeyCount, 1);
    INFO("%s", fileFormats->string().c_str());
    mpc->mInfo = fileFormats->dup();

    uint32_t activeTracks = 0;
    for (size_t i = 0; i < numTracks; ++i) {
        // PacketRequestEvent
        sp<MediaSource> ms = new MediaSource(Looper::Current(), extractor, i);

        String name = String::format("track-%zu", i);
        sp<Message> formats = fileFormats->findObject(name);

        DEBUG("session %zu: %s", i, formats.string().c_str());

        eCodecFormat codec = (eCodecFormat)formats->findInt32(kKeyFormat);
        if (codec == kCodecFormatUnknown) {
            ERROR("ignore unknown codec");
            continue;
        }

        eCodecType type = GetCodecType(codec);
        if (type == kCodecTypeAudio) {
            if (formats->findInt32(kKeySampleRate) == 0 ||
                formats->findInt32(kKeyChannels) == 0) {
                ERROR("missing mandatory format infomation, playback may be broken");
            }

            if (mpc->mHasAudio) {
                INFO("ignore this audio");
                continue;
            } else {
                mpc->mHasAudio = true;
            }
        } else if (type == kCodecTypeVideo) {
            if (formats->findInt32(kKeyWidth) == 0 ||
                formats->findInt32(kKeyHeight) == 0) {
                ERROR("missing mandatory format infomation, playback may be broken");
            }
        }

        sp<Message> options = new Message;
        options->setInt32(kKeyMode, mode);
        if (type == kCodecTypeVideo) {
            if (mpc->mVideoFrameEvent != NULL) {
                options->setObject("MediaFrameEvent", mpc->mVideoFrameEvent);
            }
            options->setInt32(kKeyRequestFormat, kPixelFormatNV12);
        }
        options->setObject("PacketRequestEvent", ms);
        options->setObject("InformationEvent", new OnInfomation);

        if (kCodecTypeAudio == type || numTracks == 1) {
            options->setObject("Clock", new Clock(mpc->mClock, kClockRoleMaster));
        } else {
            options->setObject("Clock", new Clock(mpc->mClock));
        }

        sp<IMediaSession> session = IMediaSession::Create(formats, options);
        if (session == NULL) {
            ERROR("create session for %s[%zu] failed", url, i);
            continue;
        }

        // save this packet queue
        mpc->mSessions.insert(mpc->mNextId++,
                              new SessionContext(codec, session));
        activeTracks |= (1<<i);
    }

    // tell extractor which tracks are enabled.
    sp<Message> config = new Message;
    config->setInt32(kKeyMask, activeTracks);
    if (extractor->configure(config) != kMediaNoError) {
        WARN("extractor not support configure active tracks");
    }
    return kMediaNoError;
}

struct State : public SharedObject {
    const eStateType kState;
    State(eStateType state) : kState(state) { }
    virtual ~State() { }

    virtual MediaError onLeaveState() = 0;
    virtual MediaError onEnterState(const sp<Message>& payload) = 0;
};

struct InvalidState : public State {
    InvalidState() : State(kStateInvalid) { }
    virtual MediaError onLeaveState() {
        // NOTHING
        return kMediaNoError;
    }
    virtual MediaError onEnterState(const sp<Message>& payload) {
        // NOTHING
        return kMediaNoError;
    }
};

struct InitialState : public State {
    InitialState() : State(kStateInitial) { }
    virtual MediaError onLeaveState() {
        // NOTHING
        return kMediaNoError;
    }
    
    // -> init by add media
    virtual MediaError onEnterState(const sp<Message>& payload) {
        sp<Message> media = payload->findObject("media");
        sp<Message> options = payload->findObject("options");
        
        sp<MPContext> mpc = new MPContext(options);
        Looper::Current()->bind(mpc->RetainObject());

        if (media->contains(kKeyCount)) {
            size_t count = media->findInt32(kKeyCount);
            for (size_t i = 0; i < count; ++i) {
                String name = String::format("media-%zu", i);
                sp<Message> _media = media->findObject(name);
                prepareMedia(mpc, _media);
            }
        } else {
            prepareMedia(mpc, media);
        }
        
        mpc->notify(kInfoPlayerInitialized);
        return kMediaNoError;
    }
};

struct PrepareStatusEvent : public CountedStatusEvent {
    PrepareStatusEvent(const sp<Looper>& looper, size_t n) :
        CountedStatusEvent(looper, n) { }

    virtual void onFinished(MediaError st) {
        sp<MPContext> mpc = Looper::Current()->user(0);

        INFO("prepare finished with status %d, current pos %.3f(s)",
                st, mpc->mClock->get() / 1E6);

        // notify client
        if (mpc->mStatusEvent != NULL)
            mpc->mStatusEvent->fire(st);
    }
};

struct ReadyState : public State {
    ReadyState() : State(kStateReady) { }
    virtual MediaError onLeaveState() {
        // NOTHING
        return kMediaNoError;
    }

    virtual MediaError onEnterState(const sp<Message>& payload) {
        // -> ready by prepare
        sp<MPContext> mpc = Looper::Current()->user(0);

        int64_t ts = payload->findInt64("time");

        // set clock time
        mpc->mClock->set(ts);

        // prepare sessions
        sp<PrepareStatusEvent> event = new PrepareStatusEvent(Looper::Current(), mpc->mSessions.size());
        event->keep(event);

        sp<Message> options = new Message;
        options->setInt64("time", ts);
        options->setObject("StatusEvent", event);

        HashTable<size_t, sp<SessionContext> >::iterator it = mpc->mSessions.begin();
        for (; it != mpc->mSessions.end(); ++it) {
            sp<SessionContext>& sc = it.value();
            sc->mMediaSession->prepare(options);
        }
        
        mpc->notify(kInfoPlayerReady);
        return kMediaNoError;
    }
};

struct PlayingState : public State {
    PlayingState() : State(kStatePlaying) { }
    virtual MediaError onLeaveState() {
        // NOTHINGS
        return kMediaNoError;
    }
    virtual MediaError onEnterState(const sp<Message>& payload) {
        // -> playing by start
        sp<MPContext> mpc = Looper::Current()->user(0);

        mpc->mClock->start();
        
        mpc->notify(kInfoPlayerPlaying);
        return kMediaNoError;
    }
};

struct IdleState : public State {
    IdleState() : State(kStateIdle) { }
    virtual MediaError onLeaveState() {
        // NOTHING
        return kMediaNoError;
    }
    virtual MediaError onEnterState(const sp<Message>& payload) {
        // -> paused by pause
        sp<MPContext> mpc = Looper::Current()->user(0);

        mpc->mClock->pause();

        // we may have to suspend codec after some time
        
        mpc->notify(kInfoPlayerPaused);
        return kMediaNoError;
    }
};

struct FlushedState : public State {
    FlushedState() : State(kStateFlushed) { }
    virtual MediaError onLeaveState() {
        return kMediaNoError;
    }
    virtual MediaError onEnterState(const sp<Message>& payload) {
        sp<MPContext> mpc = Looper::Current()->user(0);

        HashTable<size_t, sp<SessionContext> >::iterator it = mpc->mSessions.begin();
        for (; it != mpc->mSessions.end(); ++it) {
            sp<SessionContext>& sc = it.value();
            sc->mMediaSession->flush();
        }

        mpc->mClock->reset();
        
        mpc->notify(kInfoPlayerFlushed);
        return kMediaNoError;
    }
};

struct ReleasedState : public State {
    ReleasedState() : State(kStateReleased) { }

    virtual MediaError onLeaveState() {
        return kMediaNoError;
    }

    virtual MediaError onEnterState(const sp<Message>& payload) {
        sp<MPContext> mpc = Looper::Current()->user(0);
        Looper::Current()->bind(NULL);
        mpc->ReleaseObject();       // release refs in looper
        
        mpc->mSessions.clear();
        mpc->notify(kInfoPlayerReleased);
        
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

struct AVPlayer : public IMediaPlayer {
    mutable Mutex   mLock;
    sp<Looper>      mLooper;
    eStateType      mState;

    struct Transition : public Runnable {
        eStateType      mFrom;
        eStateType      mTo;
        sp<Message>     mOptions;
        Transition(eStateType from, eStateType to, const sp<Message>& options) :
            mFrom(from), mTo(to), mOptions(options) { }

        virtual void run() {
            INFO("transition %s => %s", kNames[mFrom], kNames[mTo]);
            sStates[mFrom]->onLeaveState();
            sStates[mTo]->onEnterState(mOptions);
        }
    };

    MediaError setState_l(const eStateType state, const sp<Message>& payload) {
        // auto state transition:
        // 1. invalid -> init -> ready -> playing -> idle -> flush -> release
        // 2. ready -> flush -> release
        eStateType link[kStateMax];
        
        // find transition path
        link[0] = mState;
        size_t n = 1;
        for (; n < kStateMax; ++n) {
            bool complete = false;
            for (size_t i = 0; i < NELEM(kStateLinks); ++i) {
                if (kStateLinks[i].from == link[n-1]) {
                    if (kStateLinks[i].to == state) {
                        link[n] = state;
                        complete = true;
                        break;
                    } else {
                        if ((state - kStateLinks[i].to) < (state - link[n-1])) {
                            // find close path
                            link[n] = kStateLinks[i].to;
                        }
                    }
                }
            }
            if (complete) break;
        }
        
        if (n == kStateMax) {
            ERROR("bad state transition %s => %s timeout", kNames[mState], kNames[state]);
            return kMediaErrorBadValue;
        }
        
        INFO("%s", kNames[link[0]]);
        for (size_t i = 1; i <= n; ++i) {
            INFO("-> %s", kNames[link[i]]);
            
            sp<Runnable> routine = new Transition(link[i-1], link[i], payload);
            mLooper->post(routine);
            mState      = state;
        }
        return kMediaNoError;
    }

    AVPlayer() : IMediaPlayer(),  mLooper(NULL), mState(kStateInvalid) {
    }

    virtual ~AVPlayer() {
        CHECK_TRUE(mLooper == NULL); // make sure context released
    }

    virtual eStateType state() const {
        AutoLock _l(mLock);
        return mState;
    }
    
    virtual sp<Clock> clock() const {
        AutoLock _l(mLock);
        sp<MPContext> mpc = mLooper->user(0);
        if (mpc->mClock != NULL) {
            return new Clock(mpc->mClock);
        }
        return NULL;
    }
    
    virtual sp<Message> info() const {
        AutoLock _l(mLock);
        sp<MPContext> mpc = mLooper->user(0);
        return mpc->mInfo;
    }

    virtual MediaError init(const sp<Message>& media, const sp<Message>& options) {
        AutoLock _l(mLock);
        mLooper = Looper::Create("avplayer");
        mLooper->loop();
        
        sp<Message> payload = new Message;
        payload->setObject("media", media);
        payload->setObject("options", options);
        return setState_l(kStateInitial, payload);
    }

    virtual MediaError prepare(int64_t us) {
        AutoLock _l(mLock);
        if (us < 0) us = 0;
        INFO("prepare @ %.3f(s)", us / 1E6);
        sp<Message> payload = new Message;
        payload->setInt64("time", us);
        
        if (mState < kStateInitial && mState == kStateReleased) {
            ERROR("prepare at bad state");
            return kMediaErrorInvalidOperation;
        }
        
        // prepare can perform @ init/ready/play/idle/flushed
        mLooper->post(new Transition(mState, kStateReady, payload));
        if (mState == kStateInitial || mState == kStateFlushed) {
            mState = kStateReady;
        }
        return kMediaNoError;
    }

    virtual MediaError start() {
        AutoLock _l(mLock);
        INFO("start");
        sp<Message> payload = new Message;
        return setState_l(kStatePlaying, payload);
    }

    virtual MediaError pause() {
        AutoLock _l(mLock);
        INFO("pause");
        sp<Message> payload = new Message;
        return setState_l(kStateIdle, payload);
    }

    virtual MediaError flush() {
        AutoLock _l(mLock);
        INFO("flush");
        sp<Message> payload = new Message;
        return setState_l(kStateFlushed, payload);
    }

    virtual MediaError release() {
        AutoLock _l(mLock);
        INFO("release");
        sp<Message> payload = new Message;
        MediaError st = setState_l(kStateReleased, payload);
        mLooper->terminate(true);
        mLooper.clear();
        return st;
    }
};

sp<IMediaPlayer> IMediaPlayer::Create() {
    return new AVPlayer();
}
