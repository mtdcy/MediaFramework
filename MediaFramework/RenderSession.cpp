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
#include "AudioConverter.h"

// TODO: calc count based on duration
// max count: 1s
// min count: 500ms
#define MIN_COUNT (4)
#define MAX_COUNT (8)
#define MIN_LENGTH  200000LL    // 200ms
#define MAX_LENGTH  500000LL    // 500ms

// default refresh rate
// video: 120 fps => 8.3ms / frame
// audio: normal frame duration => 10ms or 20ms
#define DEFAULT_REFRESH_RATE (10000LL)    // 5ms

// the jitter time allowed for render()
// short jitter time means smooth frame presentation,
// but it will cost more cpu times.
#define JITTER_TIME 5000LL      // [-5ms, 5ms]

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

__BEGIN_NAMESPACE_MPX

sp<IMediaSession> CreateDecodeSession(const sp<Message>& format, const sp<Message>& options);

struct RenderSession : public IMediaSession {
    enum eRenderState {
        kRenderInitialized,
        kRenderReady,
        kRenderTicking,
        kRenderFlushed,
        kRenderEnd
    };

    // external static context
    // options
    sp<FrameRequestEvent>   mFrameRequestEvent;     // mandatory
    sp<SessionInfoEvent>    mInfoEvent;

    // internal static context
    String                  mName;  // for Log
    sp<FrameReadyEvent>     mFrameReadyEvent;
    sp<MediaFrameEvent>     mMediaFrameEvent;
    sp<MediaOut>            mOut;
    sp<Clock>               mClock;
    int64_t                 mLatency;

    // render scope context
    eCodecType              mType;
    Atomic<int>             mGeneration;
    struct RenderJob;
    sp<RenderJob>           mRenderJob;      // for present current frame
    List<sp<MediaFrame> >   mOutputQueue;       // output frame queue
    eRenderState            mState;
    bool                    mClockUpdated;
    bool                    mInputEOS;

    MediaTime               mLastFrameTime;     // kTimeInvalid => first frame
    
    // converter
    union {
        int32_t             mFormat;
        AudioFormat         mAudio;
        ImageFormat         mImage;
    };
    sp<AudioConverter>      mAudioConverter;
    
    // statistics
    size_t                  mFramesRenderred;

    bool valid() const { return mOut != NULL || mMediaFrameEvent != NULL; }

    RenderSession(const sp<Looper>& lp) : IMediaSession(lp),
    // external static context
    mFrameRequestEvent(NULL), mInfoEvent(NULL),
    // internal static context
    mFrameReadyEvent(NULL),
    mOut(NULL), mClock(NULL), mLatency(0),
    // render context
    mType(kCodecTypeAudio), mGeneration(0),
    mRenderJob(new RenderJob(this)), mState(kRenderInitialized),
    mClockUpdated(false), mInputEOS(false),
    mLastFrameTime(kMediaTimeInvalid),
    // statistics
    mFramesRenderred(0) {
    }

    void notify(eSessionInfoType info, const sp<Message>& payload) {
        if (mInfoEvent != NULL) {
            mInfoEvent->fire(info, payload);
        }
    }

    void onInit(const sp<Message>& formats, const sp<Message>& options) {
        // setup external context
        if (!options.isNIL()) {
            CHECK_TRUE(options->contains(kKeyFrameRequestEvent));
            mFrameRequestEvent = options->findObject(kKeyFrameRequestEvent);
            
            if (options->contains(kKeySessionInfoEvent)) {
                mInfoEvent = options->findObject(kKeySessionInfoEvent);
            }

            if (options->contains(kKeyClock)) {
                mClock = options->findObject(kKeyClock);
            }

            if (options->contains(kKeyFrameReadyEvent)) {
                mMediaFrameEvent = options->findObject(kKeyFrameReadyEvent);
            }
        }

        CHECK_TRUE(formats->contains(kKeyFormat));
        mFormat = formats->findInt32(kKeyFormat);
        mName = String::format("render-%.4s", (char*)&mFormat);
        
        // update generation
        mFrameReadyEvent = new OnFrameReady(this, ++mGeneration);
        
        if (!mClock.isNIL()) {
            mClock->setListener(new OnClockEvent(this));
        }
        
        bool delayInit = false;
        if (formats->contains(kKeySampleRate) || formats->contains(kKeyChannels)) {
            mType = kCodecTypeAudio;
            // delay create out device
            mAudio.channels = formats->findInt32(kKeyChannels);
            mAudio.freq = formats->findInt32(kKeySampleRate);
            delayInit = mAudio.channels == 0 || mAudio.freq == 0;
        } else if (formats->contains(kKeyWidth) || formats->contains(kKeyHeight)) {
            mType = kCodecTypeVideo;
            int32_t width = formats->findInt32(kKeyWidth);
            int32_t height = formats->findInt32(kKeyHeight);
            delayInit = width == 0 || height == 0;
        }
        
        if (delayInit) return;
        
        // if external out device exists
        if (mMediaFrameEvent.isNIL()) {
            sp<Message> outFormat = formats->dup();
            outFormat->setInt32(kKeyType, mType);
            mOut = MediaOut::Create(outFormat, options);

            if (mOut.isNIL()) {
                ERROR("%s: create out failed", mName.c_str());
                notify(kSessionInfoError, NULL);
                return;
            }

            if (mType == kCodecTypeVideo) {
                // setup color converter
            } else if (mType == kCodecTypeAudio) {
                // setup resampler
                sp<Message> outFormat = mOut->formats();
                mLatency = outFormat->findInt32(kKeyLatency, 0);
                AudioFormat audio;
                audio.format = (eSampleFormat)outFormat->findInt32(kKeyFormat);
                audio.channels = outFormat->findInt32(kKeyChannels);
                audio.freq = outFormat->findInt32(kKeySampleRate);
                
                if (audio.format != mAudio.format ||
                    audio.channels != mAudio.channels ||
                    audio.freq != mAudio.freq) {
                    mAudioConverter = AudioConverter::Create(mAudio, audio, NULL);
                }
            } else {
                FATAL("FIXME");
            }
        }
        
        // request frames
        requestFrame(kMediaTimeInvalid);
        // -> onFrameReady

        // if no clock, start render directly
        if (mClock == NULL && !mDispatch->exists(mRenderJob)) {
            onStartRenderer();
        }
    }

    virtual void onRelease() {
        mDispatch->flush();
        if (!mOut.isNIL()) {
            mOut->flush();
            mOut.clear();
        }
        mFrameReadyEvent.clear();
        mFrameRequestEvent.clear();
        mMediaFrameEvent.clear();
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

        onInit(format, NULL);
    }

    void requestFrame(const MediaTime& time) {
        if (time != kMediaTimeInvalid) {
            INFO("%s: flush renderer @ %.3f", mName.c_str(), time.seconds());
            
            // clear state
            mClockUpdated   = false;
            mLastFrameTime  = kMediaTimeInvalid;
            mInputEOS       = false;
            mOutputQueue.clear();
            
            // update generation
            mFrameReadyEvent = new OnFrameReady(this, ++mGeneration);

            // flush output
            if (!mOut.isNIL()) mOut->flush();

            mState = kRenderFlushed;
        }
        
        // don't request frame if eos detected.
        if (mInputEOS) return;

        if (mOutputQueue.size()) {
            sp<MediaFrame>& first = mOutputQueue.front();
            sp<MediaFrame>& last = mOutputQueue.back();
            MediaTime length = last->timecode - first->timecode;
            if (length.useconds() >= MAX_LENGTH) {
                DEBUG("%s: output queue is full, length = %zu",
                      mName.c_str(), mOutputQueue.size());
                return;
            }
        }
        
        mFrameRequestEvent->fire(mFrameReadyEvent, time);
        DEBUG("%s: request more frames", mName.c_str());
        // -> onFrameReady
    }

    struct OnFrameReady : public FrameReadyEvent {
        RenderSession *thiz;
        const int mGeneration;
        OnFrameReady(RenderSession *p, int gen) :
            FrameReadyEvent(p->mDispatch), thiz(p), mGeneration(gen) { }

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
            mInputEOS = true;
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

        // check pts
        if (frame->timecode == kMediaTimeInvalid) {
            ERROR("%s: bad pts", mName.c_str());
            // FIXME:
        }
        
        if (mLastFrameTime == kMediaTimeInvalid) {
            INFO("%s: first frame %.3f(s)", mName.c_str(), frame->timecode.seconds());
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
            
            // handle outdated frames
            if (frame->timecode.useconds() < mClock->get()) {
                ERROR("%s: underrun, drop frame, %.3f(s) vs %.3f(s), queue length %zu",
                      mName.c_str(), frame->timecode.seconds(), mClock->get() / 1E6, mOutputQueue.size());
            } else {
                mOutputQueue.push(frame);
            }
        }

        // request more frames
        requestFrame(kMediaTimeInvalid);

        // prepare done ?
        if (mState == kRenderInitialized && (mOutputQueue.size() >= MIN_COUNT || mInputEOS)) {
            INFO("%s: prepare done", mName.c_str());
            mState = kRenderReady;
            notify(kSessionInfoReady, NULL);
        }

        // remember last frame pts
        mLastFrameTime = frame->timecode;
    }

    struct RenderJob : public Job {
        RenderSession *thiz;
        RenderJob(RenderSession *s) : Job(), thiz(s) { }
        virtual void onJob() {
            thiz->onRender();
        }
    };

    void onRender() {
        CHECK_FALSE(mClock.isNIL());
        DEBUG("%s: output queue size %zu", mName.c_str(), mOutputQueue.size());
    
        if (mClock->isPaused()) {
            INFO("%s: clock is paused", mName.c_str());
            return;
        } else if (mInputEOS && mOutputQueue.empty()) {
            // tell out device about eos
            INFO("%s: eos...", mName.c_str());
            writeFrame(NULL);
            notify(kSessionInfoEnd, NULL);
            return;
        }
        
        int64_t next = DEFAULT_REFRESH_RATE;
        if (!mInputEOS && mOutputQueue.empty()) {
            WARN("%s: underrun happens ...", mName.c_str());
        } else {
            next = render();
            if (next < 0) {
                // render() return error, stop render
                return;
            }
        }
        
        mDispatch->dispatch(mRenderJob, next);
        // -> onRender
    }
    
    FORCE_INLINE MediaError writeFrame(const sp<MediaFrame>& input) {
        sp<MediaFrame> frame = input;
        if (!input.isNIL() && !mAudioConverter.isNIL()) {
            frame = mAudioConverter->convert(input);
        }
        
        if (mOut != NULL) {
            return mOut->write(frame);
        } else {
            mMediaFrameEvent->fire(frame);
            return kMediaNoError;
        }
    }

    // render current frame
    // return next frame render time on success, or return -1
    // DO NOT DROP FRAMES HERE, DROP onFrameReady
    FORCE_INLINE int64_t render() {
        DEBUG("%s: render with %zu frame ready", mName.c_str(), mOutputQueue.size());

        sp<MediaFrame> frame = mOutputQueue.front();
        CHECK_TRUE(frame != NULL);
        
        const int64_t currentMediaTime = mClock->get();
        // only master clock can skip this one time
        if (mClock->role() == kClockRoleSlave || mClockUpdated) {
            int64_t early = frame->timecode.useconds() - currentMediaTime - mLatency;
            
            if (early > JITTER_TIME) {
                INFO("%s: overrun by %.3f(s), %.3f(s) vs %.3f(s)...", mName.c_str(),
                      early/1E6, frame->timecode.seconds(), currentMediaTime / 1E6);
                return early;
            } else if (early < -JITTER_TIME) {
                WARN("%s: underrun by %.3f(s), %.3f(s) vs %.3f(s)...", mName.c_str(),
                      -early/1E6, frame->timecode.seconds(), currentMediaTime / 1E6);
                // only warn here, DO NOT drop frames, onFrameReady will handle outdated frames
            }
        }

        DEBUG("%s: render frame %.3f(s) @ %.3f(s)",
              mName.c_str(), frame->timecode.seconds(), currentMediaTime / 1E6);
        
        MediaError st = writeFrame(frame);
        if (st != kMediaNoError) {
            ERROR("%s: play frame return error %#x", mName.c_str(), st);
            notify(kSessionInfoError, NULL);
            return -1;
        }
    
        mOutputQueue.pop();
        ++mFramesRenderred;
        
        // request a new frame
        requestFrame(kMediaTimeInvalid);
        
        // update clock
        if (mClock->role() == kClockRoleMaster && !mClockUpdated) {
            mClock->update(frame->timecode.useconds() - mLatency);
            INFO("%s: update clock %.3f(s) - %.3f(s), latency %.3f(s)", mName.c_str(),
                 frame->timecode.seconds(), mClock->get() / 1E6, mLatency / 1E6);
            mClockUpdated = true;
        }
        
        // render next frame n usecs later.
        int64_t next = DEFAULT_REFRESH_RATE;
        if (mOutputQueue.size()) {
            next = mOutputQueue.front()->timecode.useconds() - mClock->get();
            if (next < 0) next = 0;
        }
        return next;
    }

    // using clock to control render session, start|pause|...
    struct OnClockEvent : public ClockEvent {
        RenderSession *thiz;
        OnClockEvent(RenderSession *p) : ClockEvent(p->mDispatch), thiz(p) { }

        virtual void onEvent(const eClockState& cs) {
            DEBUG("clock state => %d", cs);
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
        INFO("%s: start @ %.3f(s)", mName.c_str(), mClock->get() / 1E6);
        //onPrintStat();

        // check
        if (mDispatch->exists(mRenderJob)) {
            ERROR("%s: already started", mName.c_str());
            return;
        }

        if (mOut != NULL) {
            sp<Message> options = new Message;
            options->setInt32(kKeyPause, 0);
            mOut->configure(options);
        }

        mClockUpdated = false;
        mClock->start();
        onRender();
    }

    void onPauseRenderer() {
        INFO("%s: pause @ %.3f(s)", mName.c_str(), mClock->get() / 1E6);
        mDispatch->remove(mRenderJob);

        if (mOut != NULL) {
            sp<Message> options = new Message;
            options->setInt32(kKeyPause, 1);
            mOut->configure(options);
        }
        INFO("%s: paused @ %.3f(s)", mName.c_str(), mClock->get() / 1E6);
        mClock->pause();
    }
    
    void onPrepareRenderer() {
        const MediaTime pos = MediaTime(mClock->get());
        INFO("%s: prepare render @ %.3f(s)", mName.c_str(), pos.seconds());
        mDispatch->remove(mRenderJob);
        
        // prepare in cache ?
        if (mOutputQueue.size()) {
            MediaTime& start = mOutputQueue.front()->timecode;
            MediaTime& end = mOutputQueue.back()->timecode;
            if (pos >= start && pos < end) {
                List<sp<MediaFrame> >::iterator it = mOutputQueue.begin();
            }
        }
        
        requestFrame(pos);
    }
};

sp<IMediaSession> CreateRenderSession(const sp<Looper>& lp) {
    return new RenderSession(lp);
}
__END_NAMESPACE_MPX
