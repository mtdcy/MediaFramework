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

struct MediaFrameTunnel : public MediaFrame {
    sp<MediaPacket>     mPacket;
    
    MediaFrameTunnel(const sp<TrackContext>& track, const sp<MediaPacket>& packet) {
        for (size_t i = 0; i < MEDIA_FRAME_NB_PLANES; ++i) {
            planes[i].data = NULL;
            planes[i].size = 0;
        }
        planes[0].data  = packet->data;
        planes[0].size  = packet->size;
        timecode        = packet->dts;
        duration        = packet->duration;
        
        if (track->mType == kCodecTypeAudio) {
            a           = track->mAudioFormat;
        } else if (track->mType == kCodecTypeVideo) {
            v           = track->mVideoFormat;
        }
        mPacket         = packet;
    }
};

struct PacketReadyTunnel : public PacketReadyEvent {
    sp<TrackContext>    mTrack;
    sp<FrameReadyEvent> mFrameReadyEvent;
    
    PacketReadyTunnel(const sp<TrackContext>& track, const sp<FrameReadyEvent>& event) :
    PacketReadyEvent(Looper::Current()),
    mTrack(track),
    mFrameReadyEvent(event) { }
    
    virtual void onEvent(const sp<MediaPacket>& packet) {
        // EOS
        if (packet.isNIL()) {
            mFrameReadyEvent->fire(NULL);
            return;
        }
        
        sp<MediaFrameTunnel> frame = new MediaFrameTunnel(mTrack, packet);
        mFrameReadyEvent->fire(frame);
    }
};

struct FrameRequestTunnel : public FrameRequestEvent {
    sp<TrackContext>    mTrack;
    
    FrameRequestTunnel(const sp<TrackContext>& track) :
    FrameRequestEvent(Looper::Current()),
    mTrack(track) { }
    
    virtual void onEvent(const sp<FrameReadyEvent>& request, const MediaTime& time) {
        sp<PacketReadyEvent> event = new PacketReadyTunnel(mTrack, request);
        mTrack->mPacketRequestEvent->fire(event, time);
    }
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
    sp<Message>             mFileFormats;
    size_t                  mTrackID;
    HashTable<size_t, sp<TrackContext> > mTracks;
    bool                    mHasAudio;
    BitSet                  mReadyMask;     // set when not ready
    BitSet                  mEndMask;       // set when not eos
    enum eState {
        kInit,
        kReady,
        kEnd
    };
    eState                  mState;

    Tiger(const sp<Message>& media, const sp<Message>& options) :
        IMediaPlayer(new Looper("tiger")),
        // external static context
        mMedia(media->dup()), mInfoEvent(NULL),
        // internal static context
        mDeferStart(new DeferStart(this)),
        // mutable context
        mMediaSource(NULL), mTrackID(0),
        mHasAudio(false), mState(kInit) {
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

    void notify(ePlayerInfoType info, const sp<Message>& payload = NULL) {
        if (mInfoEvent != NULL) {
            mInfoEvent->fire(info, payload);
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
        
        for (size_t i = 0; i < numTracks; ++i) {
            String trackName = String::format("track-%zu", i);
            sp<Message> trackFormat = formats->findObject(trackName);

            DEBUG("track %zu: %s", i, trackFormat->string().c_str());

            CHECK_TRUE(trackFormat->contains(kKeyCodecType));
            eCodecType type = (eCodecType)trackFormat->findInt32(kKeyCodecType);
            int32_t codec = trackFormat->findInt32(kKeyFormat);
            
            CHECK_TRUE(trackFormat->findObject("PacketRequestEvent"));
            sp<PacketRequestEvent> pre = trackFormat->findObject("PacketRequestEvent");
            
            sp<TrackContext> track  = new TrackContext;
            track->mTrackIndex      = i;
            track->mType            = type;
            track->mMediaSource     = mMediaSource;

            sp<SessionInfoEvent> infoEvent = new OnDecoderInfo(this, mTrackID);
            if (codec == kAudioCodecPCM) {
                track->mPacketRequestEvent  = pre;
                int32_t bits = trackFormat->findInt32(kKeyBits, 16);
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
                sampleFormat->setObject("FrameRequestEvent", new FrameRequestTunnel(track));
                
                infoEvent->fire(kSessionInfoReady, sampleFormat);
                // > onInitRenderer
            } else {
                sp<Message> options = new Message;
                options->setInt32(kKeyMode, mMode);
                options->setObject("PacketRequestEvent", pre);
                options->setObject("SessionInfoEvent", infoEvent);

                sp<IMediaSession> session = IMediaSession::Create(trackFormat, options);
                if (session == NULL) {
                    ERROR("create session failed", i);
                    continue;
                }
                track->mDecodeSession   = session;
            }
            
            mReadyMask.set(mTrackID);
            mTracks.insert(mTrackID++, track);
        }
        
        if (mTracks.empty()) {
            notify(kInfoPlayerError);
        }
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
        DEBUG("onDecoderInfo [%zu] %d", id, info);
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

        sp<FrameRequestEvent> fre = format->findObject("FrameRequestEvent");

        sp<Message> options = new Message;
        options->setInt32(kKeyMode, mMode);
        if (track->mType == kCodecTypeVideo) {
            if (!mVideoFrameEvent.isNIL()) {
                options->setObject("MediaFrameEvent", mVideoFrameEvent);
            }
            options->setInt32(kKeyRequestFormat, kPixelFormat420YpCbCrSemiPlanar);
        } else if (track->mType == kCodecTypeAudio) {
            if (!mAudioFrameEvent.isNIL()) {
                options->setObject("MediaFrameEvent", mAudioFrameEvent);
            }
        }
        options->setObject("FrameRequestEvent", fre);
        options->setObject("SessionInfoEvent", new OnRendererInfo(this, id));

        if (kCodecTypeVideo == track->mType || mTracks.size() == 1) {
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
        DEBUG("onRendererInfo [%zu] %d", id, info);
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
        if (mReadyMask.empty()) {
            mClock->start();
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

sp<IMediaPlayer> CreateTiger(const sp<Message>& media, const sp<Message>& options) {
    return new Tiger(media, options);
}
