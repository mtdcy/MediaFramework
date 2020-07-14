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


// File:    DecodeSession.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20181126     initial version
//

#define LOG_TAG "MediaCodec"
//#define LOG_NDEBUG 0
#include "MediaSession.h"
#include "MediaDevice.h"

#define MIN_PACKETS     (2)

__BEGIN_NAMESPACE_MFWK

// onPacketReady ----- MediaFrame ----> DecodeSession
//      ^                                   |
//      |                                   |
// OnRequestPacket                       MediaDecoder
//      |                                   |
//      |                                   v
// PacketRequestEvent <-- OnPacketReady -- requestPacket
struct MediaCodec : public IMediaSession {
    // external static context
    // options
    eModeType               mMode;
    sp<PacketRequestEvent>  mPacketRequestEvent;        // where we get packets
    sp<SessionInfoEvent>    mInfoEvent;

    // internal static context
    String                  mName;                      // for Log
    sp<MediaDevice>         mCodec;                     // reference to codec
    sp<PacketReadyEvent>    mPacketReadyEvent;          // when packet ready
    eCodecType              mType;

    // internal mutable context
    // TODO: clock for decoder, handle late frames
    enum eState {
        Init,
        Prepare,
        Ready,
        Decoding,
        Paused,
        PrepareInt,     // prepare without notify client
    };
    eState                  mState;
    Atomic<Int>             mGeneration;
    Bool                    mInputEOS;          // end of input ?
    Bool                    mSignalCodecEOS;    // signal codec eos only one time
    MediaTime               mLastPacketTime;    // test packets in dts order?
    struct OnFrameRequest;
    sp<OnFrameRequest>      mFrameRequestEvent;
    List<sp<MediaFrame> >   mInputQueue;
    List<sp<FrameReadyEvent> > mRequestQueue;
    // statistics
    UInt32                  mPacketsComsumed;
    UInt32                  mFramesDecoded;

    MediaCodec(const sp<Looper>& lp) : IMediaSession(lp),
    // external static context
    mPacketRequestEvent(Nil), mInfoEvent(Nil),
    // internal static context
    mCodec(Nil), mPacketReadyEvent(Nil), mType(kCodecTypeAudio),
    // internal mutable context
    mState(Init), mGeneration(0), mInputEOS(False), mSignalCodecEOS(False),
    mLastPacketTime(kMediaTimeInvalid), mFrameRequestEvent(new OnFrameRequest(this)),
    // statistics
    mPacketsComsumed(0), mFramesDecoded(0)
    {
    }

    void notify(eSessionInfoType info, const sp<Message>& payload) {
        if (mInfoEvent != Nil) {
            mInfoEvent->fire(info, payload);
        }
    }

    void onInit(const sp<Message>& formats, const sp<Message>& options) {
        DEBUG("init << %s << %s", formats->string().c_str(), options->string().c_str());
        CHECK_TRUE(options->contains(kKeyPacketRequestEvent));
        mPacketRequestEvent = options->findObject(kKeyPacketRequestEvent);

        if (options->contains(kKeySessionInfoEvent)) {
            mInfoEvent = options->findObject(kKeySessionInfoEvent);
        }
        
        UInt32 codec = formats->findInt32(kKeyFormat);
        mName = String::format("codec-%.4s", (Char *)&codec);

        mMode = (eModeType)options->findInt32(kKeyMode, kModeTypeDefault);
        
        // setup decoder...
        CHECK_TRUE(formats->contains(kKeyType));
        CHECK_TRUE(formats->contains(kKeyFormat));
        mType = (eCodecType)formats->findInt32(kKeyType);
        
        sp<Message> options0 = new Message;
        options0->setInt32(kKeyMode, mMode);

        mCodec = MediaDevice::create(formats, options0);
        if (mCodec.isNil() && mMode == kModeTypeNormal) {
            options0->setInt32(kKeyMode, kModeTypeSoftware);
            mCodec = MediaDevice::create(formats, options0);
        }
        
        if (mCodec.isNil()) {
            ERROR("%s: codec is not supported", mName.c_str());
            notify(kSessionInfoError, Nil);
            return;
        }

        // update generation
        mPacketReadyEvent = new OnPacketReady(this, ++mGeneration);
        requestPacket();
        mState = Prepare;
    }

    virtual void onRelease() {
        INFO("%s: onRelease...", mName.c_str());
        mDispatch->flush();
        mCodec.clear();
        mPacketReadyEvent.clear();
        mPacketRequestEvent.clear();
        mInputQueue.clear();
        mRequestQueue.clear();
        mFrameRequestEvent->invalidate();
        mFrameRequestEvent.clear();
    }

    void requestPacket(const MediaTime& time = kMediaTimeInvalid) {
        DEBUG("%s: requestPacket @ %.3f", mName.c_str(), time.seconds());
        if (mInputEOS) return;
        // NO max packets limit yet
        
        CHECK_FALSE(mPacketReadyEvent.isNil());
        mPacketRequestEvent->fire(mPacketReadyEvent, time);
    }

    struct OnPacketReady : public PacketReadyEvent {
        MediaCodec * thiz;
        const Int mGeneration;

        OnPacketReady(MediaCodec *p, Int gen) : PacketReadyEvent(p->mDispatch),
        thiz(p), mGeneration(gen) { }

        virtual void onEvent(const sp<MediaFrame>& packet) {
            thiz->onPacketReady(packet, mGeneration);
        }
    };

    void onPacketReady(const sp<MediaFrame>& pkt, Int generation) {
        if (mGeneration.load() != generation) {
            INFO("%s: ignore outdated packets", mName.c_str());
            return;
        }
        
        if (ABE_UNLIKELY(pkt.isNil())) {
            INFO("%s: input eos...", mName.c_str());
            mInputEOS = True;
            mSignalCodecEOS = False;
        } else {
            DEBUG("one packet ready %s", pkt->string().c_str());
            mInputQueue.push(pkt);
            
            if (mState == Prepare || mState == PrepareInt) {
                if (mInputQueue.size() >= MIN_PACKETS) {
                    INFO("%s: input is ready, queue length %zu",
                         mName.c_str(), mInputQueue.size());
                    if (mState == Prepare) {
                        mState = Ready;
                        
                        sp<Message> codecFormat = mCodec->formats();
                        codecFormat->setObject(kKeyFrameRequestEvent, mFrameRequestEvent);
                        notify(kSessionInfoReady, codecFormat);
                    }
                    
                    mState = Decoding;
                    // -> decode()
                } else {
                    // NOT READY, request more packets until we are ready
                    requestPacket();
                    return;
                }
            }
        }
        
        decode();
    }
    
    void decode() {
        CHECK_TRUE(mState == Decoding);
        
        if (mInputQueue.empty() && !mInputEOS) {
            // this happens when render request frame too frequently
            DEBUG("%s: underrun, request queue %zu", mName.c_str(), mRequestQueue.size());
            // wait until new packet is ready
            // NO NEED to request packet here
            return;
        }
        
        if (mRequestQueue.empty()) return;
        
        DEBUG("%s: input queue %zu, request queue %zu",
             mName.c_str(), mInputQueue.size(), mRequestQueue.size());
        
        // enter draining mode ?
        if (ABE_UNLIKELY(mInputQueue.empty() && mInputEOS)) {
            if (!mSignalCodecEOS) {
                mCodec->push(Nil);
                mSignalCodecEOS = True;
            }
            sp<MediaFrame> frame = drain();
            reply(frame);
            return;
        }
        
        sp<MediaFrame> packet = mInputQueue.front();
        MediaError st = mCodec->push(packet);
        // try again
        if (kMediaErrorResourceBusy == st) {
            DEBUG("%s: codec report busy", mName.c_str());
            sp<MediaFrame> frame = drain();
            CHECK_FALSE(frame.isNil());
            reply(frame);
            return;
        }
        
        if (kMediaNoError != st) {
            ERROR("%s: decoder write() return error %#x", mName.c_str(), st);
            notify(kSessionInfoError, Nil);
            return;
        }
        
        mPacketsComsumed++;
        mInputQueue.pop();
        requestPacket();
        
        sp<MediaFrame> frame = drain();
        
        // when codec is initializing, frame will be Nil
        if (!frame.isNil()) reply(frame);
    }
    
    FORCE_INLINE void reply(const sp<MediaFrame>& frame) {
        CHECK_FALSE(mRequestQueue.empty());
        sp<FrameReadyEvent> request = mRequestQueue.front();
        mRequestQueue.pop();
        DEBUG("send %s", frame->string().c_str());
        request->fire(frame);
        if (!frame.isNil()) ++mFramesDecoded;
    }
    
    FORCE_INLINE sp<MediaFrame> drain() {
        sp<MediaFrame> frame = mCodec->pull();
        if (frame.isNil()) {
            if (mInputEOS) {
                INFO("%s: codec eos...", mName.c_str());
                notify(kSessionInfoEnd, Nil);
            } else {
                INFO("%s: codec is initializing...", mName.c_str());
            }
            return Nil;
        }
        
        if (mType == kCodecTypeAudio) {
            // fix duration
            if (frame->duration == kMediaTimeInvalid) {
                frame->duration = MediaTime(frame->audio.samples, frame->audio.freq);
            }
        }
        return frame;
    }
    
    struct OnFrameRequest : public FrameRequestEvent {
        MediaCodec *thiz;
        
        OnFrameRequest(MediaCodec *p) :
        FrameRequestEvent(p->mDispatch), thiz(p) { }
        
        virtual void onEvent(const sp<FrameReadyEvent>& event, const MediaTime& time) {
            if (thiz == Nil) {
                INFO("request frame after invalidate");
                return;
            }
            thiz->onRequestFrame(event, time);
        }
        
        void invalidate() {
            thiz = Nil;
        }
    };

    void onRequestFrame(sp<FrameReadyEvent> event, const MediaTime& time) {
        DEBUG("%s: onRequestFrame @ %.3f", mName.c_str(), time.seconds());
        CHECK_TRUE(event != Nil);
        
        if (ABE_UNLIKELY(time != kMediaTimeInvalid)) {
            INFO("%s: frame request @ %.3f(s) @ state %d",
                 mName.c_str(), time.seconds(), mState);
            
            // clear state
            mState          = PrepareInt;
            mInputEOS       = False;
            mLastPacketTime = kMediaTimeInvalid;
            mInputQueue.clear();
            mRequestQueue.clear();
            
            // update generation
            mPacketReadyEvent = new OnPacketReady(this, ++mGeneration);
            
            // flush codec
            mCodec->reset();
            
            // request @ time
            mRequestQueue.push(event);
            requestPacket(time);
        } else {
            mRequestQueue.push(event);
            decode();
        }
    }
};

sp<IMediaSession> CreateMediaCodec(const sp<Looper>& lp) {
    sp<MediaCodec> codec = new MediaCodec(lp);
    return codec;
}

__END_NAMESPACE_MFWK
