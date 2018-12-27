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

#define MIN_COUNT (4)
#define MAX_COUNT (8)
#define REFRESH_RATE (10000LL)    // 10ms

namespace mtdcy {
    enum eEventType {
        // public event type, non timed event.
        kEventTypePrepare,
        kEventTypeStart,
        kEventTypeStop,
        kEventTypeFlush,
        
        // private event type
        kEventTypeRender,   // timed event
    };
    struct EventPayload {
        eEventType      type;
        MediaTime       ts;
        sp<StatusEvent> se;
    };
    typedef Event<EventPayload> SessionEvent;
    
    struct  MediaSession::DecodeSession : public SessionEvent, public RequestFrameEvent, public PacketReadyEvent {
        using SessionEvent::fire;
        // external static context
        sp<PacketRequestEvent>  mSource;
        eCodecFormat            mID;
        // internal static context
        sp<MediaDecoder>        mCodec;
        
        // decoder scope context
        // TODO: clock for decoder, handle late frames
        List<sp<MediaPacket> >  mInputQueue;
        bool                    mInputEOS;
        bool                    mFirstPacket;
        MediaTime               mLastPacketTime;    // test packets in dts order?
        List<FrameRequestPayload>   mFrameRequests;
        // statistics
        size_t                  mPacketsReceived;
        size_t                  mPacketsComsumed;
        size_t                  mFramesDecoded;
        
        DecodeSession(const sp<Looper>& lp, const Message& format, const Message& options) :
        SessionEvent(lp), RequestFrameEvent(lp), PacketReadyEvent(lp),
        mSource(NULL), mID(kCodecFormatUnknown),
        mCodec(NULL),
        mInputEOS(false),
        mFirstPacket(true),
        mLastPacketTime(kTimeInvalid),
        // statistics
        mPacketsReceived(0), mPacketsComsumed(0),
        mFramesDecoded(0)
        {
            // setup decoder...
            
            CHECK_TRUE(options.contains("PacketRequestEvent"));
            mSource = options.find<sp<PacketRequestEvent> >("PacketRequestEvent");
            
            CHECK_TRUE(format.contains(kKeyFormat));
            mID = (eCodecFormat)format.findInt32(kKeyFormat);
            
            eCodecType type = GetCodecType(mID);
            
            mCodec = MediaDecoder::Create(format);
            if (mCodec == NULL || mCodec->status() != OK) {
                ERROR("track %zu: create codec failed", mID);
                return;
            }
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
                INFO("codec %zu: request frame at %.3f(s)", ts.seconds());
                payload.mode = kModeReadClosestSync;
                payload.ts = ts;
                mFirstPacket = false;
            } else {
                payload.mode = kModeReadNext;
                payload.ts = ts;
            }
            
            payload.event = sp_Retain_this(this);
            mSource->fire(payload);
        }
        
        void onPacketReady(const sp<MediaPacket>& pkt) {
            DEBUG("codec %zu: packet %.3f(s) ready", mID, pkt->dts.seconds());
            if (pkt == NULL) {
                INFO("codec %zu: eos detected", mID);
                mInputEOS = true;
            } else {
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
            if (mFrameRequests.size() && (mInputQueue.size() || mInputEOS)) {
                onDecode();
            }
        }
        
        void onPrepareDecoder(const MediaTime& ts) {
            INFO("codec %zu: prepare decoder", mID);
            
            // TODO: prepare again without flush
            
            mFirstPacket = true;
            mInputEOS = false;
            mInputQueue.clear();
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
        
        void onRequestFrame(FrameRequestPayload request) {
            DEBUG("codec %zu: request frames", mID);
            // TODO: handle ts in request
            
            // case 1: input eos && no packets in queue
            // push eos frame to renderer
            if (mInputEOS && mInputQueue.empty()) {
                request.event->fire(NULL);
                return;
                // NO need to queue the request
            }
            
            mFrameRequests.push(request);
            // case 2: packet is ready
            // decode the first packet
            if (mInputQueue.size()) {
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
            
            // push packets to codec
            if (mInputQueue.size()) {
                sp<MediaPacket> packet = *mInputQueue.begin();
                CHECK_TRUE(packet != NULL);
                
                status_t st = mCodec->write(packet);
                if (st == TRY_AGAIN) {
                    DEBUG("codec %zu: codec is full with frames", mID);
                } else {
                    if (st != OK) {
                        ERROR("codec %zu: write packet failed.", mID);
                    }
                    mInputQueue.pop();
                    ++mPacketsComsumed;
                }
            } else {
                CHECK_TRUE(mInputEOS);
                // tell codec about eos
                mCodec->write(NULL);
            }
            
            // drain from codec
            sp<MediaFrame> frame = mCodec->read();
            
            FrameRequestPayload& request = *mFrameRequests.begin();
            if (frame != NULL || mInputEOS) {
                if (frame != NULL) ++mFramesDecoded;
                request.event->fire(frame);
                --request.number;
                if (request.number == 0) mFrameRequests.pop();
            } else {
                WARN("codec %zu: is initializing...", mID);
            }
            
            // prepare input packet for next frame
            requestPacket();
        }
        
        // PacketReadyEvent
        virtual void onEvent(const sp<MediaPacket>& packet) {
            onPacketReady(packet);
        }
        
        // RequestFrameEvent
        virtual void onEvent(const FrameRequestPayload& payload) {
            onRequestFrame(payload);
        }
        
        // SessionEvent
        virtual void onEvent(const EventPayload& ep) {
            switch (ep.type) {
                case kEventTypePrepare:
                    onPrepareDecoder(ep.ts);
                    break;
                case kEventTypeFlush:
                    onFlushDecoder();
                    break;
                default:
                    FATAL("FIXME");
                    break;
            }
        }
    };
    
    struct MediaSession::RenderSession : public SessionEvent, public FrameReadyEvent {
        using SessionEvent::fire;
        // static context
        eCodecFormat            mID;
        sp<RenderPositionEvent> mPositionEvent;
        sp<RenderEvent>         mExternalRenderer;
        sp<MediaOut>            mOut;
        sp<ColorConvertor>      mColorConvertor;
        sp<Clock>               mClock;
        sp<RequestFrameEvent>   mRequestFrameEvent;
        int64_t                 mLatency;

        // render scope context
        List<sp<MediaFrame> >   mOutputQueue;
        bool                    mRendering;
        bool                    mOutputEOS;
        MediaTime               mLastFrameTime; // kTimeInvalid => first frame
        size_t                  mFrameRequests;
        // clock context
        MediaTime               mLastUpdateTime;
        // statistics
        size_t                  mFramesRenderred;
        
        RenderSession(const sp<Looper>& lp,
                      eCodecFormat id,
                      const Message& format,
                      const Message& options) :
        SessionEvent(lp), FrameReadyEvent(lp),
        // external static context
        mID(id), mPositionEvent(NULL), mExternalRenderer(NULL),
        // internal static context
        mOut(NULL), mColorConvertor(NULL), mClock(NULL), mRequestFrameEvent(NULL),
        mLatency(0),
        // render context
        mRendering(false), mOutputEOS(false),
        mLastFrameTime(kTimeInvalid), mFrameRequests(0),
        mLastUpdateTime(kTimeInvalid),
        // statistics
        mFramesRenderred(0)
        {
            if (options.contains("RenderPositionEvent")) {
                mPositionEvent = options.find<sp<RenderPositionEvent> >("RenderPositionEvent");
            }
            
            if (options.contains("Clock")) {
                mClock = options.find<sp<Clock> >("Clock");
            }
            
            CHECK_TRUE(options.contains("RequestFrameEvent"));
            mRequestFrameEvent = options.find<sp<RequestFrameEvent> >("RequestFrameEvent");
            
            CHECK_TRUE(format.contains(kKeyFormat));
            
            eCodecType type = GetCodecType(mID);
            
            if (type == kCodecTypeVideo) {
                ePixelFormat pixel = (ePixelFormat)format.findInt32(kKeyFormat);
                ePixelFormat pixFmtAccepted = kPixelFormatYUV420P;
                if (options.contains("RenderEvent")) {
                    INFO("track %zu: using external renderer...", mID);
                    mExternalRenderer = options.find<sp<RenderEvent> >("RenderEvent");
                    // FIXME: accpet pix fmt from client.
                } else {
                    Message dup = format;
                    CHECK_TRUE(options.contains("SDL_Window"));
                    dup.setPointer("SDL_Window", options.findPointer("SDL_Window"));
                    mOut = MediaOut::Create(kCodecTypeVideo, dup);
                    if (mOut == NULL || mOut->status() != OK) {
                        ERROR("track %zu: create out failed", mID);
                        return;
                    }
                    pixFmtAccepted = (ePixelFormat)mOut->formats().findInt32(kKeyFormat);// color convert
                    if (pixFmtAccepted != pixel) {
                        mColorConvertor = new ColorConvertor(pixFmtAccepted);
                    }
                }
                
            } else if (type == kCodecTypeAudio) {
                mOut = MediaOut::Create(kCodecTypeAudio, format);
                if (mOut == NULL || mOut->status() != OK) {
                    ERROR("track %zu: create out failed", mID);
                    return;
                }
                
                Message _format = mOut->formats();
                mLatency = _format.findInt32(kKeyLatency);
            } else {
                FATAL("FIXME");
            }
            
            if (mClock != NULL) {
                mClock->setListener(new OnClockEvent(this, lp));
            }
        }
        
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
            payload.event = sp_Retain_this(this);
            mRequestFrameEvent->fire(payload);
            DEBUG("renderer %zu: request more frames", mID);
            
            mFrameRequests += payload.number;
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
            
            // always render the first video
            if (mLastFrameTime == kTimeInvalid) {
                sp<MediaFrame> frame = *mOutputQueue.begin();
                if (GetCodecType(mID) == kCodecTypeVideo) {
                    INFO("renderer %zu: first frame %.3f(s)",
                         mID, frame->pts.seconds());
                    if (mExternalRenderer != NULL) {
                        mExternalRenderer->fire(frame);
                    } else {
                        mOut->write(frame);
                    }
                }
                
                // notify about the first render postion
                if (mPositionEvent != NULL) {
                    mPositionEvent->fire(frame->pts);
                }
            }
            
            // remember last frame pts
            mLastFrameTime = frame->pts;
        }
        
        void onPrepareRenderer(const MediaTime& ts) {
            INFO("renderer %zu: prepare renderer...", mID);
            // TODO: prepare again without flush
            
            // reset flags
            mLastUpdateTime = 0;
            mOutputEOS = false;
            mLastFrameTime = kTimeInvalid;
            mFrameRequests = 0;
            
            // request MIN_COUNT frames
            // TODO: handle ts here
            requestFrame();
            // -> onFrameReady
        }
        
        void onStartRenderer() {
            INFO("renderer %zu: start", mID);
            //onPrintStat();
            
            // check
            if (mRendering) {
                ERROR("renderer %zu: already started");
                return;
            }
            
            // set flags
            mRendering = true;
            
            // case 1: start at eos
            if (mOutputEOS && mOutputQueue.empty()) {
                // eos, do nothing
                ERROR("renderer %zu: start at eos", mID);
                mRendering = false;
            }
            // case 3: frames ready
            else {
                onRender();
            }
        }
        
        void onPauseRenderer() {
            INFO("renderer %zu: pause at %.3f(s)", mID, mClock->get().seconds());
            mRendering = false;
        }
        
        void onFlushRenderer() {
            INFO("track %zu: flush %zu frames", mID, mOutputQueue.size());
            
            // flush output
            mRendering = false;
            mOutputEOS = false;
            mOutputQueue.clear();
            mFrameRequests = 0;
            if (mOut != NULL) mOut->flush();
            
            // reset flags
            mLastUpdateTime = 0;
            
            // reset statistics
            mFramesRenderred = 0;
        }
        
        void onRender() {
            if (!mRendering) return;
            
            int64_t next = REFRESH_RATE;
            
            if (mClock != NULL && mClock->isPaused() && mClock->role() == kClockRoleSlave) {
                INFO("renderer %zu: clock is paused", mID);
            } else if (mOutputQueue.size()) {
                next = renderCurrent();
            } else {
                ERROR("renderer %zu: codec underrun...");
            }
            
            EventPayload ep = { kEventTypeRender, kTimeInvalid };
            if (next < 0) { // too early
                fire(ep, -next);
            } else {
                requestFrame();
                fire(ep, next);
            }
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
            status_t rt = OK;
            if (mExternalRenderer != NULL) {
                mExternalRenderer->fire(frame);
            } else {
                rt = mOut->write(frame);
            }
            
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
                if (mExternalRenderer != NULL) mExternalRenderer->fire(NULL);
                else mOut->write(NULL);
                
                if (mPositionEvent != NULL) {
                    mPositionEvent->fire(kTimeEnd);
                }
            } else if (mClock != NULL) {
                WARN("renderer %zu: codec slightly underrun...", mID);
            }
            return REFRESH_RATE;
        }
        
        // FrameReadyEvent
        virtual void onEvent(const sp<MediaFrame>& frame) {
            onFrameReady(frame);
        }
        
        // SessionEvent
        virtual void onEvent(const EventPayload& ep) {
            switch (ep.type) {
                case kEventTypePrepare:
                    onPrepareRenderer(ep.ts);
                    break;
                case kEventTypeStart:
                    onStartRenderer();
                    break;
                case kEventTypeStop:
                    onPauseRenderer();
                    break;
                case kEventTypeFlush:
                    onFlushRenderer();
                    break;
                case kEventTypeRender:
                    onRender();
                    break;
                default:
                    FATAL("FIXME");
                    break;
            }
        }
    };

    ////////////////////////////////////////////////////////////////////////////////
    sp<MediaSession> MediaSession::Create(const Message& format, const Message& options) {
        CHECK_TRUE(format.contains(kKeyFormat));
        eCodecFormat id = (eCodecFormat)format.findInt32(kKeyFormat);
        eCodecType type = GetCodecType(id);
        
        bool clock = options.contains("Clock");
        
        String name = String::format("%d", id);
        
        sp<Looper> lp1 = new Looper(name + "_decode");
        lp1->loop();
        sp<DecodeSession> ds = new DecodeSession(lp1, format, options);
        
        if (ds->mCodec == NULL) {
            ERROR("failed to initial decoder");
            return NULL;
        }
        
        sp<Looper> lp2 = lp1;
        if (clock) {
            lp2 = new Looper(name + "_render");
            lp2->loop();
        }
        
        Message dup = options;
        dup.set<sp<RequestFrameEvent> >("RequestFrameEvent", ds);
        sp<RenderSession> rs = new RenderSession(lp2, id,
                                           ds->mCodec->formats(),
                                           dup);
        
        // test if RenderSession if valid
        if (rs->mOut == NULL && rs->mExternalRenderer == NULL) {
            ERROR("failed to initial render");
            return NULL;
        }
        
        sp<MediaSession> session = new MediaSession;
        session->mID = id;
        session->mDecodeLooper = lp1;
        session->mRenderLooper = lp2;
        session->mDecodeSession = ds;
        session->mRenderSession = rs;
        
        return session;
    }

    MediaSession::~MediaSession() {
        DEBUG("track %zu: destroy...", mID);
        if (mRenderLooper != NULL && mRenderLooper != mDecodeLooper) {
            mRenderLooper->terminate();
        }
        
        if (mDecodeLooper != NULL) {
            mDecodeLooper->terminate();
        }
        mDecodeLooper.clear();
        mRenderLooper.clear();
        mDecodeSession.clear();
        mRenderSession.clear();
    }
    
    struct MultiStatusEvent : public StatusEvent {
        volatile int mCount;
        sp<StatusEvent> mStatusEvent;
        MultiStatusEvent(size_t n, const sp<StatusEvent>& se) :
        StatusEvent(), mCount(n), mStatusEvent(se) { }
        
        virtual void onEvent(const status_t& st) {
            int old = atomic_sub(&mCount, 1);
            CHECK_GE(old, 1);
            if (st != OK) {
                mStatusEvent->fire(st);
            } else if (old == 1) {
                mStatusEvent->fire(st);
            }
        }
    };

    status_t MediaSession::prepare(const MediaTime& ts, const sp<StatusEvent>& se) {
        INFO("track %zu: prepare...", mID);

        // prepare both decoder and renderer
        EventPayload ep = { kEventTypePrepare, ts, new MultiStatusEvent(2, se) };
        mDecodeSession->fire(ep);
        mRenderSession->fire(ep);
        return OK;
    }
    
    status_t MediaSession::flush(const sp<StatusEvent>& se) {
        INFO("track %zu: flush...", mID);
        // issue flush cmd both for decoder and renderer
        EventPayload ep = { kEventTypeFlush, kTimeInvalid, new MultiStatusEvent(2, se) };
        mDecodeSession->fire(ep);
        mRenderSession->fire(ep);
        return OK;
    }
    
}
