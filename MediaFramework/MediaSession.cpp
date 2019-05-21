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
        timecode            = packet->pts;
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
    
    virtual void onEvent(const sp<FrameReadyEvent>& event) {
        mPacketRequestEvent->fire(new OnPacketReadyTunnel(event));
    }
};

struct IMediaSessionInt : SharedObject {
    virtual void onInit(const sp<Message>& format, const sp<Message>& options) = 0;
    virtual void onPrepare() { }
    virtual void onFlush() { }
    virtual void onRelease() { ReleaseObject(); Looper::Current()->terminate(); }
};

struct InitRunnable : public Runnable {
    sp<Message> mFormats;
    sp<Message> mOptions;
    InitRunnable(const sp<Message>& format, const sp<Message>& options) : mFormats(format), mOptions(options) { }
    
    virtual void run() {
        sp<IMediaSessionInt> session = Looper::Current()->user(0);
        session->onInit(mFormats, mOptions);
    }
};

struct PrepareRunnable : public Runnable {
    virtual void run() {
        sp<IMediaSessionInt> session = Looper::Current()->user(0);
        session->onPrepare();
    }
};

struct FlushRunnable : public Runnable {
    virtual void run() {
        sp<IMediaSessionInt> session = Looper::Current()->user(0);
        session->onFlush();
    }
};

struct ReleaseRunnable : public Runnable {
    virtual void run() {
        sp<IMediaSessionInt> session = Looper::Current()->user(0);
        session->onRelease();
    }
};

struct ThreadSession : public IMediaSession {
    sp<Looper> mLooper;
    
    ThreadSession(const String& name, sp<IMediaSessionInt>& internel) : mLooper(Looper::Create(name)) {
        mLooper->bind(internel->RetainObject()); // remember to release on release
        mLooper->loop();
    }
    
    void init(const sp<Message>& format, const sp<Message>& options) {
        mLooper->post(new InitRunnable(format, options));
    }
    
    virtual void prepare() {
        mLooper->post(new PrepareRunnable);
    }
    
    virtual void flush() {
        mLooper->post(new FlushRunnable);
    }
    
    virtual void release() {
        mLooper->post(new ReleaseRunnable);
    }
};

struct Decoder : public IMediaSessionInt {
    // external static context
    eCodecFormat            mFormat;
    eCodecType              mType;
    sp<PacketRequestEvent>  mPacketRequestEvent;        // where we get packets
    sp<SessionInfoEvent>    mInfoEvent;
    
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
    List<sp<FrameReadyEvent> > mFrameRequests;  // requests queue
    // statistics
    size_t                  mPacketsReceived;
    size_t                  mPacketsComsumed;
    size_t                  mFramesDecoded;

    Decoder() : IMediaSessionInt(),
        // external static context
        mFormat(kCodecFormatUnknown), mType(kCodecTypeUnknown), mPacketRequestEvent(NULL),
        // internal static context
        mCodec(NULL), mPacketReadyEvent(NULL),
        // internal mutable context
        mGeneration(0), mInputEOS(false), mSignalCodecEOS(false), mLastPacketTime(kTimeInvalid),
        // statistics
        mPacketsReceived(0), mPacketsComsumed(0), mFramesDecoded(0)
    { }
    
    void notify(eSessionInfoType info) {
        if (mInfoEvent != NULL) {
            mInfoEvent->fire(info);
        }
    }
    
    void onInit(const sp<Message>& format, const sp<Message>& options) {
        DEBUG("codec %zu: init << %s << %s", mFormat, format->string().c_str(), options->string().c_str());
        CHECK_TRUE(options->contains("PacketRequestEvent"));
        mPacketRequestEvent = options->findObject("PacketRequestEvent");
        
        if (options->contains("SessionInfoEvent")) {
            mInfoEvent = options->findObject("SessionInfoEvent");
        }

        // setup decoder...
        CHECK_TRUE(format->contains(kKeyFormat));
        mFormat = (eCodecFormat)format->findInt32(kKeyFormat);
        mType = GetCodecType(mFormat);
        
        eModeType mode = (eModeType)options->findInt32(kKeyMode, kModeTypeDefault);

        mCodec = MediaDecoder::Create(mFormat, mode);
        if (mCodec.isNIL()) {
            ERROR("codec is not supported");
            notify(kSessionInfoError);
            return;
        }
        
        if (mCodec->init(format, options) != kMediaNoError) {
            ERROR("track %zu: create codec failed", mFormat);
            mCodec.clear();

#if 1
            if (mode != kModeTypeSoftware) {
                sp<Message> soft = options->dup();
                soft->setInt32(kKeyMode, kModeTypeSoftware);
                mCodec = MediaDecoder::Create(mFormat, kModeTypeSoftware);

                if (mCodec->init(format, soft) != kMediaNoError) {
                    ERROR("track %zu: create software codec failed", mFormat);
                    mCodec.clear();
                }
            }
#endif
        }
        
        if (mCodec.isNIL()) {
            ERROR("codec initial failed");
            notify(kSessionInfoError);
            return;
        }
    }

    void requestPacket() {
        if (mInputEOS) return;

        if (mInputQueue.size() >= MAX_COUNT) {
            DEBUG("codec %zu: input queue is full", mFormat);
            return;
        }

        DEBUG("codec %zu: request packet", mFormat);
        mPacketRequestEvent->fire(mPacketReadyEvent);

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
            INFO("codec %zu: ignore outdated packets", mFormat);
            return;
        }

        if (pkt == NULL) {
            INFO("codec %zu: eos detected", mFormat);
            mInputEOS = true;
        } else {
            DEBUG("codec %zu: packet %.3f|%.3f(s) ready",
                    mID, pkt->dts.seconds(), pkt->pts.seconds());
            
            if (mPacketsReceived == 0) {
                INFO("codec %zu: first packet @ %.3f(s)|%.3f(s)", mFormat, pkt->pts.seconds(), pkt->dts.seconds());
            }

            ++mPacketsReceived;
            // @see MediaFile::read(), packets should in dts order.
            if (pkt->dts < mLastPacketTime) {
                WARN("codec %zu: unorderred packet %.3f(s) < last %.3f(s)",
                        mFormat, pkt->dts.seconds(), mLastPacketTime.seconds());
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

    void onPrepare() {
        INFO("codec %zu: prepare decoder", mFormat);

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
        requestPacket();
        // -> onPacketReady
        // MIN_COUNT packets
    }
    
    void onFlush() {
        INFO("codec %zu: flush decoder", mFormat);
        
        ++mGeneration;
        mInputQueue.clear();
        mCodec->flush();
        mFrameRequests.clear();
    }

    void onRequestFrame(const sp<FrameReadyEvent>& event) {
        DEBUG("codec %zu: request frames", mFormat);
        CHECK_TRUE(event != NULL);
    
        // request next frame
        if (mOutputEOS) {
            ERROR("codec %zu: request frame at eos", mFormat);
            return;
        }

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
            requestPacket();
        }
    }

    void onDecode() {
        DEBUG("codec %zu: with %zu packets ready", mFormat, mInputQueue.size());
        CHECK_TRUE(mInputQueue.size() || mInputEOS);
        CHECK_FALSE(mOutputEOS);

        // push packets to codec
        if (mInputQueue.size()) {
            sp<MediaPacket> packet = *mInputQueue.begin();
            CHECK_TRUE(packet != NULL);

            DEBUG("codec %zu: decode pkt %.3f(s)", mFormat, packet->timecode.seconds());
            MediaError err = mCodec->write(packet);
            if (err == kMediaErrorResourceBusy) {
                DEBUG("codec %zu: codec is full with frames", mFormat);
            } else {
                if (err != kMediaNoError) {
                    ERROR("codec %zu: write packet failed.", mFormat);
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
            WARN("codec %zu: is initializing...", mFormat);
        } else {
            sp<FrameReadyEvent>& event = mFrameRequests.front();

            if (frame != NULL) {
                DEBUG("codec %zu: decoded frame %.3f(s) ready", mFormat, frame->timecode.seconds());
                ++mFramesDecoded;
            } else {
                INFO("codec %zu: codec eos detected", mFormat);
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
        requestPacket();
    }
};

// request frame from decoder
struct OnFrameRequest : public FrameRequestEvent {
    OnFrameRequest(const sp<ThreadSession>& session) : FrameRequestEvent(session->mLooper) { }
    
    virtual void onEvent(const sp<FrameReadyEvent>& event) {
        sp<Decoder> decoder = Looper::Current()->user(0);
        decoder->onRequestFrame(event);
    }
};

struct Renderer : public IMediaSessionInt {
    enum eRenderState {
        kRenderInitialized,
        kRenderReady,
        kRenderTicking,
        kRenderFlushed,
        kRenderEnd
    };
    
    // external static context
    eCodecFormat            mFormat;
    eCodecType              mType;
    sp<FrameRequestEvent>   mFrameRequestEvent;
    sp<SessionInfoEvent>    mInfoEvent;
    // internal static context
    sp<ThreadSession>       mDecoder;
    sp<FrameReadyEvent>     mFrameReadyEvent;
    sp<MediaFrameEvent>     mMediaFrameEvent;
    sp<MediaOut>            mOut;
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

    Renderer() : IMediaSessionInt(),
        // external static context
        mFormat(kCodecFormatUnknown), mType(kCodecTypeUnknown),
        mFrameRequestEvent(NULL), mInfoEvent(NULL),
        // internal static context
        mFrameReadyEvent(NULL),
        mOut(NULL), mClock(NULL), mLatency(0),
        // render context
        mGeneration(0),
        mPresentFrame(new PresentRunnable()), mState(kRenderInitialized), mOutputEOS(false),
        mLastFrameTime(kTimeInvalid),
        mLastUpdateTime(kTimeInvalid),
        // statistics
        mFramesRenderred(0) { }
    
    // init MediaOut based on MediaFrame
    void onInitDevice(const sp<MediaFrame>& frame) {
        DEBUG("renderer %zu: init device", mFormat);
        
        if (!mMediaFrameEvent.isNIL()) {
            DEBUG("renderer %zu: MediaFrameEvent...", mFormat);
            return;
        }
        
        // init MediaOut
        if (!mOut.isNIL()) {
            mOut->flush();
            mOut.clear();
        }
        
        mOut = MediaOut::Create(mType);
        sp<Message> format = new Message;
        sp<Message> options = new Message;
        if (mType == kCodecTypeVideo) {
            format->setInt32(kKeyFormat, frame->v.format);
            format->setInt32(kKeyWidth, frame->v.width);
            format->setInt32(kKeyHeight, frame->v.height);
        } else if (mType == kCodecTypeAudio) {
            format->setInt32(kKeyFormat, frame->a.format);
            format->setInt32(kKeyChannels, frame->a.channels);
            format->setInt32(kKeySampleRate, frame->a.freq);
        }
        
        if (mOut->prepare(format, options) != kMediaNoError) {
            ERROR("renderer %zu: create out failed", mFormat);
            notify(kSessionInfoError);
            return;
        }
        
        if (mType == kCodecTypeVideo) {
            // setup color converter
        } else if (mType == kCodecTypeAudio) {
            // setup resampler
            mLatency = mOut->formats()->findInt32(kKeyLatency);
        } else {
            FATAL("FIXME");
        }
    }
    
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
    
    void onInit(const sp<Message>& format, const sp<Message>& options) {
        CHECK_TRUE(format->contains(kKeyFormat));
        mFormat = (eCodecFormat)format->findInt32(kKeyFormat);
        mType = GetCodecType(mFormat);
        
        DEBUG("renderer %zu: init << %s << %s", mFormat, format->string().c_str(), options->string().c_str());
        
        sp<IMediaSessionInt> dec = new Decoder;
        mDecoder = new ThreadSession(String::format("dec.%#x", mFormat), dec);
        mDecoder->init(format, options);
        
        // setup external context
        if (options->contains("SessionInfoEvent")) {
            mInfoEvent = options->findObject("SessionInfoEvent");
        }
        
        if (options->contains("Clock")) {
            mClock = options->findObject("Clock");
            mClock->setListener(new OnClockEvent(Looper::Current()));
        }
        
        if (options->contains("MediaFrameEvent")) {
            mMediaFrameEvent = options->findObject("MediaFrameEvent");
        }
        
        mFrameRequestEvent = new OnFrameRequest(mDecoder);
    }
    
    void notify(eSessionInfoType info) {
        if (mInfoEvent != NULL) {
            mInfoEvent->fire(info);
        }
    }

    void requestFrame() {
        // don't request frame if eos detected.
        if (mOutputEOS) return;

        // output queue: kMaxFrameNum at most
        if (mOutputQueue.size() >= MAX_COUNT) {
            DEBUG("renderer %zu: output queue is full", mFormat);
            return;
        }

        mFrameRequestEvent->fire(mFrameReadyEvent);
        DEBUG("renderer %zu: request more frames", mFormat);
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
        DEBUG("renderer %zu: one frame ready", mFormat);
        if (mGeneration.load() != generation) {
            INFO("renderer %zu: ignore outdated frames", mFormat);
            return;
        }
        
        // TODO: re-init on format changed
        if (ABE_UNLIKELY(mMediaFrameEvent.isNIL() && mOut.isNIL())) {
            onInitDevice(frame);
        }

        // case 1: eos
        if (frame == NULL) {
            INFO("renderer %zu: eos detected", mFormat);
            mOutputEOS = true;
            if (mLastFrameTime == kTimeInvalid) {
                WARN("renderer %zu: eos at start", mFormat);
                notify(kSessionInfoEnd);
            }
            // NOTHING TO DO
            return;
        }

        // check pts, kTimeInvalid < kTimeBegin
        if (frame->timecode < kTimeBegin) {
            ERROR("renderer %zu: bad pts", mFormat);
            // FIXME:
        }

        // queue frame, frames must be pts order
        DEBUG("renderer %zu: %.3f(s)", mFormat, frame->timecode.seconds());
        if (frame->timecode <= mLastFrameTime) {
            WARN("renderer %zu: unordered frame %.3f(s) < last %.3f(s)",
                    mFormat, frame->timecode.seconds(),
                    mLastFrameTime.seconds());
        }

        if (!mClock.isNIL() && frame->timecode.useconds() < mClock->get()) {
            WARN("renderer %zu: frame late %.3f(s) vs current %.3f(s)", mFormat,
                 frame->timecode.seconds(), mClock->get() / 1E6);
        } else {
            mOutputQueue.push(frame);
        }

        // request more frames
        requestFrame();

        // prepare done ?
        if (mState == kRenderInitialized && (mOutputQueue.size() >= MIN_COUNT || mOutputEOS)) {
            INFO("renderer %zu: prepare done", mFormat);
            mState = kRenderReady;
            notify(kSessionInfoReady);
        }

        // always render the first video
        if (mLastFrameTime == kTimeInvalid) {
            sp<MediaFrame> frame = *mOutputQueue.begin();
            INFO("renderer %zu: first frame %.3f(s)", mFormat, frame->timecode.seconds());

            // notify about the first render postion
            notify(kSessionInfoBegin);

            if (GetCodecType(mFormat) == kCodecTypeVideo) {
                if (mMediaFrameEvent != NULL) mMediaFrameEvent->fire(frame);
                else mOut->write(frame);
            }

#if 0
            if (mClock != NULL && mClock->role() == kClockRoleMaster) {
                INFO("renderer %zu: set clock time %.3f(s)", mFormat, frame->timecode.seconds());
                mClock->set(frame->timecode.useconds());
            }
#endif
        }

        // remember last frame pts
        mLastFrameTime = frame->timecode;
    }

    void onPrepare() {
        INFO("renderer %zu: prepare renderer", mFormat);
        
        if (!mDecoder.isNIL()) mDecoder->prepare();
        
        // update generation
        mFrameReadyEvent = new OnFrameReady(++mGeneration);

        // reset flags
        mState = kRenderInitialized;
        mLastUpdateTime = kTimeBegin;
        mOutputEOS = false;
        mLastFrameTime = kTimeInvalid;
        mOutputQueue.clear();

        // request frames
        requestFrame();
        // -> onFrameReady

        // if no clock, start render directly
        if (mClock == NULL && !Looper::Current()->exists(mPresentFrame)) {
            onStartRenderer();
        }
    }

    // just flush resources, leave state as it is, reset state on prepare
    void onFlush() {
        INFO("track %zu: flush %zu frames", mFormat, mOutputQueue.size());
        
        if (!mDecoder.isNIL()) mDecoder->flush();

        // update generation
        ++mGeneration;

        // remove present runnable
        Looper::Current()->remove(mPresentFrame);

        // flush output
        mOutputQueue.clear();
        if (!mMediaFrameEvent.isNIL()) mMediaFrameEvent->fire(NULL);
        else if (!mOut.isNIL()) mOut->flush();

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
            INFO("renderer %zu: clock is paused", mFormat);
        } else if (mOutputQueue.size()) {
            next = renderCurrent();
        } else if (mOutputEOS) {
            INFO("renderre %zu: eos, stop render", mFormat);
            // NOTHING
            return;
        } else {
            ERROR("renderer %zu: codec underrun...", mFormat);
        }

        if (next < 0) { // too early
            //INFO("renderer %zu: overrun by %.3f(s)...", mFormat, - next / 1E6);
            Looper::Current()->post(mPresentFrame, -next);
        } else {
            //INFO("renderer %zu: render next @ %.3f(s)...", mFormat, next / 1E6);
            Looper::Current()->post(mPresentFrame, next);
        }

        requestFrame();

        // -> onRender
    }

    // render current frame
    // return negtive value if render current frame too early
    // return postive value for next frame render time
    FORCE_INLINE int64_t renderCurrent() {
        DEBUG("renderer %zu: render with %zu frame ready", mFormat, mOutputQueue.size());

        sp<MediaFrame> frame = mOutputQueue.front();
        CHECK_TRUE(frame != NULL);

        if (mClock != NULL) {
            // check render time
            if (mClock->role() == kClockRoleSlave) {
                // render too early or late ?
                int64_t late = mClock->get() - frame->timecode.useconds();
                if (late > REFRESH_RATE) {
                    // render late: drop current frame
                    WARN("renderer %zu: render late by %.3f(s)|%.3f(s), drop frame... [%zu]",
                            mFormat, late/1E6, frame->timecode.seconds(), mOutputQueue.size());
                    mOutputQueue.pop();
                    return 0;
                } else if (late < -REFRESH_RATE/2) {
                    // FIXME: sometimes render too early for unknown reason
                    DEBUG("renderer %zu: overrun by %.3f(s)|%.3f(s)...",
                            mFormat, -late/1E6, frame->timecode.seconds());
                    return -late;
                }
            }
        }

        DEBUG("renderer %zu: render frame %.3f(s)", mFormat, frame->timecode.seconds());
        MediaError rt = kMediaNoError;
        if (mOut != NULL) CHECK_TRUE(mOut->write(frame) == kMediaNoError);
        else mMediaFrameEvent->fire(frame);
        mOutputQueue.pop();
        ++mFramesRenderred;

        // setup clock to tick if we are master clock
        // FIXME: if current frame is too big, update clock after write will cause clock advance too far
        if (mClock != NULL && mClock->role() == kClockRoleMaster && !mClock->isTicking()) {
            const int64_t realTime = SystemTimeUs();
            INFO("renderer %zu: update clock %.3f(s)+%.3f(s)", mFormat, frame->timecode.seconds(), realTime / 1E6);
            mClock->update(frame->timecode.useconds() - mLatency, realTime);
        }

        // next frame render time.
        if (mOutputQueue.size()) {
            if (mClock == NULL) {
                // if no clock exists, render next immediately
                return 0;
            }

            sp<MediaFrame> next = mOutputQueue.front();
            int64_t delay = next->timecode.useconds() - mClock->get();
            if (delay < 0)  return 0;
            else            return delay;
        } else if (mOutputEOS) {
            INFO("renderer %zu: eos...", mFormat);
            // tell out device about eos
            if (mOut != NULL) mOut->write(NULL);
            else mMediaFrameEvent->fire(NULL);
            
            notify(kSessionInfoEnd);
        }
        INFO("refresh rate");
        return REFRESH_RATE;
    }

    void onStartRenderer() {
        INFO("renderer %zu: start", mFormat);
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
            ERROR("renderer %zu: start at eos", mFormat);
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
        INFO("renderer %zu: pause at %.3f(s)", mFormat, mClock->get() / 1E6);
        Looper::Current()->remove(mPresentFrame);

        if (mOut != NULL) {
            sp<Message> options = new Message;
            options->setInt32(kKeyPause, 1);
            mOut->configure(options);
        }
    }
};

sp<IMediaSession> IMediaSession::create(const sp<Message>& format, const sp<Message>& options) {
    eCodecFormat codec = (eCodecFormat)format->findInt32(kKeyFormat);
    sp<IMediaSessionInt> internal = new Renderer;
    sp<ThreadSession> session = new ThreadSession(String::format("st.%#x", codec), internal);
    session->init(format, options);
    
    return session;
}
