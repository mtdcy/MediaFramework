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


// File:    RenderSession.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20181126     initial version
//

#define LOG_TAG "RenderSession"
//#define LOG_NDEBUG 0
#include "MediaSession.h"
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

Object<IMediaSession> CreateDecodeSession(const Object<Message>& format, const Object<Message>& options);

struct RenderSession : public IMediaSession {
    enum eRenderState {
        kRenderInitialized,
        kRenderReady,
        kRenderTicking,
        kRenderFlushed,
        kRenderEnd
    };

    // external static context
    Object<Message>         mFormat;
    // options
    sp<FrameRequestEvent>   mFrameRequestEvent;     // mandatory
    sp<SessionInfoEvent>    mInfoEvent;

    // internal static context
    String                  mName;  // for Log
    Object<IMediaSession>   mDecoder;
    sp<FrameReadyEvent>     mFrameReadyEvent;
    sp<MediaFrameEvent>     mMediaFrameEvent;
    sp<MediaOut>            mOut;
    sp<Clock>               mClock;
    int64_t                 mLatency;

    // render scope context
    eCodecType              mType;
    Atomic<int>             mGeneration;
    struct PresentJob;
    sp<PresentJob>          mPresentFrame;      // for present current frame
    List<sp<MediaFrame> >   mOutputQueue;       // output frame queue
    eRenderState            mState;
    bool                    mFirstFrame;
    bool                    mOutputEOS;

    MediaTime               mLastFrameTime;     // kTimeInvalid => first frame
    // statistics
    size_t                  mFramesRenderred;

    bool valid() const { return mOut != NULL || mMediaFrameEvent != NULL; }

    RenderSession(const sp<Message>& format, const sp<Message>& options) : IMediaSession(new Looper("renderer")),
    // external static context
    mFormat(format),
    mFrameRequestEvent(NULL), mInfoEvent(NULL),
    // internal static context
    mFrameReadyEvent(NULL),
    mOut(NULL), mClock(NULL), mLatency(0),
    // render context
    mType(kCodecTypeAudio), mGeneration(0),
    mPresentFrame(new PresentJob(this)), mState(kRenderInitialized),
    mFirstFrame(true), mOutputEOS(false),
    mLastFrameTime(kMediaTimeInvalid),
    // statistics
    mFramesRenderred(0) {
        // setup external context
        CHECK_TRUE(options->contains("FrameRequestEvent"));
        mFrameRequestEvent = options->findObject("FrameRequestEvent");
        
        if (options->contains("SessionInfoEvent")) {
            mInfoEvent = options->findObject("SessionInfoEvent");
        }

        if (options->contains("Clock")) {
            mClock = options->findObject("Clock");
        }

        if (options->contains("MediaFrameEvent")) {
            mMediaFrameEvent = options->findObject("MediaFrameEvent");
        }

        CHECK_TRUE(mFormat->contains(kKeyFormat));
        eCodecFormat codec = (eCodecFormat)mFormat->findInt32(kKeyFormat);
        mName = String::format("render-%x", codec);
    }

    void notify(eSessionInfoType info, const sp<Message>& payload) {
        if (mInfoEvent != NULL) {
            mInfoEvent->fire(info, payload);
        }
    }

    void onInit() {
        DEBUG("%s: onInit...", mName.c_str());
        
        // update generation
        mFrameReadyEvent = new OnFrameReady(this, ++mGeneration);
        
        if (!mClock.isNIL()) {
            mClock->setListener(new OnClockEvent(this));
        }
        
        // request frames
        requestFrame(kMediaTimeInvalid);
        // -> onFrameReady
        
        bool delayInit = false;
        if (mFormat->contains(kKeySampleRate) || mFormat->contains(kKeyChannels)) {
            mType = kCodecTypeAudio;
            // delay create out device
            int32_t channels = mFormat->findInt32(kKeyChannels);
            int32_t sampleRate = mFormat->findInt32(kKeySampleRate);
            delayInit = channels == 0 || sampleRate == 0;
        } else if (mFormat->contains(kKeyWidth) || mFormat->contains(kKeyHeight)) {
            mType = kCodecTypeVideo;
            int32_t width = mFormat->findInt32(kKeyWidth);
            int32_t height = mFormat->findInt32(kKeyHeight);
            delayInit = width == 0 || height == 0;
        }
        
        if (delayInit) return;
        
        // if external out device exists
        if (mMediaFrameEvent.isNIL()) {
            mFormat->setInt32(kKeyCodecType, mType);
            sp<Message> options = new Message;
            mOut = MediaOut::Create(mFormat, options);

            if (mOut.isNIL()) {
                ERROR("%s: create out failed", mName.c_str());
                notify(kSessionInfoError, NULL);
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

        // if no clock, start render directly
        if (mClock == NULL && !Looper::Current()->exists(mPresentFrame)) {
            onStartRenderer();
        }
    }

    virtual void onRelease() {
        mDecoder.clear();
        mOut.clear();
    }
    
    // init MediaOut based on MediaFrame
    void onMediaFrameChanged(const sp<MediaFrame>& frame) {
        DEBUG("%s: init device", mName.c_str());

        if (!mMediaFrameEvent.isNIL()) {
            DEBUG("%s: MediaFrameEvent...", mName.c_str());
            return;
        }

        // init MediaOut
        if (!mOut.isNIL()) {
            mOut->flush();
            mOut.clear();
        }

        sp<Message> format = new Message;
        if (mType == kCodecTypeVideo) {
            format->setInt32(kKeyFormat, frame->v.format);
            format->setInt32(kKeyWidth, frame->v.width);
            format->setInt32(kKeyHeight, frame->v.height);
        } else if (mType == kCodecTypeAudio) {
            format->setInt32(kKeyFormat, frame->a.format);
            format->setInt32(kKeyChannels, frame->a.channels);
            format->setInt32(kKeySampleRate, frame->a.freq);
        }
        
        mFormat = format;

        onInit();
    }

    void requestFrame(const MediaTime& time) {
        if (time != kMediaTimeInvalid) {
            INFO("%s: flush renderer @ %.3f", mName.c_str(), time.seconds());
            
            // clear state
            mFirstFrame     = true;
            mLastFrameTime  = kMediaTimeInvalid;
            mOutputEOS      = false;
            mOutputQueue.clear();
            
            // update generation
            mFrameReadyEvent = new OnFrameReady(this, ++mGeneration);

            // flush output
            if (!mMediaFrameEvent.isNIL()) mMediaFrameEvent->fire(NULL);
            else if (!mOut.isNIL()) mOut->flush();

            mState = kRenderFlushed;
        }
        
        // don't request frame if eos detected.
        if (mOutputEOS) return;

        // output queue: kMaxFrameNum at most
        if (mOutputQueue.size() >= MAX_COUNT) {
            DEBUG("%s: output queue is full", mName.c_str());
            return;
        }

        mFrameRequestEvent->fire(mFrameReadyEvent, time);
        DEBUG("%s: request more frames", mName.c_str());
        // -> onFrameReady
    }

    struct OnFrameReady : public FrameReadyEvent {
        RenderSession *thiz;
        const int mGeneration;
        OnFrameReady(RenderSession *s, int gen) :
            FrameReadyEvent(Looper::Current()), thiz(s), mGeneration(gen) { }

        virtual void onEvent(const sp<MediaFrame>& frame) {
            thiz->onFrameReady(frame, mGeneration);
        }
    };

    void onFrameReady(const sp<MediaFrame>& frame, int generation) {
        DEBUG("%s: one frame ready", mName.c_str());
        if (mGeneration.load() != generation) {
            INFO("%s: ignore outdated frames", mName.c_str());
            return;
        }

        // TODO: re-init on format changed
        if (ABE_UNLIKELY(mMediaFrameEvent.isNIL() && mOut.isNIL())) {
            onMediaFrameChanged(frame);
        }

        // case 1: eos
        if (frame == NULL) {
            INFO("%s: eos detected", mName.c_str());
            mOutputEOS = true;
            if (mLastFrameTime == kMediaTimeInvalid) {
                WARN("%s: eos at start", mName.c_str());
                notify(kSessionInfoEnd, NULL);
            }
            // NOTHING TO DO
            return;
        }
        
        // if no clock exists. write frames directly
        if (mClock.isNIL()) {
            writeFrame(frame);
            return;
        }

        // check pts, kTimeInvalid < kTimeBegin
        if (frame->timecode < kMediaTimeBegin) {
            ERROR("%s: bad pts", mName.c_str());
            // FIXME:
        }
        
        if (mLastFrameTime == kMediaTimeInvalid) {
            INFO("%s: first frame %.3f(s)", mName.c_str(), frame->timecode.seconds());

            // notify about the first render postion
            notify(kSessionInfoBegin, NULL);
        }
        
        // always render the first video
        if (mLastFrameTime == kMediaTimeInvalid && mType == kCodecTypeVideo) {
            writeFrame(frame);
        } else {
            // queue frame, frames must be pts order
            DEBUG("%s: %.3f(s)", mName.c_str(), frame->timecode.seconds());
            if (frame->timecode <= mLastFrameTime) {
                WARN("%s: unordered frame %.3f(s) < last %.3f(s)",
                        mName.c_str(), frame->timecode.seconds(),
                        mLastFrameTime.seconds());
            }
            mOutputQueue.push(frame);
        }

        // request more frames
        requestFrame(kMediaTimeInvalid);

        // prepare done ?
        if (mState == kRenderInitialized && (mOutputQueue.size() >= MIN_COUNT || mOutputEOS)) {
            INFO("%s: prepare done", mName.c_str());
            mState = kRenderReady;
            notify(kSessionInfoReady, NULL);
        }

        // remember last frame pts
        mLastFrameTime = frame->timecode;
    }

    struct PresentJob : public Job {
        RenderSession *thiz;
        PresentJob(RenderSession *s) : Job(), thiz(s) { }
        virtual void onJob() {
            thiz->onRender();
        }
    };

    void onRender() {
        CHECK_FALSE(mClock.isNIL());
    
        // always render the first video frame
        if (mClock->isPaused() || mOutputEOS) {
            return;
        }
        
        int64_t next = REFRESH_RATE;
        if (mOutputQueue.size()) {
            next = renderCurrent();
            CHECK_GE(next, 0);
        } else {
            WARN("%s: underrun happens, please check codec ...", mName.c_str());
        }
        
        Looper::Current()->post(mPresentFrame, next);

        requestFrame(kMediaTimeInvalid);

        // -> onRender
    }
    
    FORCE_INLINE void writeFrame(const sp<MediaFrame>& frame) {
        if (mOut != NULL) CHECK_TRUE(mOut->write(frame) == kMediaNoError);
        else mMediaFrameEvent->fire(frame);
    }
    
    FORCE_INLINE int64_t renderAudio() {
        sp<MediaFrame> frame = mOutputQueue.front();
        CHECK_TRUE(frame != NULL);
        mOutputQueue.pop();
        
        writeFrame(frame);
        ++mFramesRenderred;
        
        return 0;
    }
    
    FORCE_INLINE int64_t renderVideo() {
        DEBUG("%s: render with %zu frame ready", mName.c_str(), mOutputQueue.size());

        sp<MediaFrame> frame = mOutputQueue.front();
        CHECK_TRUE(frame != NULL);

        if (!mFirstFrame) {
            int64_t early = frame->timecode.useconds() - mClock->get();
            
            if (early > REFRESH_RATE) {  // 5ms
                DEBUG("%s: overrun by %.3f(s)|%.3f(s)...", mName.c_str(),
                      early/1E6, frame->timecode.seconds());
                return early;
            } else if (-early > REFRESH_RATE) {
                WARN("%s: render late by %.3f(s)|%.3f(s), drop frame...", mName.c_str(),
                     -early/1E6, frame->timecode.seconds());
                mOutputQueue.pop();
                return 0;
            }
        }

        DEBUG("%s: render frame %.3f(s)", mName.c_str(), frame->timecode.seconds());
        writeFrame(frame);
        mOutputQueue.pop();
        ++mFramesRenderred;

        // setup clock to tick if we are master clock
        // FIXME: if current frame is too big, update clock after write will cause clock advance too far
        if (mFirstFrame) {
            if (mClock != NULL && mClock->role() == kClockRoleMaster) {
                mClock->update(frame->timecode.useconds() - mLatency);
                INFO("%s: update clock %.3f(s) (%.3f)", mName.c_str(),
                     frame->timecode.seconds(), mClock->get() / 1E6);
            }
            mFirstFrame = false;
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
            INFO("%s: eos...", mName.c_str());
            // tell out device about eos
            if (mOut != NULL) mOut->write(NULL);
            else mMediaFrameEvent->fire(NULL);

            notify(kSessionInfoEnd, NULL);
        }
        
        INFO("refresh rate");
        return REFRESH_RATE;
    }

    // render current frame
    // return next frame render time
    FORCE_INLINE int64_t renderCurrent() {
        if (mType == kCodecTypeVideo) {
            return renderVideo();
        } else {
            return renderAudio();
        }
    }

    // using clock to control render session, start|pause|...
    struct OnClockEvent : public ClockEvent {
        RenderSession *thiz;
        OnClockEvent(RenderSession *session) : ClockEvent(Looper::Current()), thiz(session) { }

        virtual void onEvent(const eClockState& cs) {
            INFO("clock state => %d", cs);
            switch (cs) {
                case kClockStateTicking:
                    thiz->onStartRenderer();
                    break;
                case kClockStatePaused:
                    thiz->onPauseRenderer();
                    break;
                case kClockStateTimeChanged:
                    thiz->onPrepareRenderer();
                    break;
                default:
                    break;
            }
        }
    };

    void onStartRenderer() {
        INFO("%s: start", mName.c_str());
        //onPrintStat();
        mFirstFrame = true;

        // check
        if (Looper::Current()->exists(mPresentFrame)) {
            ERROR("%s: already started", mName.c_str());
            return;
        }

        // start
        // case 1: start at eos
        if (mOutputEOS && mOutputQueue.empty()) {
            // eos, do nothing
            ERROR("%s: start at eos", mName.c_str());
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
        INFO("%s: pause at %.3f(s)", mName.c_str(), mClock->get() / 1E6);
        Looper::Current()->remove(mPresentFrame);

        if (mOut != NULL) {
            sp<Message> options = new Message;
            options->setInt32(kKeyPause, 1);
            mOut->configure(options);
        }
    }
    
    void onPrepareRenderer() {
        const MediaTime pos = MediaTime(mClock->get());
        INFO("%s: prepare render @ %.3f", mName.c_str(), pos.seconds());
        requestFrame(pos);
    }
};

Object<IMediaSession> CreateRenderSession(const Object<Message>& format, const Object<Message>& options) {
    return new RenderSession(format, options);
}
