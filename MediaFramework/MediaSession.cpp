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

#define LOG_TAG "mpx.Track"
//#define LOG_NDEBUG 0
#include <MediaToolkit/Toolkit.h>

#include "MediaSession.h"
#include "opengl/GLVideo.h"
#include "sdl2/SDLAudio.h"

#define MIN_COUNT (4)
#define MAX_COUNT (8)
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

namespace mtdcy {

    // no control session, control by FrameRequestEvent.
    struct DecodeSession : public Looper, public FrameRequestEvent, public PacketReadyEvent {
        // external static context
        eCodecFormat            mID;
        sp<PacketRequestEvent>  mSource;        // where we get packets
        // internal static context
        sp<MediaDecoder>        mCodec;         // reference to codec

        // internal mutable context
        // TODO: clock for decoder, handle late frames
        List<sp<MediaPacket> >  mInputQueue;    // input packets queue
        bool                    mInputEOS;      // end of input ?
        bool                    mSignalCodecEOS;    // tell codec eos
        bool                    mOutputEOS;         // end of output ?
        MediaTime               mLastPacketTime;    // test packets in dts order?
        List<FrameRequestPayload>   mFrameRequests; // requests queue
        // statistics
        size_t                  mPacketsReceived;
        size_t                  mPacketsComsumed;
        size_t                  mFramesDecoded;

        DecodeSession(const String& name, const Message& format, const Message& options) :
            Looper(name),
            FrameRequestEvent(sp_Retain(this)),
            PacketReadyEvent(sp_Retain(this)),    // all event in the same looper
            // external static context
            mID(kCodecFormatUnknown), mSource(NULL),
            // internal static context
            mCodec(NULL),
            // internal mutable context
            mInputEOS(false), mSignalCodecEOS(false), mLastPacketTime(kTimeInvalid),
            // statistics
            mPacketsReceived(0), mPacketsComsumed(0), mFramesDecoded(0)
        {
            loop();     // start looper

            // setup external context
            CHECK_TRUE(options.contains("PacketRequestEvent"));
            mSource = options.find<sp<PacketRequestEvent> >("PacketRequestEvent");

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
            terminate();    // stop looper
        }

        void requestPacket(const MediaTime& ts = kTimeInvalid) {
            if (mInputEOS) return;

            if (mInputQueue.size() >= MAX_COUNT) {
                DEBUG("codec %zu: input queue is full", mID);
                return;
            }

            DEBUG("codec %zu: request packet", mID);
            PacketRequestPayload payload;
            if (ts != kTimeInvalid) {
                INFO("codec %zu: request frame at %.3f(s)", mID, ts.seconds());
                payload.mode = kModeReadClosestSync;
                payload.ts = ts;
            } else {
                payload.mode = kModeReadNext;
                payload.ts = ts;
            }

            payload.event = sp_Retain(this);
            mSource->fire(payload);

            // -> onPacketReady
        }

        void onPacketReady(const sp<MediaPacket>& pkt) {
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
        }

        void onPrepareDecoder(const MediaTime& ts) {
            INFO("codec %zu: prepare decoder", mID);
            CHECK_TRUE(ts != kTimeInvalid);

            // TODO: prepare again without flush

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

            // remove cmds
            mFrameRequests.clear();

            // flush input queue and codec
            mInputQueue.clear();
            mCodec->flush();
        }

        void onRequestFrame(const FrameRequestPayload& request) {
            DEBUG("codec %zu: request frames", mID);

            // request to flush ?
            if (request.ts == kTimeEnd) {
                onFlushDecoder();
                return;
                // NO need to queue the request
            }
            // request to prepare @ ts
            else if (request.ts != kTimeInvalid) {
                onPrepareDecoder(request.ts);
                return;
                // NO need to queue the request
            } else if (mOutputEOS) {
                WARN("codec %zu: request frame at eos", mID);
                // DO NOTHING
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
            FrameRequestPayload& request = *mFrameRequests.begin();
            if (frame == NULL && !mInputEOS) {
                WARN("codec %zu: is initializing...", mID);
            } else {
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
                    --request.number;
                    if (request.number == 0) mFrameRequests.pop();
                }
            }

            // prepare input packet for next frame
            if (!mInputEOS) requestPacket();
        }

        // PacketReadyEvent
        virtual void onEvent(const sp<MediaPacket>& packet) {
            onPacketReady(packet);
        }

        // FrameRequestEvent
        virtual void onEvent(const FrameRequestPayload& payload) {
            onRequestFrame(payload);
        }
    };

    //
    struct RenderSession : public ControlEvent, public FrameReadyEvent {
        using ControlEvent::fire;
        // static context
        eCodecFormat            mID;
        sp<Looper>              mLooper;    // keep a ref, for terminate()
        sp<RenderPositionEvent> mPositionEvent;
        sp<MediaOut>            mOut;
        sp<ColorConvertor>      mColorConvertor;
        sp<Clock>               mClock;
        sp<FrameRequestEvent>   mFrameRequestEvent;
        int64_t                 mLatency;

        // render scope context
        struct PresentRunnable;
        sp<PresentRunnable>     mPresentFrame;      // for present current frame
        List<sp<MediaFrame> >   mOutputQueue;       // output frame queue
        bool                    mOutputEOS;         // output frame eos
        MediaTime               mLastFrameTime;     // kTimeInvalid => first frame
        size_t                  mFrameRequests;     // sent requests
        sp<StatusEvent>         mStatusEvent;
        // clock context
        MediaTime               mLastUpdateTime;    // last clock update time
        // statistics
        size_t                  mFramesRenderred;

        RenderSession(const sp<Looper>& lp,
                eCodecFormat id,
                const Message& format,
                const Message& options) :
            ControlEvent(lp), FrameReadyEvent(lp),  // share the same looper
            // external static context
            mID(id), mLooper(lp),
            mPositionEvent(NULL),
            // internal static context
            mOut(NULL), mColorConvertor(NULL), mClock(NULL),
            mFrameRequestEvent(NULL), mLatency(0),
            // render context
            mPresentFrame(new PresentRunnable(this)),
            mOutputEOS(false),
            mLastFrameTime(kTimeInvalid), mFrameRequests(0),
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

            CHECK_TRUE(options.contains("FrameRequestEvent"));
            mFrameRequestEvent = options.find<sp<FrameRequestEvent> >("FrameRequestEvent");

            // setup out context
            CHECK_TRUE(format.contains(kKeyFormat));

            eCodecType type = GetCodecType(mID);

            if (type == kCodecTypeVideo) {
                ePixelFormat pixel = (ePixelFormat)format.findInt32(kKeyFormat);
                ePixelFormat pixFmtAccepted;
                if (options.contains("MediaOut")) {
                    mOut = options.find<sp<MediaOut> >("MediaOut");
                } else {
                    mOut = new GLVideo();
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
                mClock->setListener(new OnClockEvent(this, mLooper));
            }
        }

        virtual ~RenderSession() {
            // are we share looper with others
            mLooper->terminate();
        }

        void requestFrame() {
            // don't request frame if eos detected.
            if (mOutputEOS) return;

            size_t count = mOutputQueue.size() + mFrameRequests;

            // output queue: kMaxFrameNum at most
            if (count >= MAX_COUNT) {
                DEBUG("renderer %zu: output queue is full", mID);
                return;
            }

            FrameRequestPayload payload;
            payload.ts = kTimeInvalid;
            payload.number = count >= MIN_COUNT ? 1 : MIN_COUNT - count;
            payload.event = sp_Retain(this);
            mFrameRequestEvent->fire(payload);
            DEBUG("renderer %zu: request more frames", mID);

            mFrameRequests += payload.number;

            // -> onFrameReady
        }

        void onFrameReady(const sp<MediaFrame>& frame) {
            DEBUG("renderer %zu: one frame ready", mID);

            --mFrameRequests;

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

        void onPrepareRenderer(const MediaTime& ts, const sp<StatusEvent>& se) {
            INFO("renderer %zu: prepare renderer...", mID);
            CHECK_TRUE(ts >= kTimeBegin);

            // tell decoder to prepare
            FrameRequestPayload payload;
            payload.ts = ts;
            mFrameRequestEvent->fire(payload);

            // reset flags
            mLastUpdateTime = 0;
            mOutputEOS = false;
            mLastFrameTime = kTimeInvalid;
            mFrameRequests = 0;
            mStatusEvent = se;
            mOutputQueue.clear();

            // request MIN_COUNT frames
            requestFrame();
            // -> onFrameReady

            // if no clock, start render directly
            if (mClock == NULL && !mLooper->exists(mPresentFrame)) {
                onStartRenderer();
            }
        }

        void onFlushRenderer() {
            INFO("track %zu: flush %zu frames", mID, mOutputQueue.size());

            // tell decoder to flush
            FrameRequestPayload payload;
            payload.ts = kTimeEnd;
            mFrameRequestEvent->fire(payload);

            // remove present runnable
            mLooper->remove(mPresentFrame);

            // flush output
            mOutputEOS = false;
            mOutputQueue.clear();
            mFrameRequests = 0;
            if (mOut != NULL) mOut->flush();

            // reset flags
            mLastUpdateTime = 0;

            // reset statistics
            mFramesRenderred = 0;
        }

        struct PresentRunnable : public Runnable {
            RenderSession *mTgt;
            PresentRunnable(RenderSession *tgt) : Runnable(), mTgt(tgt) { }
            virtual void run() { mTgt->onRender(); }
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
                requestFrame();
                mLooper->post(mPresentFrame, next);
            }

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
                    && mClock->isPaused()) {
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
            RenderSession *mTgt;
            
            OnClockEvent(RenderSession *tgt, const sp<Looper>& lp) :
            ClockEvent(lp), mTgt(tgt) { }
            
            virtual void onEvent(const eClockState& cs) {
                INFO("clock state => %d", cs);
                switch (cs) {
                    case kClockStateTicking:
                        mTgt->onStartRenderer();
                        break;
                    case kClockStatePaused:
                        mTgt->onPauseRenderer();
                        break;
                    case kClockStateReset:
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

        // FrameReadyEvent
        virtual void onEvent(const sp<MediaFrame>& frame) {
            onFrameReady(frame);
        }

        // ControlEvent
        virtual void onEvent(const ControlEventPayload& ep) {
            switch (ep.type) {
                case kControlEventPrepare:
                    onPrepareRenderer(ep.ts, ep.event);
                    break;
                case kControlEventFlush:
                    onFlushRenderer();
                    break;
                default:
                    FATAL("FIXME");
                    break;
            }
        }
    };

    sp<MediaSession> MediaSessionCreate(const Message& format, const Message& options) {
        CHECK_TRUE(format.contains(kKeyFormat));
        eCodecFormat id = (eCodecFormat)format.findInt32(kKeyFormat);
        eCodecType type = GetCodecType(id);

        bool clock = options.contains("Clock");

        String name = String::format("%d.decode", id);
        sp<DecodeSession> ds = new DecodeSession(name, format, options);

        if (ds->mCodec == NULL) {
            ERROR("failed to initial decoder");
            return NULL;
        }

        // do we need a new looper for renderer ?
        sp<Looper> rlp = ds;
        if (clock) {
            rlp = new Looper(String::format("%d.render", id));
            rlp->loop();
        }

        Message dup = options;
        dup.set<sp<FrameRequestEvent> >("FrameRequestEvent", ds);
        sp<RenderSession> rs = new RenderSession(rlp, id,
                ds->mCodec->formats(), dup);

        // test if RenderSession is valid
        if (rs->mOut == NULL) {
            ERROR("failed to initial render");
            return NULL;
        }

        return rs;
    }
};
