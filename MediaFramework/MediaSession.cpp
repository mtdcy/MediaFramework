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


// File:    Track.h
// Author:  mtdcy.chen
// Changes:
//          1. 20181126     initial version
//

#define LOG_TAG "MediaSession"
//#define LOG_NDEBUG 0
#include "MediaSession.h"
#include "MediaDecoder.h"
#include "MediaOut.h"
#include "ColorConvertor.h"
#include "MediaClock.h"
#include "MediaPlayer.h"

// TODO: calc count based on duration
// max count: 1s
// min count: 500ms
#define MIN_COUNT (4)
#define MAX_COUNT (16)
#define REFRESH_RATE (10000LL)    // 10ms

// media session <= control session
//  packet ready event
//          v
//  decode session  <= looper
//          v
//  frame request event
//          v
//  frame ready event
//          v
//  render session  <= control session
//                  <= clock event
//                  <= looper(external)

__USING_NAMESPACE_MPX

struct MediaFrameTunnel : public MediaFrame {
    sp<MediaPacket>     mPacket;
    MediaFrameTunnel(const sp<MediaPacket>& packet) : MediaFrame(), mPacket(packet) {
        pts                 = packet->pts;
        duration            = kTimeInvalid;
        planes[0].data      = packet->data;
        planes[0].size      = packet->size;
        // FIXME
    }
};

struct OnPacketReadyTunnel : public PacketReadyEvent {
    sp<FrameReadyEvent>     mFrameReadyEvent;
    OnPacketReadyTunnel(const sp<FrameReadyEvent>& event) :
    PacketReadyEvent(), mFrameReadyEvent(event) { }
    virtual void onEvent(const sp<MediaPacket>& packet) {
        mFrameReadyEvent->fire(new MediaFrameTunnel(packet));
    }
};

struct OnFrameRequestTunnel : public FrameRequestEvent {
    sp<PacketRequestEvent>      mPacketRequestEvent;
    OnFrameRequestTunnel(const sp<PacketRequestEvent>& event) :
    FrameRequestEvent(), mPacketRequestEvent(event) { }
    
    virtual void onEvent(const FrameRequest& request) {
        PacketRequest packetRequest;
        packetRequest.event     = new OnPacketReadyTunnel(request.event);
        packetRequest.mode      = kModeReadNext;
        if (request.ts != kTimeInvalid) {
            packetRequest.ts    = request.ts;
            packetRequest.mode  = kModeReadClosestSync;
        }
        // request packet
        mPacketRequestEvent->fire(packetRequest);
    }
};

struct Decoder : public SharedObject {
    // external static context
    eCodecFormat            mID;
    sp<PacketRequestEvent>  mPacketRequestEvent;        // where we get packets
    // internal static context
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
    List<FrameRequest>      mFrameRequests;     // requests queue
    // statistics
    size_t                  mPacketsReceived;
    size_t                  mPacketsComsumed;
    size_t                  mFramesDecoded;

    bool valid() const { return mCodec != NULL; }

    Decoder(const sp<Message>& format, const sp<Message>& options) :
        // external static context
        mID(kCodecFormatUnknown),
        mPacketRequestEvent(NULL),
        // internal static context
        mCodec(NULL), mPacketReadyEvent(NULL),
        // internal mutable context
        mGeneration(0), mInputEOS(false), mSignalCodecEOS(false), mLastPacketTime(kTimeInvalid),
        // statistics
        mPacketsReceived(0), mPacketsComsumed(0), mFramesDecoded(0)
    {
        CHECK_TRUE(options->contains("PacketRequestEvent"));
        mPacketRequestEvent = options->findObject("PacketRequestEvent");

        // setup decoder...
        CHECK_TRUE(format->contains(kKeyFormat));
        mID = (eCodecFormat)format->findInt32(kKeyFormat);
        eModeType mode = (eModeType)options->findInt32(kKeyMode, kModeTypeDefault);

        eCodecType type = GetCodecType(mID);

        // always set mCodec to null, if create or initial faileds
        mCodec = MediaDecoder::Create(mID, mode);
        if (mCodec->init(format, options) != kMediaNoError) {
            ERROR("track %zu: create codec failed", mID);
            mCodec.clear();

#if 1
            if (mode != kModeTypeSoftware) {
                sp<Message> soft = options->dup();
                soft->setInt32(kKeyMode, kModeTypeSoftware);
                mCodec = MediaDecoder::Create(mID, kModeTypeSoftware);

                if (mCodec->init(format, soft) != kMediaNoError) {
                    ERROR("track %zu: create software codec failed", mID);
                    mCodec.clear();
                }
            }
#endif
            return;
        }
    }

    virtual ~Decoder() {
    }

    void requestPacket(const MediaTime& ts = kTimeInvalid) {
        if (mInputEOS) return;

        if (mInputQueue.size() >= MAX_COUNT) {
            DEBUG("codec %zu: input queue is full", mID);
            return;
        }

        DEBUG("codec %zu: request packet", mID);
        PacketRequest payload;
        if (ts != kTimeInvalid) {
            INFO("codec %zu: request frame at %.3f(s)", mID, ts.seconds());
            payload.mode = kModeReadClosestSync;
            payload.ts = ts;
        } else {
            payload.mode = kModeReadNext;
            payload.ts = ts;
        }

        payload.event = mPacketReadyEvent;
        mPacketRequestEvent->fire(payload);

        // -> onPacketReady
    }

    struct OnPacketReady : public PacketReadyEvent {
        const int mGeneration;
        OnPacketReady(int gen) : mGeneration(gen) { }
        virtual void onEvent(const sp<MediaPacket>& packet) {
            sp<Decoder> decoder = Looper::Current()->user(0);
            decoder->onPacketReady(packet, mGeneration);
        }
    };

    void onPacketReady(const sp<MediaPacket>& pkt, int generation) {
        if (mGeneration.load() != generation) {
            INFO("codec %zu: ignore outdated packets", mID);
            return;
        }

        if (pkt == NULL) {
            INFO("codec %zu: eos detected", mID);
            mInputEOS = true;
        } else {
            DEBUG("codec %zu: packet %.3f|%.3f(s) ready",
                    mID, pkt->dts.seconds(), pkt->pts.seconds());
            
            if (mPacketsReceived == 0) {
                INFO("codec %zu: first packet @ %.3f(s)|%.3f(s)", mID, pkt->pts.seconds(), pkt->dts.seconds());
            }

            ++mPacketsReceived;
            // @see MediaExtractor::read(), packets should in dts order.
            if (pkt->dts < mLastPacketTime) {
                WARN("codec %zu: unorderred packet %.3f(s) < last %.3f(s)",
                        mID, pkt->dts.seconds(), mLastPacketTime.seconds());
            }
            mLastPacketTime = pkt->dts;
            mInputQueue.push(pkt);
        }

        // always decode as long as there is packet exists.
        while (mFrameRequests.size() && mInputQueue.size()) {
            onDecode();
        }

        requestPacket();
    }

    void onPrepareDecoder(const MediaTime& ts) {
        INFO("codec %zu: prepare decoder @ %.3f(s)", mID, ts.seconds());
        CHECK_TRUE(ts != kTimeInvalid);
        
        // clear
        mInputQueue.clear();
        mCodec->flush();
        mFrameRequests.clear();

        // update generation
        mPacketReadyEvent = new OnPacketReady(++mGeneration);

        // reset flags
        mInputEOS = false;
        mOutputEOS = false;
        mSignalCodecEOS = false;
        mLastPacketTime = kTimeBegin;

        mPacketsReceived = 0;
        mPacketsComsumed = 0;
        mFramesDecoded = 0;

        // request packets
        requestPacket(ts);
        // -> onPacketReady
        // MIN_COUNT packets
    }

    void onRequestFrame(const FrameRequest& request) {
        DEBUG("codec %zu: request frames", mID);
        CHECK_TRUE(request.event != NULL);
        
        if (request.ts != kTimeInvalid) {
            // request frame @ new position
            onPrepareDecoder(request.ts);
        }
    
        // request next frame
        if (mOutputEOS) {
            ERROR("codec %zu: request frame at eos", mID);
            return;
        }
        CHECK_TRUE(request.event != NULL);

        mFrameRequests.push(request);
        // case 2: packet is ready
        // decode the first packet
        // or drain the codec if input eos
        if (mInputEOS || mInputQueue.size()) {
            onDecode();
        }
        // case 3: packet is not ready && not eos
        // request packet + pending a decode request
        else {
            requestPacket();
        }
    }

    void onDecode() {
        DEBUG("codec %zu: with %zu packets ready", mID, mInputQueue.size());
        CHECK_TRUE(mInputQueue.size() || mInputEOS);
        CHECK_FALSE(mOutputEOS);

        // push packets to codec
        if (mInputQueue.size()) {
            sp<MediaPacket> packet = *mInputQueue.begin();
            CHECK_TRUE(packet != NULL);

            DEBUG("codec %zu: decode pkt %.3f(s)", mID, packet->dts.seconds());
            MediaError err = mCodec->write(packet);
            if (err == kMediaErrorResourceBusy) {
                DEBUG("codec %zu: codec is full with frames", mID);
            } else {
                if (err != kMediaNoError) {
                    ERROR("codec %zu: write packet failed.", mID);
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
            WARN("codec %zu: is initializing...", mID);
        } else {
            FrameRequest& request = *mFrameRequests.begin();

            if (frame != NULL) {
                DEBUG("codec %zu: decoded frame %.3f(s) ready", mID, frame->pts.seconds());
                ++mFramesDecoded;
            } else {
                INFO("codec %zu: codec eos detected", mID);
                mOutputEOS = true;
            }
            request.event->fire(frame);

            if (mOutputEOS) {
                mFrameRequests.clear(); // clear requests
            } else {
                mFrameRequests.pop();
            }
        }

        // request more packets
        requestPacket();
    }
};

// request frame from decoder
struct OnFrameRequest : public FrameRequestEvent {
    OnFrameRequest(const sp<Looper>& looper) : FrameRequestEvent(looper) { }
    
    virtual void onEvent(const FrameRequest& request) {
        sp<Decoder> decoder = Looper::Current()->user(0);
        decoder->onRequestFrame(request);
    }
};

enum eRenderState {
    kRenderInitialized,
    kRenderReady,
    kRenderTicking,
    kRenderFlushed,
    kRenderEnd
};

struct Renderer : public SharedObject {
    // external static context
    sp<FrameRequestEvent>   mFrameRequestEvent;
    eCodecFormat            mID;
    sp<SessionInfoEvent>    mInfoEvent;
    // internal static context
    sp<Looper>              mDecoder;
    sp<FrameReadyEvent>     mFrameReadyEvent;
    sp<MediaFrameEvent>     mMediaFrameEvent;
    sp<MediaOut>            mOut;
    sp<ColorConvertor>      mColorConvertor;
    sp<Clock>               mClock;
    int64_t                 mLatency;

    // render scope context
    Atomic<int>             mGeneration;
    struct PresentRunnable;
    sp<PresentRunnable>     mPresentFrame;      // for present current frame
    List<sp<MediaFrame> >   mOutputQueue;       // output frame queue
    eRenderState            mState;
    bool                    mOutputEOS;
    
    MediaTime               mLastFrameTime;     // kTimeInvalid => first frame
    // clock context
    MediaTime               mLastUpdateTime;    // last clock update time
    // statistics
    size_t                  mFramesRenderred;

    bool valid() const { return mOut != NULL || mMediaFrameEvent != NULL; }

    Renderer(const sp<Message>& format, const sp<Message>& options) :
        SharedObject(),
        // external static context
        mFrameRequestEvent(NULL), mID(kCodecFormatUnknown), mInfoEvent(NULL),
        // internal static context
        mFrameReadyEvent(NULL),
        mOut(NULL), mColorConvertor(NULL), mClock(NULL), mLatency(0),
        // render context
        mGeneration(0),
        mPresentFrame(new PresentRunnable()), mState(kRenderInitialized), mOutputEOS(false),
        mLastFrameTime(kTimeInvalid),
        mLastUpdateTime(kTimeInvalid),
        // statistics
        mFramesRenderred(0)
    {
        // setup external context
        if (options->contains("SessionInfoEvent")) {
            mInfoEvent = options->findObject("SessionInfoEvent");
        }
        
        if (options->contains("Clock")) {
            mClock = options->findObject("Clock");
        }
        
        CHECK_TRUE(format->contains(kKeyFormat));
        mID = (eCodecFormat)format->findInt32(kKeyFormat);
        eCodecType type = GetCodecType(mID);

        sp<Message> raw;
        if (1 /* FIXME */) {
            sp<Decoder> decoder = new Decoder(format, options);
            if (decoder->valid() == false) return;
            
            mDecoder = Looper::Create(String::format("d.%zu", mID));
            mDecoder->loop();
            mDecoder->bind(decoder->RetainObject());
            mFrameRequestEvent = new OnFrameRequest(mDecoder);

            raw = decoder->mCodec->formats();
        } else {
            CHECK_TRUE(options->contains("PacketRequestEvent"));
            sp<PacketRequestEvent> pre = options->findObject("PacketRequestEvent");
            mFrameRequestEvent = new OnFrameRequestTunnel(pre);
            raw = format;
        }
        
        // setup out context
        CHECK_TRUE(raw->contains(kKeyFormat));
        
        INFO("output format %s", raw->string().c_str());
        if (options->contains("MediaFrameEvent")) {
            mMediaFrameEvent = options->findObject("MediaFrameEvent");
        } else {
            mOut = MediaOut::Create(type);
            if (mOut->prepare(raw, options) != kMediaNoError) {
                ERROR("track %zu: create out failed", mID);
                return;
            }
            
            if (type == kCodecTypeVideo) {
                ePixelFormat pixel = (ePixelFormat)raw->findInt32(kKeyFormat);
                ePixelFormat accpeted = (ePixelFormat)mOut->formats()->findInt32(kKeyFormat);// color convert
                if (accpeted != pixel) {
                    mColorConvertor = new ColorConvertor(accpeted);
                }
            } else if (type == kCodecTypeAudio) {
                mLatency = mOut->formats()->findInt32(kKeyLatency);
            } else {
                FATAL("FIXME");
            }
        }
    }

    virtual ~Renderer() {
    }
    
    void notify(eSessionInfoType info) {
        if (mInfoEvent != NULL) {
            mInfoEvent->fire(info);
        }
    }

    void requestFrame(int64_t us = -1) {
        // don't request frame if eos detected.
        if (mOutputEOS) return;

        // output queue: kMaxFrameNum at most
        if (mOutputQueue.size() >= MAX_COUNT) {
            DEBUG("renderer %zu: output queue is full", mID);
            return;
        }

        FrameRequest request;
        request.ts      = us < 0 ? kTimeInvalid : us;
        request.event   = mFrameReadyEvent;
        mFrameRequestEvent->fire(request);
        DEBUG("renderer %zu: request more frames", mID);
        // -> onFrameReady
    }

    struct OnFrameReady : public FrameReadyEvent {
        const int mGeneration;
        OnFrameReady(int gen) : mGeneration(gen) { }
        virtual void onEvent(const sp<MediaFrame>& frame) {
            sp<Renderer> renderer = Looper::Current()->user(0);
            renderer->onFrameReady(frame, mGeneration);
        }
    };

    void onFrameReady(const sp<MediaFrame>& frame, int generation) {
        DEBUG("renderer %zu: one frame ready", mID);
        if (mGeneration.load() != generation) {
            INFO("renderer %zu: ignore outdated frames", mID);
            return;
        }

        // case 1: eos
        if (frame == NULL) {
            INFO("renderer %zu: eos detected", mID);
            mOutputEOS = true;
            if (mLastFrameTime == kTimeInvalid) {
                WARN("renderer %zu: eos at start", mID);
                notify(kSessionInfoEnd);
            }
            // NOTHING TO DO
            return;
        }

        // check pts, kTimeInvalid < kTimeBegin
        if (frame->pts < kTimeBegin) {
            ERROR("renderer %zu: bad pts", mID);
            // FIXME:
        }

        // queue frame, frames must be pts order
        DEBUG("renderer %zu: %.3f(s)", mID, frame->pts.seconds());
        if (frame->pts <= mLastFrameTime) {
            WARN("renderer %zu: unordered frame %.3f(s) < last %.3f(s)",
                    mID, frame->pts.seconds(),
                    mLastFrameTime.seconds());
        }

        if (mColorConvertor != NULL) {
            // XXX: convert here or just before render????
            mOutputQueue.push(mColorConvertor->convert(frame));
        } else {
            mOutputQueue.push(frame);
        }

        // request more frames
        requestFrame();

        // prepare done ?
        if (mState == kRenderInitialized && (mOutputQueue.size() >= MIN_COUNT || mOutputEOS)) {
            INFO("renderer %zu: prepare done", mID);
            mState = kRenderReady;
            notify(kSessionInfoReady);
        }

        // always render the first video
        if (mLastFrameTime == kTimeInvalid) {
            sp<MediaFrame> frame = *mOutputQueue.begin();
            INFO("renderer %zu: first frame %.3f(s)",
                    mID, frame->pts.seconds());

            // notify about the first render postion
            notify(kSessionInfoBegin);

            if (GetCodecType(mID) == kCodecTypeVideo) {
                if (mMediaFrameEvent != NULL) mMediaFrameEvent->fire(frame);
                else mOut->write(frame);
            }

            if (mClock != NULL && mClock->role() == kClockRoleMaster) {
                DEBUG("renderer %zu: set clock time %.3f(s)", mID, frame->pts.seconds());
                mClock->set(frame->pts.useconds());
            }
        }

        // remember last frame pts
        mLastFrameTime = frame->pts;
    }

    void onPrepareRenderer(int64_t us) {
        INFO("renderer %zu: prepare renderer @ %.3f(s)", mID, us / 1E6);
        if (us < 0) us = 0;
        
        // update generation
        mFrameReadyEvent = new OnFrameReady(++mGeneration);

        // reset flags
        mState = kRenderInitialized;
        mLastUpdateTime = kTimeBegin;
        mOutputEOS = false;
        mLastFrameTime = kTimeInvalid;
        mOutputQueue.clear();

        // request frames
        requestFrame(us);
        // -> onFrameReady

        // if no clock, start render directly
        if (mClock == NULL && !Looper::Current()->exists(mPresentFrame)) {
            onStartRenderer();
        }
    }

    // just flush resources, leave state as it is, reset state on prepare
    void onFlushRenderer() {
        INFO("track %zu: flush %zu frames", mID, mOutputQueue.size());

        // update generation
        ++mGeneration;

        // remove present runnable
        Looper::Current()->remove(mPresentFrame);

        // flush output
        mOutputQueue.clear();
        if (mMediaFrameEvent != NULL) mMediaFrameEvent->fire(NULL);
        else mOut->flush();

        mState = kRenderFlushed;
    }

    struct PresentRunnable : public Runnable {
        PresentRunnable() : Runnable() { }
        virtual void run() {
            sp<Renderer> renderer = Looper::Current()->user(0);
            renderer->onRender();
        }
    };

    void onRender() {
        int64_t next = REFRESH_RATE;

        if (mClock != NULL && mClock->isPaused() && mClock->role() == kClockRoleSlave) {
            INFO("renderer %zu: clock is paused", mID);
        } else if (mOutputQueue.size()) {
            next = renderCurrent();
        } else if (mOutputEOS) {
            INFO("renderre %zu: eos, stop render", mID);
            // NOTHING
            return;
        } else {
            ERROR("renderer %zu: codec underrun...", mID);
        }

        if (next < 0) { // too early
            Looper::Current()->post(mPresentFrame, -next);
        } else {
            Looper::Current()->post(mPresentFrame, next);
        }

        requestFrame();

        // -> onRender
    }

    // render current frame
    // return negtive value if render current frame too early
    // return postive value for next frame render time
    FORCE_INLINE int64_t renderCurrent() {
        DEBUG("renderer %zu: render with %zu frame ready", mID, mOutputQueue.size());

        sp<MediaFrame> frame = *mOutputQueue.begin();
        CHECK_TRUE(frame != NULL);

        if (mClock != NULL) {
            // check render time
            if (mClock->role() == kClockRoleSlave) {
                // render too early or late ?
                int64_t late = mClock->get() - frame->pts.useconds();
                if (late > REFRESH_RATE) {
                    // render late: drop current frame
                    WARN("renderer %zu: render late by %.3f(s)|%.3f(s), drop frame... [%zu]",
                            mID, late/1E6, frame->pts.seconds(), mOutputQueue.size());
                    mOutputQueue.pop();
                    return 0;
                } else if (late < -REFRESH_RATE/2) {
                    // FIXME: sometimes render too early for unknown reason
                    DEBUG("renderer %zu: overrun by %.3f(s)|%.3f(s)...",
                            mID, -late/1E6, frame->pts.seconds());
                    return -late;
                }
            }
        }

        DEBUG("renderer %zu: render frame %.3f(s)", mID, frame->pts.seconds());
        MediaError rt = kMediaNoError;
        if (mOut != NULL) CHECK_TRUE(mOut->write(frame) == kMediaNoError);
        else mMediaFrameEvent->fire(frame);
        mOutputQueue.pop();
        ++mFramesRenderred;

        // setup clock to tick if we are master clock
        // FIXME: if current frame is too big, update clock after write will cause clock advance too far
        if (mClock != NULL && mClock->role() == kClockRoleMaster && !mClock->isTicking()) {
            const int64_t realTime = SystemTimeUs();
            INFO("renderer %zu: update clock %.3f(s)+%.3f(s)",
                    mID, frame->pts.seconds(), realTime / 1E6);
            mClock->update(frame->pts.useconds() - mLatency, realTime);
        }

        // next frame render time.
        if (mOutputQueue.size()) {
            if (mClock == NULL) {
                // if no clock exists, render next immediately
                return 0;
            }

            sp<MediaFrame> next = mOutputQueue.front();
            int64_t delay = next->pts.useconds() - mClock->get();
            if (delay < 0)  return 0;
            else            return delay;
        } else if (mOutputEOS) {
            INFO("renderer %zu: eos...", mID);
            // tell out device about eos
            if (mOut != NULL) mOut->write(NULL);
            else mMediaFrameEvent->fire(NULL);
            
            notify(kSessionInfoEnd);
        }
        INFO("refresh rate");
        return REFRESH_RATE;
    }

    void onStartRenderer() {
        INFO("renderer %zu: start", mID);
        //onPrintStat();

        // check
        if (Looper::Current()->exists(mPresentFrame)) {
            ERROR("renderer %zu: already started");
            return;
        }

        // start
        // case 1: start at eos
        if (mOutputEOS && mOutputQueue.empty()) {
            // eos, do nothing
            ERROR("renderer %zu: start at eos", mID);
        }
        // case 3: frames ready
        else {
            if (mOut != NULL) {
                sp<Message> options = new Message;
                options->setInt32(kKeyPause, 0);
                mOut->configure(options);
            }

            onRender();
        }
    }

    void onPauseRenderer() {
        INFO("renderer %zu: pause at %.3f(s)", mID, mClock->get() / 1E6);
        Looper::Current()->remove(mPresentFrame);

        if (mOut != NULL) {
            sp<Message> options = new Message;
            options->setInt32(kKeyPause, 1);
            mOut->configure(options);
        }
    }
};

struct PrepareRunnable : public Runnable {
    sp<Message>     mOptions;
    PrepareRunnable(const sp<Message>& options) : mOptions(options) { }
    virtual void run() {
        int64_t us = mOptions->findInt64("time");
        sp<Renderer> renderer = Looper::Current()->user(0);
        renderer->onPrepareRenderer(us);
    }
};

struct FlushRunnable : public Runnable {
    FlushRunnable() : Runnable() { }
    virtual void run() {
        sp<Renderer> renderer = Looper::Current()->user(0);
        renderer->onFlushRenderer();
    }
};

// using clock to control render session, start|pause|...
struct OnClockEvent : public ClockEvent {
    OnClockEvent(const sp<Looper>& lp) : ClockEvent(lp) { }

    virtual void onEvent(const eClockState& cs) {
        sp<Renderer> renderer = Looper::Current()->user(0);
        INFO("clock state => %d", cs);
        switch (cs) {
            case kClockStateTicking:
                renderer->onStartRenderer();
                break;
            case kClockStatePaused:
                renderer->onPauseRenderer();
                break;
            case kClockStateReset:
                renderer->onPauseRenderer();
                break;
            default:
                break;
        }
    }
};

struct AVSession : public IMediaSession {
    sp<Looper>  mLooper;

    bool valid() const { return mLooper != NULL; }

    AVSession(const sp<Message>& format, const sp<Message>& options) : IMediaSession() {
        sp<Renderer> renderer = new Renderer(format, options);
        if (renderer->valid() == false) {
            return;
        }
        
        mLooper = Looper::Create(String::format("%#x", renderer->mID));
        mLooper->loop();
        mLooper->bind(renderer->RetainObject());

        if (renderer->mClock != NULL)
            renderer->mClock->setListener(new OnClockEvent(mLooper));
    }
    
    virtual ~AVSession() { CHECK_TRUE(mLooper == NULL); }

    virtual void prepare(const sp<Message>& options) {
        mLooper->post(new PrepareRunnable(options));
    }

    virtual void flush() {
        mLooper->post(new FlushRunnable);
    }
    
    virtual void release() {
        mLooper->terminate(true);
        static_cast<SharedObject *>(mLooper->user(0))->ReleaseObject();
        mLooper.clear();
        INFO("release av session");
    }
};

// PacketRequestEvent <- DecodeSession <- FrameRequestEvent <- RenderSession
sp<IMediaSession> IMediaSession::Create(const sp<Message>& format, const sp<Message>& options) {
    sp<AVSession> av = new AVSession(format, options);
    if (av->valid())    return av;
    else                return NULL;
}
