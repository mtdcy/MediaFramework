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

struct TrackContext : public SharedObject {
    eCodecType          mType;
    size_t              mTrackIndex;
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
    size_t                  mTrackID;
    HashTable<size_t, sp<TrackContext> > mTracks;
    bool                    mHasAudio;
    Bits<uint32_t>          mReadyMask;     // set when not ready
    Bits<uint32_t>          mEndMask;       // set when not eos
    enum eState { kInit, kReady, kEnd };
    eState                  mState;

    Tiger() : IMediaPlayer(new Looper("tiger")),
        // external static context
        mInfoEvent(NULL), mOpenGLContext(NULL),
        // internal static context
        mDeferStart(new DeferStart(this)),
        // mutable context
        mMediaSource(NULL), mTrackID(0),
        mHasAudio(false), mState(kInit) {
            
        }

    void notify(ePlayerInfoType info, const sp<Message>& payload = NULL) {
        if (mInfoEvent != NULL) {
            mInfoEvent->fire(info, payload);
        }
    }

    virtual void onInit(const sp<Message>& media, const sp<Message>& options) {
        DEBUG("onInit...");
        INFO("media => %s", media->string().c_str());
        INFO("options => %s", options->string().c_str());
        
        if (!options.isNIL()) {
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
        size_t numTracks = formats->findInt32(kKeyCount, 1);
        
        CHECK_TRUE(formats->contains(kKeyTrackSelectEvent));
        sp<TrackSelectEvent> selector = formats->findObject(kKeyTrackSelectEvent);
        
        // there is no need to use HashTable, but it help keep code clean
        HashTable<uint32_t, bool> selectedTracks;
        for (size_t i = 0; i < numTracks; ++i) {
            sp<Message> trackFormat = formats->findObject(kKeyTrack + i);

            DEBUG("track %zu: %s", i, trackFormat->string().c_str());

            CHECK_TRUE(trackFormat->contains(kKeyType));
            eCodecType type = (eCodecType)trackFormat->findInt32(kKeyType);
            int32_t codec = trackFormat->findInt32(kKeyFormat);
            
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
            if (codec == kAudioCodecPCM) {
#if 0 // FIXME
                track->mPacketRequestEvent  = pre;
                int32_t bits = trackFormat->findInt32(kKeySampleBits, 16);
                switch (bits) {
                    case 8:     track->mAudioFormat.format = kSampleFormatU8;   break;
                    case 16:    track->mAudioFormat.format = kSampleFormatS16;  break;
                    case 32:    track->mAudioFormat.format = kSampleFormatS32;  break;
                        // TODO: handle flt & dbl
                }
                track->mAudioFormat.channels = trackFormat->findInt32(kKeyChannels);
                track->mAudioFormat.freq = trackFormat->findInt32(kKeySampleRate);
                
                sp<Message> sampleFormat = new Message;
                sampleFormat->setInt32(kKeyFormat, track->mAudioFormat.format);
                sampleFormat->setInt32(kKeyChannels, track->mAudioFormat.channels);
                sampleFormat->setInt32(kKeySampleRate, track->mAudioFormat.freq);
                sampleFormat->setObject(kKeyFrameRequestEvent, new FrameRequestTunnel(track));
                
                infoEvent->fire(kSessionInfoReady, sampleFormat);
#endif
                // > onInitRenderer
            } else {
                sp<Message> options = new Message;
                options->setInt32(kKeyMode, mMode);
                options->setObject(kKeyPacketRequestEvent, pre);
                options->setObject(kKeySessionInfoEvent, infoEvent);

                sp<IMediaSession> session = IMediaSession::Create(trackFormat, options);
                if (session == NULL) {
                    ERROR("create session failed", i);
                    continue;
                }
                track->mDecodeSession   = session;
            }
            
            mReadyMask.set(mTrackID);
            selectedTracks.insert(type, true);
            mTracks.insert(mTrackID++, track);
        }
        
        if (mTracks.empty()) {
            notify(kInfoPlayerError);
        }
        
        selector->fire((size_t)mReadyMask.value());
        mFileFormats = formats;
    }
    
    struct OnDecoderInfo : public SessionInfoEvent {
        Tiger *thiz;
        const size_t id;
        OnDecoderInfo(Tiger *p, const size_t n) :
        SessionInfoEvent(p->mDispatch),
        thiz(p), id(n) { }

        virtual void onEvent(const eSessionInfoType& info, const sp<Message>& payload) {
            thiz->onDecoderInfo(id, info, payload);
        }
    };
    
    void onDecoderInfo(const size_t id, const eSessionInfoType& info, const sp<Message>& payload) {
        DEBUG("onDecoderInfo [%zu] %.4s", id, (const char *)&info);
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
    
    void onTrackError(const size_t& id) {
        ERROR("onTrackError [%zu]", id);
        // on decoder error, release current track
        sp<TrackContext>& track = mTracks[id];
        mTracks.erase(id);
        
        if (mTracks.empty()) notify(kInfoPlayerError);
    }
    
    void onInitRenderer(const size_t& id, const sp<Message>& format) {
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
                mHasAudio = true;
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
            if (!mVideoFrameEvent.isNIL()) {
                options->setObject(kKeyFrameReadyEvent, mVideoFrameEvent);
            } else if (mOpenGLContext != NULL) {
                options->setPointer(kKeyOpenGLContext, mOpenGLContext);
            }
            options->setInt32(kKeyRequestFormat, kPixelFormat420YpCbCrSemiPlanar);
        } else if (track->mType == kCodecTypeAudio) {
            if (!mAudioFrameEvent.isNIL()) {
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
        if (session == NULL) {
            ERROR("create track %zu failed", id);
            return;
        }
        
        track->mRenderSession = session;
        mEndMask.set(id);
    }

    struct OnRendererInfo : public SessionInfoEvent {
        Tiger *thiz;
        size_t id;

        OnRendererInfo(Tiger *p, size_t n) : SessionInfoEvent(p->mDispatch),
        thiz(p), id(n) { }

        virtual void onEvent(const eSessionInfoType& info, const sp<Message>& payload) {
            thiz->onRendererInfo(id, info, payload);
        }
    };

    void onRendererInfo(const size_t& id, const eSessionInfoType& info, const sp<Message>& payload) {
        DEBUG("onRendererInfo [%zu] %.4s", id, (const char *)&info);
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
    
    void onRendererReady(const size_t& id, const sp<Message>& payload) {
        DEBUG("onRendererReady [%zu] %s", id, payload.isNIL() ? "" : payload->string().c_str());
        const sp<TrackContext>& track = mTracks[id];
        mReadyMask.clear(id);
        if (mReadyMask.empty()) {
            INFO("all tracks are ready");
            if (mState == kInit) {  // notify client only once
                CHECK_FALSE(mFileFormats.isNIL());
                notify(kInfoPlayerReady, mFileFormats);
            }
            mState = kReady;
        }
    }
    
    void onRendererEnd(const size_t& id) {
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
    
#define kDeferTimeUs     500000LL    // 500ms
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
        int64_t delta = ABS(pos.useconds() - mClock->get());
        if (delta < MIN_SEEK_TIME) {
            INFO("ignore seek, request @ %.3f, current %.3f",
                 pos.seconds(), mClock->get() / 1E6);
            return;
        }
        
        mDispatch->remove(mDeferStart);

        // -> ready by prepare
        bool paused = mClock->isPaused();
        if (!paused) {
            // pause clock before seek
            mClock->pause();
        }

        // set clock time
        mClock->set(pos.useconds());

        if (!paused) {
            mDispatch->dispatch(mDeferStart, kDeferTimeUs);
        }
        
        HashTable<size_t, sp<TrackContext> >::const_iterator it = mTracks.cbegin();
        for (; it != mTracks.cend(); ++it) {
            mEndMask.set(it.key());
        }

        return;
    }

    virtual void onStart() {
        INFO("onStart @ %.3f", mClock->get() / 1E6);
        if (!mClock->isPaused()) {
            INFO("already started");
            return;
        }
        
        if (mClock->isPaused()) mClock->start();
        
        if (mReadyMask.empty()) {
           // NOTHING
        } else {
            DEBUG("defer start...");
            mDispatch->dispatch(mDeferStart, kDeferTimeUs);
        }
        notify(kInfoPlayerPlaying);
    }
    
    virtual void onPause() {
        INFO("onPause @ %.3f", mClock->get() / 1E6);
        if (mClock->isPaused()) {
            INFO("already paused");
            return;
        }
        mDispatch->remove(mDeferStart);
        mClock->pause();
        notify(kInfoPlayerPaused);
        HashTable<size_t, sp<TrackContext> >::const_iterator it = mTracks.cbegin();
        for (; it != mTracks.cend(); ++it) {
            mReadyMask.set(it.key());
        }
    }
};

sp<IMediaPlayer> CreateTiger() {
    return new Tiger;
}

__END_NAMESPACE_MPX
