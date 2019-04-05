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

#define LOG_TAG "Session"
//#define LOG_NDEBUG 0
#include <ABE/ABE.h>

#include "MediaSession.h"

// TODO: calc count based on duration
// max count: 1s
// min count: 500ms
#define MIN_COUNT (4)
#define MAX_COUNT (16)
#define REFRESH_RATE (10000LL)    // 10ms

enum {
    INDEX0,
    INDEX1,
    N_INDEX
};

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

struct __ABE_HIDDEN Decoder : public SharedObject {
    // external static context
    eCodecFormat            mID;
    sp<PacketRequestEvent>  mPacketRequestEvent;        // where we get packets
    // internal static context
    sp<MediaDecoder>        mCodec;                     // reference to codec
    sp<PacketReadyEvent>    mPacketReadyEvent;          // when packet ready

    // internal mutable context
    // TODO: clock for decoder, handle late frames
    volatile int            mGeneration;
    bool                    mFlushed;
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

    Decoder(const Message& format, const Message& options) :
        // external static context
        mID(kCodecFormatUnknown),
        mPacketRequestEvent(NULL),
        // internal static context
        mCodec(NULL), mPacketReadyEvent(NULL),
        // internal mutable context
        mGeneration(0), mFlushed(false),
        mInputEOS(false), mSignalCodecEOS(false), mLastPacketTime(kTimeInvalid),
        // statistics
        mPacketsReceived(0), mPacketsComsumed(0), mFramesDecoded(0)
    {
        CHECK_TRUE(options.contains("PacketRequestEvent"));
        mPacketRequestEvent = options.findObject("PacketRequestEvent");

        // setup decoder...
        CHECK_TRUE(format.contains(kKeyFormat));
        mID = (eCodecFormat)format.findInt32(kKeyFormat);
        eModeType mode = (eModeType)options.findInt32(kKeyMode, kModeTypeDefault);

        eCodecType type = GetCodecType(mID);

        // always set mCodec to null, if create or initial faileds
        mCodec = MediaDecoder::Create(mID, mode);
        if (mCodec->init(format, options) != kMediaNoError) {
            ERROR("track %zu: create codec failed", mID);
            mCodec.clear();

#if 1
            if (mode != kModeTypeSoftware) {
                Message options_software = options;
                options_software.setInt32(kKeyMode, kModeTypeSoftware);
                mCodec = MediaDecoder::Create(mID, kModeTypeSoftware);

                if (mCodec->init(format, options_software) != kMediaNoError) {
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
        OnPacketReady(int gen) : PacketReadyEvent(String::format("OnPacketReady %d", gen), Looper::Current()), mGeneration(gen) { }
        virtual void onEvent(const sp<MediaPacket>& packet) {
            sp<Decoder> decoder = Looper::Current()->user(INDEX0);
            decoder->onPacketReady(packet, mGeneration);
        }
    };

    void onPacketReady(const sp<MediaPacket>& pkt, int generation) {
        if (atomic_load(&mGeneration) != generation) {
            INFO("codec %zu: ignore outdated packets", mID);
            return;
        }

        if (pkt == NULL) {
            INFO("codec %zu: eos detected", mID);
            mInputEOS = true;
        } else {
            DEBUG("codec %zu: packet %.3f|%.3f(s) ready",
                    mID, pkt->dts.seconds(), pkt->pts.seconds());

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
        INFO("codec %zu: prepare decoder", mID);
        CHECK_TRUE(ts != kTimeInvalid);

        // update generation
        atomic_add(&mGeneration, 1);
        mPacketReadyEvent = new OnPacketReady(mGeneration);

        // TODO: prepare again without flush
        mFlushed = false;
        mInputEOS = false;
        mOutputEOS = false;
        mSignalCodecEOS = false;
        mInputQueue.clear();
        mCodec->flush();
        mLastPacketTime = kTimeBegin;

        mPacketsReceived = 0;
        mPacketsComsumed = 0;
        mFramesDecoded = 0;

        // request packets
        requestPacket(ts);
        // -> onPacketReady
        // MIN_COUNT packets
    }

    void onFlushDecoder() {
        INFO("codec %zu: flush %zu packets", mID, mInputQueue.size());

        // update generation
        atomic_add(&mGeneration, 1);
        mPacketReadyEvent = NULL;

        // remove cmds
        mFrameRequests.clear();

        // flush input queue and codec
        mInputQueue.clear();
        mCodec->flush();

        mFlushed = true;
    }

    void onRequestFrame(const FrameRequest& request) {
        DEBUG("codec %zu: request frames", mID);

        if (request.event == NULL) {
            // request to flush
            if (request.ts == kTimeEnd) {
                onFlushDecoder();
            }
            // request to prepare @ ts
            else if (request.ts != kTimeInvalid) {
                onPrepareDecoder(request.ts);
            }
            return;
        }

        if (mOutputEOS) {
            ERROR("codec %zu: request frame at eos", mID);
            return;
        }
        CHECK_TRUE(request.event != NULL);

        // request.ts == kTimeInvalid => request next
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

struct __ABE_HIDDEN Renderer : public SharedObject {
    // external static context
    sp<FrameRequestEvent>   mFrameRequestEvent;
    eCodecFormat            mID;
    sp<RenderPositionEvent> mPositionEvent;
    // internal static context
    sp<FrameReadyEvent>     mFrameReadyEvent;
    sp<MediaOut>            mOut;
    sp<ColorConvertor>      mColorConvertor;
    sp<Clock>               mClock;
    int64_t                 mLatency;

    // render scope context
    volatile int            mGeneration;
    struct PresentRunnable;
    sp<PresentRunnable>     mPresentFrame;      // for present current frame
    List<sp<MediaFrame> >   mOutputQueue;       // output frame queue
    bool                    mOutputEOS;         // output frame eos
    bool                    mFlushed;
    MediaTime               mLastFrameTime;     // kTimeInvalid => first frame
    sp<StatusEvent>         mStatusEvent;
    // clock context
    MediaTime               mLastUpdateTime;    // last clock update time
    // statistics
    size_t                  mFramesRenderred;

    bool valid() const { return mOut != NULL; }

    Renderer(eCodecFormat id,
            const Message& format,
            const Message& options,
            const sp<FrameRequestEvent>& fre) :
        SharedObject(),
        // external static context
        mFrameRequestEvent(fre), mID(id),
        mPositionEvent(NULL),
        // internal static context
        mFrameReadyEvent(NULL),
        mOut(NULL), mColorConvertor(NULL), mClock(NULL), mLatency(0),
        // render context
        mGeneration(0),
        mPresentFrame(new PresentRunnable()),
        mOutputEOS(false), mFlushed(false),
        mLastFrameTime(kTimeInvalid),
        mStatusEvent(NULL),
        mLastUpdateTime(kTimeInvalid),
        // statistics
        mFramesRenderred(0)
    {
        // setup external context
        if (options.contains("RenderPositionEvent")) {
            mPositionEvent = options.findObject("RenderPositionEvent");
        }

        if (options.contains("Clock")) {
            mClock = options.findObject("Clock");
        }

        // setup out context
        CHECK_TRUE(format.contains(kKeyFormat));

        eCodecType type = GetCodecType(mID);

        INFO("output format %s", format.string().c_str());
        if (options.contains("MediaOut")) {
            mOut = options.findObject("MediaOut");
        } else {
            mOut = MediaOut::Create(type);
        }
        
        if (mOut->prepare(format) != kMediaNoError) {
            ERROR("track %zu: create out failed", mID);
            return;
        }

        if (type == kCodecTypeVideo) {
            ePixelFormat pixel = (ePixelFormat)format.findInt32(kKeyFormat);
            ePixelFormat accpeted = (ePixelFormat)mOut->formats().findInt32(kKeyFormat);// color convert
            if (accpeted != pixel) {
                mColorConvertor = new ColorConvertor(accpeted);
            }
        } else if (type == kCodecTypeAudio) {
            mLatency = mOut->formats().findInt32(kKeyLatency);
        } else {
            FATAL("FIXME");
        }
    }

    virtual ~Renderer() {
    }

    void requestFrame() {
        // don't request frame if eos detected.
        if (mOutputEOS) return;

        // output queue: kMaxFrameNum at most
        if (mOutputQueue.size() >= MAX_COUNT) {
            DEBUG("renderer %zu: output queue is full", mID);
            return;
        }

        FrameRequest request;
        request.ts      = kTimeInvalid;
        request.event   = mFrameReadyEvent;
        mFrameRequestEvent->fire(request);
        DEBUG("renderer %zu: request more frames", mID);
        // -> onFrameReady
    }

    struct OnFrameReady : public FrameReadyEvent {
        const int mGeneration;
        OnFrameReady(int gen) : FrameReadyEvent(String::format("OnFrameReady %d", gen), Looper::Current()), mGeneration(gen) { }
        virtual void onEvent(const sp<MediaFrame>& frame) {
            sp<Renderer> renderer = Looper::Current()->user(INDEX1);
            renderer->onFrameReady(frame, mGeneration);
        }
    };

    void onFrameReady(const sp<MediaFrame>& frame, int generation) {
        DEBUG("renderer %zu: one frame ready", mID);
        if (atomic_load(&mGeneration) != generation) {
            INFO("renderer %zu: ignore outdated frames", mID);
            return;
        }

        // case 1: eos
        if (frame == NULL) {
            INFO("renderer %zu: eos detected", mID);
            mOutputEOS = true;
            if (mLastFrameTime == kTimeInvalid) {
                WARN("renderer %zu: eos at start", mID);
                mStatusEvent->fire(kMediaErrorUnknown);
                mStatusEvent = NULL;
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
        if (mStatusEvent != NULL && (mOutputQueue.size() >= MIN_COUNT || mOutputEOS)) {
            INFO("renderer %zu: prepare done", mID);
            mStatusEvent->fire(kMediaNoError);
            mStatusEvent = NULL;
        }

        // always render the first video
        if (mLastFrameTime == kTimeInvalid) {
            sp<MediaFrame> frame = *mOutputQueue.begin();
            INFO("renderer %zu: first frame %.3f(s)",
                    mID, frame->pts.seconds());

            // notify about the first render postion
            if (mPositionEvent != NULL) {
                mPositionEvent->fire(frame->pts);
            }

            if (GetCodecType(mID) == kCodecTypeVideo) {
                mOut->write(frame);
            }

            if (mClock != NULL && mClock->role() == kClockRoleMaster) {
                DEBUG("renderer %zu: set clock time %.3f(s)", mID, frame->pts.seconds());
                mClock->set(frame->pts);
            }
        }

        // remember last frame pts
        mLastFrameTime = frame->pts;
    }

    void onPrepareRenderer(const Message& options) {
        const MediaTime& ts = options.find<MediaTime>("time");
        sp<StatusEvent> se;
        if (options.contains("StatusEvent")) {
            se = options.findObject("StatusEvent");
        }
        onPrepareRenderer(ts, se);
    }

    void onPrepareRenderer(const MediaTime& ts, const sp<StatusEvent>& se) {
        INFO("renderer %zu: prepare renderer...", mID);
        CHECK_TRUE(ts >= kTimeBegin);

        // update generation
        atomic_add(&mGeneration, 1);
        mFrameReadyEvent = new OnFrameReady(mGeneration);

        // tell decoder to prepare
        FrameRequest request;
        request.ts = ts;
        mFrameRequestEvent->fire(request);

        // reset flags
        mFlushed = false;
        mLastUpdateTime = 0;
        mOutputEOS = false;
        mLastFrameTime = kTimeInvalid;
        mStatusEvent = se;
        mOutputQueue.clear();

        // request frames
        requestFrame();
        // -> onFrameReady

        // if no clock, start render directly
        if (mClock == NULL && !Looper::Current()->exists(mPresentFrame)) {
            onStartRenderer();
        }
    }

    void onFlushRenderer() {
        INFO("track %zu: flush %zu frames", mID, mOutputQueue.size());

        // update generation
        atomic_add(&mGeneration, 1);
        mFrameReadyEvent = NULL;

        Looper::Current()->flush();

        // tell decoder to flush
        FrameRequest request;
        request.ts = kTimeEnd;
        mFrameRequestEvent->fire(request);

        // remove present runnable
        Looper::Current()->remove(mPresentFrame);

        // flush output
        mOutputEOS = false;
        mOutputQueue.clear();
        if (mOut != NULL) mOut->flush();

        // reset flags
        mLastUpdateTime = 0;

        // reset statistics
        mFramesRenderred = 0;

        mFlushed = true;
    }

    struct PresentRunnable : public Runnable {
        PresentRunnable() : Runnable() { }
        virtual void run() {
            sp<Renderer> renderer = Looper::Current()->user(INDEX1);
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
    __ABE_INLINE int64_t renderCurrent() {
        DEBUG("renderer %zu: render with %zu frame ready", mID, mOutputQueue.size());

        sp<MediaFrame> frame = *mOutputQueue.begin();
        CHECK_TRUE(frame != NULL);

        if (mClock != NULL) {
            // check render time
            if (mClock->role() == kClockRoleSlave) {
                // render too early or late ?
                int64_t late = mClock->get().useconds() - frame->pts.useconds();
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
        MediaError rt = mOut->write(frame);
        CHECK_TRUE(rt == kMediaNoError); // always eat frame
        mOutputQueue.pop();
        ++mFramesRenderred;

        // setup clock to tick if we are master clock
        // FIXME: if current frame is too big, update clock after write will cause clock advance too far
        if (mClock != NULL && mClock->role() == kClockRoleMaster && !mClock->isTicking()) {
            const int64_t realTime = SystemTimeUs();
            INFO("renderer %zu: update clock %.3f(s)+%.3f(s)",
                    mID, frame->pts.seconds(), realTime / 1E6);
            mClock->update(frame->pts - mLatency, realTime);
        }

        // broadcast render position to others every 1s
        if (frame->pts - mLastUpdateTime > 1000000LL) {
            mLastUpdateTime = frame->pts;
            if (mPositionEvent != NULL) {
                mPositionEvent->fire(frame->pts);
            }
        }

        // next frame render time.
        if (mOutputQueue.size()) {
            if (mClock == NULL) {
                // if no clock exists, render next immediately
                return 0;
            }

            sp<MediaFrame> next = mOutputQueue.front();
            MediaTime delay = next->pts - mClock->get();
            if (delay < kTimeBegin) return 0;
            else                    return delay.useconds();
        } else if (mOutputEOS) {
            INFO("renderer %zu: eos...", mID);
            // tell out device about eos
            mOut->write(NULL);

            if (mPositionEvent != NULL) {
                mPositionEvent->fire(kTimeEnd);
            }
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
                Message options;
                options.setInt32(kKeyPause, 0);
                mOut->configure(options);
            }

            onRender();
        }
    }

    void onPauseRenderer() {
        INFO("renderer %zu: pause at %.3f(s)", mID, mClock->get().seconds());
        Looper::Current()->remove(mPresentFrame);

        if (mOut != NULL) {
            Message options;
            options.setInt32(kKeyPause, 1);
            mOut->configure(options);
        }
    }
};

// request frame from decoder
struct __ABE_HIDDEN OnFrameRequest : public FrameRequestEvent {
    OnFrameRequest(const sp<Looper>& looper) : FrameRequestEvent(looper) { }
    
    virtual void onEvent(const FrameRequest& request) {
        sp<Decoder> decoder = Looper::Current()->user(INDEX0);
        decoder->onRequestFrame(request);
    }
};

struct __ABE_HIDDEN MediaFrameTunnel : public MediaFrame {
    sp<MediaPacket>     mPacket;
    MediaFrameTunnel(const sp<MediaPacket>& packet) : MediaFrame(), mPacket(packet) {
        pts                 = packet->pts;
        duration            = kTimeInvalid;
        planes[0].data      = packet->data;
        planes[0].size      = packet->size;
        // FIXME
    }
};

struct __ABE_HIDDEN OnPacketReadyTunnel : public PacketReadyEvent {
    sp<FrameReadyEvent>     mFrameReadyEvent;
    OnPacketReadyTunnel(const sp<FrameReadyEvent>& event) :
        PacketReadyEvent(), mFrameReadyEvent(event) { }
    virtual void onEvent(const sp<MediaPacket>& packet) {
        mFrameReadyEvent->fire(new MediaFrameTunnel(packet));
    }
};

struct __ABE_HIDDEN OnFrameRequestTunnel : public FrameRequestEvent {
    sp<PacketRequestEvent>      mPacketRequestEvent;
    OnFrameRequestTunnel(const sp<PacketRequestEvent>& event) :
        FrameRequestEvent(), mPacketRequestEvent(event) { }

    virtual void onEvent(const FrameRequest& request) {
        PacketRequest packetRequest;
        if (request.ts == kTimeEnd) {
            return;
        }
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

struct PrepareRunnable : public Runnable {
    Message             mOptions;
    PrepareRunnable(const Message& options) : mOptions(options) { }
    virtual void run() {
        sp<Renderer> renderer = Looper::Current()->user(INDEX1);
        renderer->onPrepareRenderer(mOptions);
    }
};

struct FlushRunnable : public Runnable {
    FlushRunnable() : Runnable() { }
    virtual void run() {
        sp<Renderer> renderer = Looper::Current()->user(INDEX1);
        renderer->onFlushRenderer();
    }
};

// using clock to control render session, start|pause|...
struct __ABE_HIDDEN OnClockEvent : public ClockEvent {
    OnClockEvent(const sp<Looper>& lp) : ClockEvent(lp) { }

    virtual void onEvent(const eClockState& cs) {
        sp<Renderer> renderer = Looper::Current()->user(INDEX1);
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

struct __ABE_HIDDEN AVSession : public IMediaSession {
    Vector<sp<Looper> >     mLoopers;

    bool valid() const { return mLoopers.size(); }

    AVSession(const Message& format, const Message& options) : IMediaSession() {
        CHECK_TRUE(format.contains(kKeyFormat));
        eCodecFormat codec = (eCodecFormat)format.findInt32(kKeyFormat);
        eCodecType type = GetCodecType(codec);

        sp<Decoder> decoder = new Decoder(format, options);
        if (decoder->valid() == false) return;

        mLoopers.push(Looper::Create(String::format("d.%#x", codec)));

        sp<FrameRequestEvent> fre = new OnFrameRequest(mLoopers[INDEX0]);

        sp<Renderer> renderer = new Renderer(codec, decoder->mCodec->formats(), options, fre);
        if (renderer->valid() == false) {
            mLoopers.clear();
            return;
        }

        mLoopers[INDEX0]->bind(INDEX0, decoder->RetainObject());
        if (type == kCodecTypeVideo) {
            mLoopers[INDEX0]->bind(INDEX1, NULL);

            mLoopers.push(Looper::Create(String::format("r.%#x", codec)));

            mLoopers[INDEX1]->bind(INDEX0, NULL);
            mLoopers[INDEX1]->bind(INDEX1, renderer->RetainObject());
        } else {
            mLoopers[INDEX0]->bind(INDEX1, renderer->RetainObject());
        }

        if (renderer->mClock != NULL)
            renderer->mClock->setListener(new OnClockEvent(mLoopers.back()));

        for (size_t i = 0; i < mLoopers.size(); ++i) {
            mLoopers[i]->loop();
            mLoopers[i]->profile();
        }
    }
    
    virtual ~AVSession() { CHECK_TRUE(mLoopers.empty()); }

    virtual void prepare(const Message& options) {
        mLoopers.back()->post(new PrepareRunnable(options));
    }

    virtual void flush() {
        mLoopers.back()->post(new FlushRunnable);
    }
    
    virtual void release() {
        for (size_t i = 0; i < mLoopers.size(); ++i) {
            mLoopers[i]->terminate(true);
            for (size_t j = INDEX0; j < N_INDEX; ++j) {
                if (mLoopers[i]->user(j) != NULL) {
                    static_cast<SharedObject *>(mLoopers[i]->user(j))->ReleaseObject();
                }
            }
        }
        mLoopers.clear();
        INFO("release av session");
    }
};

// PacketRequestEvent <- DecodeSession <- FrameRequestEvent <- RenderSession
sp<IMediaSession> IMediaSession::Create(const Message& format, const Message& options) {
    sp<AVSession> av = new AVSession(format, options);
    if (av->valid())    return av;
    else                return NULL;
}

