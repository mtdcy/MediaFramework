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
#include <MediaToolkit/Toolkit.h>

#include "MediaSession.h"
#include "sdl2/SDLAudio.h"

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

using namespace mtdcy;

// no control session, control by FrameRequestEvent.
struct DecodeSession : public SharedObject {
    // external static context
    eCodecFormat            mID;
    sp<Looper>              mLooper;
    sp<PacketRequestEvent>  mPacketRequestEvent;        // where we get packets
    // internal static context
    sp<MediaDecoder>        mCodec;         // reference to codec
    sp<PacketReadyEvent>    mPacketReadyEvent;          // when packet ready

    // internal mutable context
    // TODO: clock for decoder, handle late frames
    volatile int            mGeneration;
    bool                    mFlushed;
    List<sp<MediaPacket> >  mInputQueue;    // input packets queue
    bool                    mInputEOS;      // end of input ?
    bool                    mSignalCodecEOS;    // tell codec eos
    bool                    mOutputEOS;         // end of output ?
    MediaTime               mLastPacketTime;    // test packets in dts order?
    List<FrameRequest>      mFrameRequests; // requests queue
    // statistics
    size_t                  mPacketsReceived;
    size_t                  mPacketsComsumed;
    size_t                  mFramesDecoded;

    DecodeSession(const sp<Looper>& looper,
                  const Message& format,
                  const Message& options,
                  const sp<PacketRequestEvent>& fre) :
        // external static context
        mID(kCodecFormatUnknown),
        mLooper(looper), mPacketRequestEvent(fre),
        // internal static context
        mCodec(NULL), mPacketReadyEvent(NULL),
        // internal mutable context
        mGeneration(0), mFlushed(false),
        mInputEOS(false), mSignalCodecEOS(false), mLastPacketTime(kTimeInvalid),
        // statistics
        mPacketsReceived(0), mPacketsComsumed(0), mFramesDecoded(0)
    {
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

    virtual ~DecodeSession() {
        mLooper->terminate(true );
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
        OnPacketReady(const sp<Looper>& looper, int gen) :
        PacketReadyEvent(String::format("OnPacketReady %d", gen), looper),
        mGeneration(gen) { }
        virtual void onEvent(const sp<MediaPacket>& packet) {
            sp<Looper> current = Looper::Current();
            DecodeSession *ds = static_cast<DecodeSession*>(current->user(0));
            ds->onPacketReady(packet, mGeneration);
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
        mPacketReadyEvent = new OnPacketReady(mLooper, mGeneration);

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
        
        mLooper->flush();

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

struct RenderSession : public MediaSession {
    // external static context
    sp<Looper>              mLooper;
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

    RenderSession(const sp<Looper>& looper,
                  eCodecFormat id,
                  const Message& format,
                  const Message& options,
                  const sp<FrameRequestEvent>& fre) :
        MediaSession(), // share the same looper
        // external static context
        mLooper(looper), mFrameRequestEvent(fre), mID(id),
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
            mPositionEvent = options.find<sp<RenderPositionEvent> >("RenderPositionEvent");
        }

        if (options.contains("Clock")) {
            mClock = options.find<sp<Clock> >("Clock");
        }
        
        // setup out context
        CHECK_TRUE(format.contains(kKeyFormat));

        eCodecType type = GetCodecType(mID);

        INFO("output format %s", format.string().c_str());

        if (type == kCodecTypeVideo) {
            ePixelFormat pixel = (ePixelFormat)format.findInt32(kKeyFormat);
            ePixelFormat pixFmtAccepted;
            if (options.contains("MediaOut")) {
                mOut = options.find<sp<MediaOut> >("MediaOut");
            } else {
                mOut = MediaOut::Create(kCodecTypeVideo);
            }

            if (mOut->prepare(format) != OK) {
                ERROR("track %zu: create out failed", mID);
                return;
            }

            pixFmtAccepted = (ePixelFormat)mOut->formats().findInt32(kKeyFormat);// color convert
            if (pixFmtAccepted != pixel) {
                mColorConvertor = new ColorConvertor(pixFmtAccepted);
            }
        } else if (type == kCodecTypeAudio) {
            mOut = new SDLAudio();

            if (mOut->prepare(format) != OK) {
                ERROR("track %zu: create out failed", mID);
                return;
            }

            mLatency = mOut->formats().findInt32(kKeyLatency);
        } else {
            FATAL("FIXME");
        }

        if (mClock != NULL) {
            mClock->setListener(new OnClockEvent(mLooper));
        }
    }

    virtual ~RenderSession() {
        mLooper->terminate(true);
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
        OnFrameReady(const sp<Looper>& looper, int gen) :
        FrameReadyEvent(String::format("OnFrameReady %d", gen), looper),
        mGeneration(gen) { }
        virtual void onEvent(const sp<MediaFrame>& frame) {
            sp<Looper> current = Looper::Current();
            RenderSession *rs = static_cast<RenderSession*>(current->user(0));
            rs->onFrameReady(frame, mGeneration);
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
            se = options.find<sp<StatusEvent> >("StatusEvent");
        }
        onPrepareRenderer(ts, se);
    }

    void onPrepareRenderer(const MediaTime& ts, const sp<StatusEvent>& se) {
        INFO("renderer %zu: prepare renderer...", mID);
        CHECK_TRUE(ts >= kTimeBegin);
        
        // update generation
        atomic_add(&mGeneration, 1);
        mFrameReadyEvent = new OnFrameReady(mLooper, mGeneration);

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
        if (mClock == NULL && !mLooper->exists(mPresentFrame)) {
            onStartRenderer();
        }
    }

    void onFlushRenderer() {
        INFO("track %zu: flush %zu frames", mID, mOutputQueue.size());
        
        // update generation
        atomic_add(&mGeneration, 1);
        mFrameReadyEvent = NULL;

        mLooper->flush();
        
        // tell decoder to flush
        FrameRequest request;
        request.ts = kTimeEnd;
        mFrameRequestEvent->fire(request);

        // remove present runnable
        mLooper->remove(mPresentFrame);

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
            sp<Looper> current = Looper::Current();
            RenderSession *rs = static_cast<RenderSession*>(current->user(0));
            rs->onRender();
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
            mLooper->post(mPresentFrame, -next);
        } else {
            mLooper->post(mPresentFrame, next);
        }
        
        requestFrame();

        // -> onRender
    }

    int64_t renderCurrent() {
        DEBUG("renderer %zu: render with %zu frame ready", mID, mOutputQueue.size());

        sp<MediaFrame> frame = *mOutputQueue.begin();
        CHECK_TRUE(frame != NULL);

        // render too early ?
        if (mClock != NULL && mClock->role() == kClockRoleSlave) {
            int64_t delay = frame->pts.useconds() - mClock->get().useconds();
            if (delay < -REFRESH_RATE) {
                WARN("renderer %zu: render late by %.3f(s)|%.3f(s), drop frame...",
                        mID, -delay/1E6, frame->pts.seconds());
                mOutputQueue.pop();
                return -delay;
            } else if (delay > REFRESH_RATE / 2) {
                // FIXME: sometimes render too early for unknown reason
                INFO("renderer %zu: overrun by %.3f(s)|%.3f(s)...",
                        mID, delay/1E6, frame->pts.seconds());
                return -delay;
            }
        }

        DEBUG("renderer %zu: render frame %.3f(s)", mID, frame->pts.seconds());
        status_t rt = mOut->write(frame);

        CHECK_TRUE(rt == OK); // always eat frame
        mOutputQueue.pop();

        ++mFramesRenderred;

        int64_t realTime = SystemTimeUs();

        // update clock
        if (mClock != NULL && mClock->role() == kClockRoleMaster
                && !mClock->isTicking()) {
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
        if (mClock != NULL && mOutputQueue.size()) {
            sp<MediaFrame> next = *mOutputQueue.begin();
            int64_t delay = next->pts.useconds() - mClock->get().useconds();
            if (delay < 0) {
                if (delay > REFRESH_RATE)
                    WARN("renderer %zu: frame late by %.3f(s)|%.3f(s)",
                            mID, -delay/1E6, next->pts.seconds());
                delay = 0;
            } else if (delay < REFRESH_RATE / 2) {
                delay = 0;
            } else {
                DEBUG("renderer %zu: render next frame %.3f(s)|%.3f(s) later",
                        mID, delay/1E6, next->pts.seconds());
            }
            return delay;
        } else if (mOutputEOS) {
            INFO("renderer %zu: eos...", mID);
            // tell out device about eos
            mOut->write(NULL);

            if (mPositionEvent != NULL) {
                mPositionEvent->fire(kTimeEnd);
            }
        } else if (mClock != NULL) {
            WARN("renderer %zu: codec slightly underrun...", mID);
        }
        return REFRESH_RATE;
    }

    // using clock to control render session
    struct OnClockEvent : public ClockEvent {
        OnClockEvent(const sp<Looper>& lp) : ClockEvent(lp) { }

        virtual void onEvent(const eClockState& cs) {
            sp<Looper> current = Looper::Current();
            RenderSession *rs = static_cast<RenderSession*>(current->user(0));
            INFO("clock state => %d", cs);
            switch (cs) {
                case kClockStateTicking:
                    rs->onStartRenderer();
                    break;
                case kClockStatePaused:
                    rs->onPauseRenderer();
                    break;
                case kClockStateReset:
                    rs->onPauseRenderer();
                    break;
                default:
                    break;
            }
        }
    };

    void onStartRenderer() {
        INFO("renderer %zu: start", mID);
        //onPrintStat();

        // check
        if (mLooper->exists(mPresentFrame)) {
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
        mLooper->remove(mPresentFrame);

        if (mOut != NULL) {
            Message options;
            options.setInt32(kKeyPause, 1);
            mOut->configure(options);
        }
    }
    
    // MediaSession
    struct PrepareRunnable : public Runnable {
        Message             mOptions;
        PrepareRunnable(const Message& options) : mOptions(options) { }
        virtual void run() {
            sp<Looper> current = Looper::Current();
            RenderSession *rs = static_cast<RenderSession*>(current->user(0));
            rs->onPrepareRenderer(mOptions);
        }
    };

    virtual void prepare(const Message& options) {
        mLooper->post(new PrepareRunnable(options));
    }

    struct FlushRunnable : public Runnable {
        FlushRunnable() : Runnable() { }
        virtual void run() {
            sp<Looper> current = Looper::Current();
            RenderSession *rs = static_cast<RenderSession*>(current->user(0));
            rs->onFlushRenderer();
        }
    };

    virtual void flush() {
        mLooper->post(new FlushRunnable());
    }
};

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

struct OnFrameRequest : public FrameRequestEvent {
    sp<DecodeSession>   mDecodeSession;
    OnFrameRequest(const sp<Looper>& looper, const sp<DecodeSession>& ds) :
    FrameRequestEvent(looper), mDecodeSession(ds) {
    }
    
    virtual ~OnFrameRequest() {
    }
    
    virtual void onEvent(const FrameRequest& request) {
        mDecodeSession->onRequestFrame(request);
    }
};

// PacketRequestEvent <- DecodeSession <- FrameRequestEvent <- RenderSession
sp<MediaSession> MediaSession::Create(const Message& format, const Message& options) {
    CHECK_TRUE(format.contains(kKeyFormat));
    eCodecFormat codec = (eCodecFormat)format.findInt32(kKeyFormat);
    eCodecType type = GetCodecType(codec);

    CHECK_TRUE(options.contains("PacketRequestEvent"));
    sp<PacketRequestEvent> pre = options.find<sp<PacketRequestEvent> >("PacketRequestEvent");
    
    sp<Looper> looper = new Looper(String::format("ds.%#x", codec));
    sp<DecodeSession> ds = new DecodeSession(looper,
                                             format,
                                             options,
                                             pre);
    looper->bind(ds.get());
    looper->loop();
    
    if (ds->mCodec == NULL) {
        ERROR("failed to initial decoder");
        return NULL;
    }
    
    sp<FrameRequestEvent> fre = new OnFrameRequest(looper, ds);
    
    looper = new Looper(String::format("rs.%#x", codec));
    sp<RenderSession> rs = new RenderSession(looper,
                                             codec,
                                             ds->mCodec->formats(),
                                             options,
                                             fre);
    looper->bind(rs.get());
    looper->loop();

    // test if RenderSession is valid
    if (rs->mOut == NULL) {
        ERROR("failed to initial render");
        return NULL;
    }

    return rs;
}

