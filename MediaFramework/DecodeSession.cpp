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

#define LOG_TAG "DecodeSession"
//#define LOG_NDEBUG 0
#include "MediaSession.h"
#include "MediaDecoder.h"

__USING_NAMESPACE_MPX

// onPacketReady ----- MediaPacket ----> DecodeSession
//      ^                                   |
//      |                                   |
// OnRequestPacket                       MediaDecoder
//      |                                   |
//      |                                   v
// PacketRequestEvent <-- OnPacketReady -- requestPacket
struct DecodeSession : public IMediaSession {
    // external static context
    sp<Message>             mFormat;
    // options
    eModeType               mMode;
    sp<PacketRequestEvent>  mPacketRequestEvent;        // where we get packets
    sp<SessionInfoEvent>    mInfoEvent;

    // internal static context
    String                  mName;                      // for Log
    sp<MediaDecoder>        mCodec;                     // reference to codec
    sp<PacketReadyEvent>    mPacketReadyEvent;          // when packet ready

    // internal mutable context
    // TODO: clock for decoder, handle late frames
    Atomic<int>             mGeneration;
    bool                    mInputEOS;          // end of input ?
    MediaTime               mLastPacketTime;    // test packets in dts order?
    List<sp<FrameReadyEvent> > mPendingRequests;
    // statistics
    size_t                  mPacketsComsumed;
    size_t                  mFramesDecoded;

    DecodeSession(const sp<Message>& format, const sp<Message>& options) : IMediaSession(new Looper("decoder")),
    // external static context
    mFormat(format), mPacketRequestEvent(NULL), mInfoEvent(NULL),
    // internal static context
    mCodec(NULL), mPacketReadyEvent(NULL),
    // internal mutable context
    mGeneration(0), mInputEOS(false),
    mLastPacketTime(kMediaTimeInvalid),
    // statistics
    mPacketsComsumed(0), mFramesDecoded(0)
    {
        DEBUG("init << %s << %s", format->string().c_str(), options->string().c_str());
        CHECK_TRUE(options->contains("PacketRequestEvent"));
        mPacketRequestEvent = options->findObject("PacketRequestEvent");

        if (options->contains("SessionInfoEvent")) {
            mInfoEvent = options->findObject("SessionInfoEvent");
        }
        
        uint32_t codec = mFormat->findInt32(kKeyFormat);
        mName = String::format("codec-%4s", (char *)&codec);

        mMode = (eModeType)options->findInt32(kKeyMode, kModeTypeDefault);
    }

    void notify(eSessionInfoType info, const sp<Message>& payload) {
        if (mInfoEvent != NULL) {
            mInfoEvent->fire(info, payload);
        }
    }

    void onInit() {
        DEBUG("%s: onInit...", mName.c_str());
        // setup decoder...
        CHECK_TRUE(mFormat->contains(kKeyCodecType));
        CHECK_TRUE(mFormat->contains(kKeyFormat));
        eCodecType type = (eCodecType)mFormat->findInt32(kKeyCodecType);
        
        sp<Message> options = new Message;
        options->setInt32(kKeyMode, mMode);

        mCodec = MediaDecoder::Create(mFormat, options);
        if (mCodec.isNIL() && mMode == kModeTypeNormal) {
            options->setInt32(kKeyMode, kModeTypeSoftware);
            mCodec = MediaDecoder::Create(mFormat, options);
        }
        
        if (mCodec.isNIL()) {
            ERROR("%s: codec is not supported", mName.c_str());
            notify(kSessionInfoError, NULL);
            return;
        }

        // update generation
        mPacketReadyEvent = new OnPacketReady(this, ++mGeneration);
        
        sp<Message> formats = mCodec->formats();
        formats->setObject("FrameRequestEvent", new OnFrameRequest(this));
        notify(kSessionInfoReady, formats);
    }

    virtual void onRelease() {
        INFO("%s: onRelease...", mName.c_str());
        mDispatch->flush();
        mCodec.clear();
        mPacketReadyEvent.clear();
        mPacketRequestEvent.clear();
        mPendingRequests.clear();
    }

    void requestPacket(const MediaTime& time = kMediaTimeInvalid) {
        DEBUG("%s: requestPacket @ %.3f", mName.c_str(), time.seconds());
        if (mInputEOS) return;
        if (mPendingRequests.empty()) return;
        
        CHECK_FALSE(mPacketReadyEvent.isNIL());
        mPacketRequestEvent->fire(mPacketReadyEvent, time);
    }

    struct OnPacketReady : public PacketReadyEvent {
        DecodeSession * thiz;
        const int mGeneration;

        OnPacketReady(DecodeSession *p, int gen) : PacketReadyEvent(p->mDispatch),
        thiz(p), mGeneration(gen) { }

        virtual void onEvent(const sp<MediaPacket>& packet) {
            thiz->onPacketReady(packet, mGeneration);
        }
    };

    void onPacketReady(const sp<MediaPacket>& pkt, int generation) {
        DEBUG("%s: onPacketReady %zu bytes @ %.3f", mName.c_str(), pkt->size, pkt->dts.seconds());
        if (mGeneration.load() != generation) {
            INFO("%s: ignore outdated packets", mName.c_str());
            return;
        }

        if (!pkt.isNIL()) {
            DEBUG("%s: packet %.3f|%.3f(s) ready", mName.c_str(),
                  pkt->dts.seconds(), pkt->pts.seconds());

            if (mPacketsComsumed == 0) {
                INFO("%s: first packet @ %.3f(s)|%.3f(s)", mName.c_str(),
                     pkt->pts.seconds(), pkt->dts.seconds());
            }

            // @see MediaFile::read(), packets should in dts order.
            if (pkt->dts < mLastPacketTime) {
                WARN("%s: unorderred packet %.3f(s) < last %.3f(s)", mName.c_str(),
                     pkt->dts.seconds(), mLastPacketTime.seconds());
            }
            mLastPacketTime = pkt->dts;
        }
        
        if (mPendingRequests.empty()) {
            FATAL("%s: request packet for no reason", mName.c_str());
        }
        
        decode(pkt);
        
        // contine request packet if pending request exists
        if (mPendingRequests.size()) {
            DEBUG("%s: we are on underrun state, request more packet", mName.c_str());
            requestPacket();
        }
    }
    
    void decode(const sp<MediaPacket>& packet) {
        CHECK_TRUE(mPendingRequests.size());
        // only flush once on input eos
        if (!mInputEOS) {
            MediaError st = mCodec->write(packet);
            // try again
            if (kMediaErrorResourceBusy == st) {
                sp<MediaFrame> frame = drain();
                CHECK_FALSE(frame.isNIL());
                sp<FrameReadyEvent> request = mPendingRequests.front();
                mPendingRequests.pop();
                request->fire(frame);
            }
            
            if (kMediaNoError != st) {
                ERROR("%s: decoder write() return error", mName.c_str());
                notify(kSessionInfoError, NULL);
                return;
            }
        }
        
        if (packet.isNIL()) {
            INFO("%s: eos detected", mName.c_str());
            mInputEOS = true;
        } else {
            mPacketsComsumed++;
        }
        
        sp<MediaFrame> frame = drain();
        if (frame.isNIL() && !mInputEOS) {
            // codec is initializing
        } else {
            sp<FrameReadyEvent> request = mPendingRequests.front();
            mPendingRequests.pop();
            request->fire(frame);
        }
    }
    
    FORCE_INLINE sp<MediaFrame> drain() {
        sp<MediaFrame> frame = mCodec->read();
        if (frame.isNIL()) {
            if (mInputEOS) {
                INFO("%s: codec eos...", mName.c_str());
                notify(kSessionInfoEnd, NULL);
            } else {
                INFO("%s: codec is initializing...", mName.c_str());
            }
        }
        return frame;
    }
    
    struct OnFrameRequest : public FrameRequestEvent {
        DecodeSession *thiz;
        
        OnFrameRequest(DecodeSession *p) :
        FrameRequestEvent(p->mDispatch), thiz(p) { }
        
        virtual void onEvent(const sp<FrameReadyEvent>& event, const MediaTime& time) {
            thiz->onRequestFrame(event, time);
        }
    };

    void onRequestFrame(sp<FrameReadyEvent> event, const MediaTime& time) {
        DEBUG("%s: onRequestFrame @ %.3f", mName.c_str(), time.seconds());
        CHECK_TRUE(event != NULL);
        
        if (time != kMediaTimeInvalid) {
            INFO("%s: flush on frame request, time %.3f", mName.c_str(), time.seconds());
            
            // clear state
            mInputEOS       = false;
            mLastPacketTime = kMediaTimeInvalid;
            mPendingRequests.clear();
            
            // update generation
            mPacketReadyEvent = new OnPacketReady(this, ++mGeneration);
            
            // flush codec
            mCodec->flush();
        }
        
        if (mInputEOS) {
            // drain decoder until eos
            event->fire(drain());
        } else {
            mPendingRequests.push(event);
            // start request packets
            // this should be the common case,
            // decoder should be fast than renderer, or underrun hapens
            if (mPendingRequests.size() == 1) {
                DEBUG("%s: request packet on frame request", mName.c_str());
                requestPacket();
            }
        }
    }
};

sp<IMediaSession> CreateDecodeSession(const sp<Message>& format, const sp<Message>& options) {
    sp<DecodeSession> decoder = new DecodeSession(format, options);
    return decoder;
}
