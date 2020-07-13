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

#define LOG_TAG "Tiger"
#define LOG_NDEBUG 0
#include "MediaPlayer.h"
#include "MediaSession.h"
#include "MediaDevice.h"
#include "MediaClock.h"

__BEGIN_NAMESPACE_MPX

static Bool IsRawFormat(UInt32 format) {
    switch (format) {
        case kSampleFormatU8:
        case kSampleFormatS16:
        case kSampleFormatS32:
        case kSampleFormatF32:
        case kSampleFormatF64:
        case kSampleFormatU8Packed:
        case kSampleFormatS16Packed:
        case kSampleFormatS32Packed:
        case kSampleFormatF32Packed:
        case kSampleFormatF64Packed:
            return True;
        default:
            return False;
    }
}

struct TrackContext : public SharedObject {
    eCodecType          mType;
    UInt32              mTrackIndex;
    sp<IMediaSession>   mMediaSource;
    sp<IMediaSession>   mDecodeSession;
    sp<IMediaSession>   mRenderSession;
    
    // for tunnel
    union {
        AudioFormat     mAudioFormat;
        ImageFormat     mVideoFormat;
    };
    sp<PacketRequestEvent>  mPacketRequestEvent;
    
    TrackContext() : mType(kCodecTypeUnknown), mTrackIndex(0) { }
    
    ~TrackContext() {
        mMediaSource.clear();
        mDecodeSession.clear();
        mRenderSession.clear();
    }
};

struct Tiger : public IMediaPlayer {
    // external static context
    sp<PlayerInfoEvent>     mInfoEvent;
    eModeType               mMode;
    sp<MediaFrameEvent>     mAudioFrameEvent;
    sp<MediaFrameEvent>     mVideoFrameEvent;
    void *                  mOpenGLContext;

    // internal static context
    sp<Job>                 mDeferStart;

    // mutable context
    sp<IMediaSession>       mMediaSource;
    sp<Message>             mFileFormats;
    UInt32                  mTrackID;
    HashTable<UInt32, sp<TrackContext> > mTracks;
    Bool                    mHasAudio;
    Bits<UInt32>          mReadyMask;     // set when not ready
    Bits<UInt32>          mEndMask;       // set when not eos
    enum eState { kInit, kReady, kEnd };
    eState                  mState;

    Tiger() : IMediaPlayer(new Looper("tiger")),
        // external static context
        mInfoEvent(Nil), mOpenGLContext(Nil),
        // internal static context
        mDeferStart(new DeferStart(this)),
        // mutable context
        mMediaSource(Nil), mTrackID(0),
        mHasAudio(False), mState(kInit) {
            
        }

    void notify(ePlayerInfoType info, const sp<Message>& payload = Nil) {
        if (mInfoEvent != Nil) {
            mInfoEvent->fire(info, payload);
        }
    }

    virtual void onInit(const sp<Message>& media, const sp<Message>& options) {
        DEBUG("onInit...");
        INFO("media => %s", media->string().c_str());
        INFO("options => %s", options->string().c_str());
        
        if (!options.isNil()) {
            if (options->contains(kKeyPlayerInfoEvent)) {
                mInfoEvent = options->findObject(kKeyPlayerInfoEvent);
            }

            mMode = (eModeType)options->findInt32(kKeyMode, kModeTypeNormal);

            if (media->contains(kKeyVideoFrameEvent)) {
                mVideoFrameEvent = media->findObject(kKeyVideoFrameEvent);
            }

            if (media->contains(kKeyAudioFrameEvent)) {
                mAudioFrameEvent = media->findObject(kKeyAudioFrameEvent);
            }
            
            if (options->contains(kKeyOpenGLContext)) {
                mOpenGLContext = options->findPointer(kKeyOpenGLContext);
            }
        }
    
        sp<Message> options0 = new Message;
        options0->setObject(kKeySessionInfoEvent, new OnSourceInfo(this));

        mMediaSource = IMediaSession::Create(media, options0);
    }

    struct OnSourceInfo : public SessionInfoEvent {
        Tiger *thiz;
        OnSourceInfo(Tiger *p) : SessionInfoEvent(p->mDispatch), thiz(p) { }

        virtual void onEvent(const eSessionInfoType& info, const sp<Message>& payload) {
            thiz->onSourceInfo(info, payload);
        }
    };

    void onSourceInfo(const eSessionInfoType& info, const sp<Message>& payload) {
        DEBUG("onSourceInfo %d", info);
        switch (info) {
            case kSessionInfoReady:
                onInitDecoders(payload);
                break;
            default:
                break;
        }
    }
    
    void onInitDecoders(const sp<Message>& formats) {
        DEBUG("onInitDecoders %s", formats->string().c_str());
        UInt32 numTracks = formats->findInt32(kKeyCount, 1);
        
        CHECK_TRUE(formats->contains(kKeyTrackSelectEvent));
        sp<TrackSelectEvent> selector = formats->findObject(kKeyTrackSelectEvent);
        
        // there is no need to use HashTable, but it help keep code clean
        HashTable<UInt32, Bool> selectedTracks;
        for (UInt32 i = 0; i < numTracks; ++i) {
            sp<Message> trackFormat = formats->findObject(kKeyTrack + i);

            DEBUG("track %zu: %s", i, trackFormat->string().c_str());

            CHECK_TRUE(trackFormat->contains(kKeyType));
            eCodecType type = (eCodecType)trackFormat->findInt32(kKeyType);
            Int32 codec = trackFormat->findInt32(kKeyFormat);
            
            CHECK_TRUE(trackFormat->findObject(kKeyPacketRequestEvent));
            sp<PacketRequestEvent> pre = trackFormat->findObject(kKeyPacketRequestEvent);
            // we don't want to export this to client, so remove it here.
            trackFormat->remove(kKeyPacketRequestEvent);
            
            if (selectedTracks.find(type)) continue;
            
            sp<TrackContext> track  = new TrackContext;
            track->mTrackIndex      = i;
            track->mType            = type;
            track->mMediaSource     = mMediaSource;

            sp<SessionInfoEvent> infoEvent = new OnDecoderInfo(this, mTrackID);
            if (IsRawFormat(codec)) {
                // NO codec required -> init MediaRenderer directly
                trackFormat->setObject(kKeyFrameRequestEvent, pre);
                infoEvent->fire(kSessionInfoReady, trackFormat);
                // > onInitRenderer
            } else {
                sp<Message> options = new Message;
                options->setInt32(kKeyMode, mMode);
                options->setObject(kKeyPacketRequestEvent, pre);
                options->setObject(kKeySessionInfoEvent, infoEvent);

                sp<IMediaSession> session = IMediaSession::Create(trackFormat, options);
                if (session == Nil) {
                    ERROR("create session failed", i);
                    continue;
                }
                track->mDecodeSession   = session;
            }
            
            mReadyMask.set(mTrackID);
            selectedTracks.insert(type, True);
            mTracks.insert(mTrackID++, track);
        }
        
        if (mTracks.empty()) {
            notify(kInfoPlayerError);
        }
        
        selector->fire((UInt32)mReadyMask.value());
        mFileFormats = formats;
    }
    
    struct OnDecoderInfo : public SessionInfoEvent {
        Tiger *thiz;
        const UInt32 id;
        OnDecoderInfo(Tiger *p, const UInt32 n) :
        SessionInfoEvent(p->mDispatch),
        thiz(p), id(n) { }

        virtual void onEvent(const eSessionInfoType& info, const sp<Message>& payload) {
            thiz->onDecoderInfo(id, info, payload);
        }
    };
    
    void onDecoderInfo(const UInt32 id, const eSessionInfoType& info, const sp<Message>& payload) {
        DEBUG("onDecoderInfo [%zu] %.4s", id, (const Char *)&info);
        switch (info) {
            case kSessionInfoError:
                onTrackError(id);
                break;
            case kSessionInfoReady:
                onInitRenderer(id, payload);
                break;
            default:
                break;
        }
    }
    
    void onTrackError(const UInt32& id) {
        ERROR("onTrackError [%zu]", id);
        // on decoder error, release current track
        sp<TrackContext>& track = mTracks[id];
        mTracks.erase(id);
        
        if (mTracks.empty()) notify(kInfoPlayerError);
    }
    
    void onInitRenderer(const UInt32& id, const sp<Message>& format) {
        DEBUG("onInitRenderer [%zu] %s", id, format->string().c_str());
        sp<TrackContext>& track = mTracks[id];
        
        if (track->mType == kCodecTypeAudio) {
            if (format->findInt32(kKeySampleRate) == 0 ||
                    format->findInt32(kKeyChannels) == 0) {
                ERROR("missing mandatory format infomation, playback may be broken");
            }

            if (mHasAudio) {
                INFO("ignore this audio");
                return;
            } else {
                mHasAudio = True;
            }
        } else if (track->mType == kCodecTypeVideo) {
            if (format->findInt32(kKeyWidth) == 0 ||
                    format->findInt32(kKeyHeight) == 0) {
                ERROR("missing mandatory format infomation, playback may be broken");
            }
        }

        sp<FrameRequestEvent> fre = format->findObject(kKeyFrameRequestEvent);

        sp<Message> options = new Message;
        options->setInt32(kKeyMode, mMode);
        if (track->mType == kCodecTypeVideo) {
            if (!mVideoFrameEvent.isNil()) {
                options->setObject(kKeyFrameReadyEvent, mVideoFrameEvent);
            } else if (mOpenGLContext != Nil) {
                options->setPointer(kKeyOpenGLContext, mOpenGLContext);
            }
            options->setInt32(kKeyRequestFormat, kPixelFormat420YpCbCrSemiPlanar);
        } else if (track->mType == kCodecTypeAudio) {
            if (!mAudioFrameEvent.isNil()) {
                options->setObject(kKeyFrameReadyEvent, mAudioFrameEvent);
            }
        }
        options->setObject(kKeyFrameRequestEvent, fre);
        options->setObject(kKeySessionInfoEvent, new OnRendererInfo(this, id));

        if (kCodecTypeAudio == track->mType || mTracks.size() == 1) {
            options->setObject(kKeyClock, new Clock(mClock, kClockRoleMaster));
        } else {
            options->setObject(kKeyClock, new Clock(mClock));
        }

        sp<IMediaSession> session = IMediaSession::Create(format, options);
        if (session == Nil) {
            ERROR("create track %zu failed", id);
            return;
        }
        
        track->mRenderSession = session;
        mEndMask.set(id);
    }

    struct OnRendererInfo : public SessionInfoEvent {
        Tiger *thiz;
        UInt32 id;

        OnRendererInfo(Tiger *p, UInt32 n) : SessionInfoEvent(p->mDispatch),
        thiz(p), id(n) { }

        virtual void onEvent(const eSessionInfoType& info, const sp<Message>& payload) {
            thiz->onRendererInfo(id, info, payload);
        }
    };

    void onRendererInfo(const UInt32& id, const eSessionInfoType& info, const sp<Message>& payload) {
        DEBUG("onRendererInfo [%zu] %.4s", id, (const Char *)&info);
        switch (info) {
            case kSessionInfoReady:
                onRendererReady(id, payload);
                break;
            case kSessionInfoEnd:
                onRendererEnd(id);
                break;
            case kSessionInfoError:
                onTrackError(id);
                break;
            default:
                break;
        }
    }
    
    void onRendererReady(const UInt32& id, const sp<Message>& payload) {
        DEBUG("onRendererReady [%zu] %s", id, payload.isNil() ? "" : payload->string().c_str());
        const sp<TrackContext>& track = mTracks[id];
        mReadyMask.clear(id);
        if (mReadyMask.empty()) {
            INFO("all tracks are ready");
            if (mState == kInit) {  // notify client only once
                CHECK_FALSE(mFileFormats.isNil());
                notify(kInfoPlayerReady, mFileFormats);
            }
            mState = kReady;
        }
    }
    
    void onRendererEnd(const UInt32& id) {
        DEBUG("onRendererEnd [%zu]", id);
        mEndMask.clear(id);
        if (mEndMask.empty()) {
            INFO("all tracks are eos");
            mState = kEnd;
            mClock->pause();
            notify(kInfoPlayerEnd);
        }
    }

    virtual void onRelease() {
        DEBUG("onRelease...");
        mTracks.clear();
        mMediaSource.clear();
    }
    
#define kDeferTime     0.5    // 500ms
    struct DeferStart : public Job {
        Tiger *thiz;

        DeferStart(Tiger *p) : Job(), thiz(p) { }

        virtual void onJob() {
            thiz->onStart();
        }
    };

#define ABS(x) (x < 0 ? -(x) : x)
#define MIN_SEEK_TIME   200000LL        // 200ms
    virtual void onPrepare(const MediaTime& pos) {
        INFO("onPrepare @ %.3f", pos.seconds());
        Int64 delta = ABS((pos.time() - mClock->get()).useconds());
        if (delta < MIN_SEEK_TIME) {
            INFO("ignore seek, request @ %.3f, current %.3f",
                 pos.seconds(), mClock->get().seconds());
            return;
        }
        
        mDispatch->remove(mDeferStart);

        // -> ready by prepare
        Bool paused = mClock->isPaused();
        if (!paused) {
            // pause clock before seek
            mClock->pause();
        }

        // set clock time
        mClock->set(pos.time());

        if (!paused) {
            mDispatch->dispatch(mDeferStart, Time::Seconds(kDeferTime));
        }
        
        HashTable<UInt32, sp<TrackContext> >::const_iterator it = mTracks.cbegin();
        for (; it != mTracks.cend(); ++it) {
            mEndMask.set(it.key());
        }

        return;
    }

    virtual void onStart() {
        INFO("onStart @ %.3f", mClock->get().seconds());
        if (!mClock->isPaused()) {
            INFO("already started");
            return;
        }
        
        if (mClock->isPaused()) mClock->start();
        
        if (mReadyMask.empty()) {
           // NOTHING
        } else {
            DEBUG("defer start...");
            mDispatch->dispatch(mDeferStart, Time::Seconds(kDeferTime));
        }
        notify(kInfoPlayerPlaying);
    }
    
    virtual void onPause() {
        INFO("onPause @ %.3f", mClock->get().seconds());
        if (mClock->isPaused()) {
            INFO("already paused");
            return;
        }
        mDispatch->remove(mDeferStart);
        mClock->pause();
        notify(kInfoPlayerPaused);
        HashTable<UInt32, sp<TrackContext> >::const_iterator it = mTracks.cbegin();
        for (; it != mTracks.cend(); ++it) {
            mReadyMask.set(it.key());
        }
    }
};

sp<IMediaPlayer> CreateTiger() {
    return new Tiger;
}

__END_NAMESPACE_MPX
