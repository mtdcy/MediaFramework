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
#include "MediaDecoder.h"
#include "MediaClock.h"

__USING_NAMESPACE_MPX

struct TrackContext : public SharedObject {
    eCodecFormat        mCodec;
    size_t              mTrackIndex;
    sp<IMediaSession>   mMediaSource;
    sp<IMediaSession>   mDecodeSession;
    sp<IMediaSession>   mRenderSession;
};

struct Tiger : public IMediaPlayer {
    // external static context
    sp<Message>             mMedia;
    sp<PlayerInfoEvent>     mInfoEvent;
    eModeType               mMode;
    sp<MediaFrameEvent>     mAudioFrameEvent;
    sp<MediaFrameEvent>     mVideoFrameEvent;

    // internal static context
    sp<Job>                 mDeferStart;

    // mutable context
    sp<IMediaSession>       mMediaSource;
    size_t                  mTrackID;
    HashTable<size_t, sp<TrackContext> > mTracks;
    bool                    mHasAudio;
    BitSet                  mReadyMask;     // set when not ready

    Tiger(const sp<Message>& media, const sp<Message>& options) :
        IMediaPlayer(new Looper("tiger")),
        // external static context
        mMedia(media->dup()), mInfoEvent(NULL),
        // internal static context
        mDeferStart(new DeferStart(this)),
        // mutable context
        mMediaSource(NULL), mTrackID(0),
        mHasAudio(false) {
            INFO("media => %s", media->string().c_str());
            INFO("options => %s", options->string().c_str());
            
            if (options->contains("PlayerInfoEvent")) {
                mInfoEvent = options->findObject("PlayerInfoEvent");
            }

            mMode = (eModeType)options->findInt32(kKeyMode, kModeTypeNormal);

            if (media->contains("VideoFrameEvent")) {
                mVideoFrameEvent = media->findObject("VideoFrameEvent");
            }

            if (media->contains("AudioFrameEvent")) {
                mAudioFrameEvent = media->findObject("AudioFrameEvent");
            }
        }

    void notify(ePlayerInfoType info) {
        if (mInfoEvent != NULL) {
            mInfoEvent->fire(info);
        }
    }

    virtual void onInit() {
        DEBUG("onInit...");
        sp<Message> options = new Message;
        options->setObject("SessionInfoEvent", new OnSourceInfo(this));

        mMediaSource = IMediaSession::Create(mMedia, options);
    }

    struct OnSourceInfo : public SessionInfoEvent {
        Tiger *thiz;
        OnSourceInfo(Tiger *p) : SessionInfoEvent(Looper::Current()), thiz(p) { }

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
        
        for (size_t i = 0; i < numTracks; ++i) {
            String trackName = String::format("track-%zu", i);
            sp<Message> trackFormat = formats->findObject(trackName);

            DEBUG("track %zu: %s", i, trackFormat->string().c_str());

            eCodecFormat codec = (eCodecFormat)trackFormat->findInt32(kKeyFormat);
            if (codec == kCodecFormatUnknown) {
                ERROR("ignore unknown codec");
                continue;
            }

            eCodecType type = GetCodecType(codec);
            //if (type == kCodecTypeAudio) continue;

            CHECK_TRUE(trackFormat->findObject("PacketRequestEvent"));
            sp<PacketRequestEvent> pre = trackFormat->findObject("PacketRequestEvent");

            sp<Message> options = new Message;
            options->setInt32(kKeyMode, mMode);
            options->setObject("PacketRequestEvent", pre);
            options->setObject("SessionInfoEvent", new OnDecoderInfo(this, mTrackID));

            sp<IMediaSession> session = IMediaSession::Create(trackFormat, options);
            if (session == NULL) {
                ERROR("create session failed", i);
                continue;
            }
            
            sp<TrackContext> track = new TrackContext;
            track->mCodec           = codec;
            track->mTrackIndex      = i;
            track->mMediaSource     = mMediaSource;
            track->mDecodeSession   = session;
            
            mTracks.insert(mTrackID++, track);
            
            mReadyMask.set(i);
        }
        
        if (mTracks.empty()) {
            notify(kInfoPlayerError);
        }
    }
    
    struct OnDecoderInfo : public SessionInfoEvent {
        Tiger *thiz;
        const size_t id;
        OnDecoderInfo(Tiger *p, const size_t n) :
        SessionInfoEvent(Looper::Current()),
        thiz(p), id(n) { }

        virtual void onEvent(const eSessionInfoType& info, const sp<Message>& payload) {
            thiz->onDecoderInfo(id, info, payload);
        }
    };
    
    void onDecoderInfo(const size_t id, const eSessionInfoType& info, const sp<Message>& payload) {
        DEBUG("onDecoderInfo [%zu] %d", id, info);
        switch (info) {
            case kSessionInfoReady:
                onInitRenderer(id, payload);
                break;
            default:
                break;
        }
    }
    
    void onInitRenderer(const size_t id, const sp<Message>& format) {
        DEBUG("onInitRenderer [%zu] %s", id, format->string().c_str());
        sp<TrackContext>& track = mTracks[id];
        
        eCodecType type = GetCodecType(track->mCodec);
        if (type == kCodecTypeAudio) {
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
        } else if (type == kCodecTypeVideo) {
            if (format->findInt32(kKeyWidth) == 0 ||
                    format->findInt32(kKeyHeight) == 0) {
                ERROR("missing mandatory format infomation, playback may be broken");
            }
        }

        sp<FrameRequestEvent> fre = format->findObject("FrameRequestEvent");

        sp<Message> options = new Message;
        options->setInt32(kKeyMode, mMode);
        if (type == kCodecTypeVideo) {
            if (!mVideoFrameEvent.isNIL()) {
                options->setObject("MediaFrameEvent", mVideoFrameEvent);
            }
            options->setInt32(kKeyRequestFormat, kPixelFormat420YpCbCrSemiPlanar);
        } else if (type == kCodecTypeAudio) {
            if (!mAudioFrameEvent.isNIL()) {
                options->setObject("MediaFrameEvent", mAudioFrameEvent);
            }
        }
        options->setObject("FrameRequestEvent", fre);
        options->setObject("SessionInfoEvent", new OnRendererInfo(this, id));

        if (kCodecTypeAudio == type || mTracks.size() == 1) {
            options->setObject("Clock", new Clock(mClock, kClockRoleMaster));
        } else {
            options->setObject("Clock", new Clock(mClock));
        }

        sp<IMediaSession> session = IMediaSession::Create(format, options);
        if (session == NULL) {
            ERROR("create track %zu failed", id);
            return;
        }
        
        track->mRenderSession = session;
    }

    struct OnRendererInfo : public SessionInfoEvent {
        Tiger *thiz;
        size_t id;

        OnRendererInfo(Tiger *p, size_t n) : SessionInfoEvent(Looper::Current()),
        thiz(p), id(n) { }

        virtual void onEvent(const eSessionInfoType& info, const sp<Message>& payload) {
            thiz->onRendererInfo(id, info, payload);
        }
    };

    void onRendererInfo(size_t id, const eSessionInfoType& info, const sp<Message>& payload) {
        DEBUG("onRendererInfo [%zu] %d", id, info);
        switch (info) {
            case kSessionInfoReady:
                INFO("track %zu is ready", id);
                mReadyMask.clear(id);
                if (mReadyMask.empty()) {
                    INFO("all tracks are ready");
                    notify(kInfoPlayerReady);
                }
                break;
            case kSessionInfoError:
                INFO("track %zu report error", id);
                notify(kInfoPlayerError);
                // TODO: stop everything
                break;
            default:
                break;
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
            thiz->onStartPause();
        }
    };

    virtual void onPrepare(const MediaTime& pos) {
        DEBUG("onPrepare @ %.3f", pos.seconds());
        Looper::Current()->remove(mDeferStart);

        // -> ready by prepare
        bool paused = mClock->isPaused();
        if (!paused) {
            // pause clock before seek
            mClock->pause();
        }

        // set clock time
        mClock->set(pos.useconds());

        if (!paused) {
            Looper::Current()->post(mDeferStart, kDeferTimeUs);
        }

        return;
    }

    virtual void onStartPause() {
        DEBUG("onStartPause...");
        mLooper->remove(mDeferStart);
        if (mClock->isPaused()) {
            if (mReadyMask.empty()) {
                mClock->start();
            } else {
                DEBUG("defer start...");
                mLooper->post(mDeferStart, kDeferTimeUs);
            }
            notify(kInfoPlayerPlaying);
        } else {
            // DO pause
            if (!mClock->isPaused()) mClock->pause();
            notify(kInfoPlayerPaused);
        }
    }
};

sp<IMediaPlayer> CreateTiger(const sp<Message>& media, const sp<Message>& options) {
    return new Tiger(media, options);
}
