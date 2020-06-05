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
    Object<Message>         mFormat;
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
    List<sp<MediaPacket> >  mInputQueue;        // input packets queue
    bool                    mInputEOS;          // end of input ?
    bool                    mSignalCodecEOS;    // tell codec eos
    bool                    mOutputEOS;         // end of output ?
    MediaTime               mLastPacketTime;    // test packets in dts order?
    List<sp<FrameReadyEvent> > mFrameRequests;  // requests queue
    // statistics
    size_t                  mPacketsReceived;
    size_t                  mPacketsComsumed;
    size_t                  mFramesDecoded;

    DecodeSession(const sp<Message>& format, const sp<Message>& options) : IMediaSession(new Looper("decoder")),
    // external static context
    mFormat(format), mPacketRequestEvent(NULL), mInfoEvent(NULL),
    // internal static context
    mCodec(NULL), mPacketReadyEvent(NULL),
    // internal mutable context
    mGeneration(0), mInputEOS(false), mSignalCodecEOS(false), mOutputEOS(false),
    mLastPacketTime(kMediaTimeInvalid),
    // statistics
    mPacketsReceived(0), mPacketsComsumed(0), mFramesDecoded(0)
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
        
        Object<Message> options = new Message;
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

        // request packets
        requestPacket(kMediaTimeBegin);
        // -> onPacketReady
        
        sp<Message> formats = mCodec->formats();
        formats->setObject("FrameRequestEvent", new OnFrameRequest(this));
        notify(kSessionInfoReady, formats);
    }

    void onRelease() {
        DEBUG("%s: onRelease...", mName.c_str());

        mCodec.clear();
        mPacketReadyEvent.clear();
        mPacketRequestEvent.clear();
        // can NOT flush looper here
    }


    void requestPacket(const MediaTime& time = kMediaTimeInvalid) {
        DEBUG("%s: requestPacket @ %.3f", mName.c_str(), time.seconds());
        if (mInputEOS) return;

        if (mInputQueue.size() >= 1) {
            DEBUG("%s: input queue is full", mName.c_str());
            return;
        }

        DEBUG("%s: request packet", mName.c_str());

        CHECK_FALSE(mPacketReadyEvent.isNIL());
        mPacketRequestEvent->fire(mPacketReadyEvent, time);

        // -> onPacketReady
    }

    struct OnPacketReady : public PacketReadyEvent {
        DecodeSession * thiz;
        const int mGeneration;

        OnPacketReady(DecodeSession *p, int gen) : PacketReadyEvent(Looper::Current()),
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

        if (pkt == NULL) {
            INFO("%s: eos detected", mName.c_str());
            mInputEOS = true;
        } else {
            DEBUG("%s: packet %.3f|%.3f(s) ready", mName.c_str(),
                  pkt->dts.seconds(), pkt->pts.seconds());

            if (mPacketsReceived == 0) {
                INFO("%s: first packet @ %.3f(s)|%.3f(s)", mName.c_str(),
                     pkt->pts.seconds(), pkt->dts.seconds());
            }

            ++mPacketsReceived;
            // @see MediaFile::read(), packets should in dts order.
            if (pkt->dts < mLastPacketTime) {
                WARN("%s: unorderred packet %.3f(s) < last %.3f(s)", mName.c_str(),
                     pkt->dts.seconds(), mLastPacketTime.seconds());
            }
            mLastPacketTime = pkt->dts;
            mInputQueue.push(pkt);
        }

        // always decode as long as there is packet exists.
        while (mFrameRequests.size() && mInputQueue.size()) {
            onDecode();
        }

        requestPacket(kMediaTimeInvalid);
    }
    
    struct OnFrameRequest : public FrameRequestEvent {
        DecodeSession *thiz;
        
        OnFrameRequest(DecodeSession *session) :
        FrameRequestEvent(Looper::Current()), thiz(session) { }
        
        virtual void onEvent(const sp<FrameReadyEvent>& event, const MediaTime& time) {
            thiz->onRequestFrame(event, time);
        }
    };

    void onRequestFrame(const sp<FrameReadyEvent>& event, const MediaTime& time) {
        DEBUG("%s: onRequestFrame @ %.3f", mName.c_str(), time.seconds());
        CHECK_TRUE(event != NULL);
        
        if (time != kMediaTimeInvalid) {
            INFO("%s: flush on frame request, time %.3f", mName.c_str(), time.seconds());
            
            // clear state
            mInputEOS       = false;
            mOutputEOS      = false;
            mLastPacketTime = kMediaTimeInvalid;
            mInputQueue.clear();
            
            // update generation
            mPacketReadyEvent   = new OnPacketReady(this, ++mGeneration);
            
            // flush codec
            mCodec->flush();
            mFrameRequests.clear();
        }

        if (mOutputEOS) {
            DEBUG("%s: request frame at eos", mName.c_str());
            return;
        }
        
        // request next frame
        mFrameRequests.push(event);
        // case 2: packet is ready
        // decode the first packet
        // or drain the codec if input eos
        if (mInputEOS || mInputQueue.size()) {
            onDecode();
        }
        // case 3: packet is not ready && not eos
        // request packet + pending a decode request
        else {
            requestPacket(time);
        }
    }

    void onDecode() {
        DEBUG("%s: with %zu packets ready", mName.c_str(), mInputQueue.size());
        CHECK_TRUE(mInputQueue.size() || mInputEOS);
        CHECK_FALSE(mOutputEOS);

        // push packets to codec
        if (mInputQueue.size()) {
            sp<MediaPacket> packet = *mInputQueue.begin();
            CHECK_TRUE(packet != NULL);

            DEBUG("%s: decode pkt %.3f(s)", mName.c_str(), packet->dts.seconds());
            MediaError err = mCodec->write(packet);
            if (err == kMediaErrorResourceBusy) {
                DEBUG("%s: codec is full with frames", mName.c_str());
            } else {
                if (err != kMediaNoError) {
                    ERROR("%s: write packet failed.", mName.c_str());
                }
                mInputQueue.pop();
                ++mPacketsComsumed;
            }
        } else if (!mSignalCodecEOS) {
            CHECK_TRUE(mInputEOS);
            // tell codec about eos
            mCodec->write(NULL);
            mSignalCodecEOS = true;
        }

        // drain from codec
        sp<MediaFrame> frame = mCodec->read();
        if (frame == NULL && !mInputEOS) {
            INFO("%s: is initializing...", mName.c_str());
        } else {
            sp<FrameReadyEvent>& event = mFrameRequests.front();

            if (frame != NULL) {
                DEBUG("%s: decoded frame %.3f(s) ready", mName.c_str(), frame->timecode.seconds());
                ++mFramesDecoded;
            } else {
                INFO("%s: codec eos detected", mName.c_str());
                notify(kSessionInfoEnd, NULL);
                mOutputEOS = true;
            }
            event->fire(frame);

            if (mOutputEOS) {
                mFrameRequests.clear(); // clear requests
            } else {
                mFrameRequests.pop();
            }
        }

        // request more packets
        requestPacket(kMediaTimeInvalid);
    }
};

Object<IMediaSession> CreateDecodeSession(const Object<Message>& format, const Object<Message>& options) {
    Object<DecodeSession> decoder = new DecodeSession(format, options);
    return decoder;
}
