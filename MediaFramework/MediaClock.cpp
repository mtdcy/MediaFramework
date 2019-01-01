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


// File:    Clock.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20181126     initial version
//

#define LOG_TAG "mpx.Clock"
//#define LOG_NDEBUG 0
#include <MediaToolkit/Toolkit.h>
#include "MediaClock.h"

namespace mtdcy {
    SharedClock::ClockInt::ClockInt() :
        mPaused(true),
        mMediaTime(kTimeBegin), mSystemTime(kTimeBegin),
        mSpeed(1.0f)
    {

    }

    SharedClock::SharedClock() :
        mGeneration(0), mMasterClock(0),
        mClockInt()
    {
    }

    void SharedClock::start() {
        if (atomic_load(&mMasterClock)) {
            // wait master clock to update
            // BUT, we still set clock state here.
        } else {
            AutoLock _l(mLock);
            mClockInt.mPaused = false;
            mClockInt.mSystemTime = SystemTimeUs();
            atomic_add(&mGeneration, 1);
        }
        notifyListeners_l(kClockStateTicking);
    }
    
    void SharedClock::set(const MediaTime& t) {
        AutoLock _l(mLock);
        mClockInt.mMediaTime  = t;
        mClockInt.mSystemTime = SystemTimeUs();
        atomic_add(&mGeneration, 1);
    }
    
    void SharedClock::update(ClockInt& c) {
        AutoLock _l(mLock);
        mClockInt = c;
        atomic_add(&mGeneration, 1);
    }

    MediaTime SharedClock::get() const {
        if (mClockInt.mPaused) {
            return mClockInt.mMediaTime;
        }

        MediaTime now = SystemTimeUs();
        return mClockInt.mMediaTime +
            (now - mClockInt.mSystemTime) * mClockInt.mSpeed;
    }

    void SharedClock::pause() {
        AutoLock _l(mLock);
        mClockInt.mMediaTime = get();
        mClockInt.mSystemTime = SystemTimeUs();
        mClockInt.mPaused = true;
        atomic_add(&mGeneration, 1);
        notifyListeners_l(kClockStatePaused);
    }

    bool SharedClock::isPaused() const {
        return mClockInt.mPaused;
    }

    void SharedClock::setSpeed(double s) {
        AutoLock _l(mLock);
        mClockInt.mSpeed = s;
        atomic_add(&mGeneration, 1);
    }

    double SharedClock::speed() const {
        return mClockInt.mSpeed;
    }

    void SharedClock::reset() {
        AutoLock _l(mLock);

        mClockInt.mMediaTime = 0;
        mClockInt.mSystemTime = 0;
        mClockInt.mPaused = true;
        atomic_add(&mGeneration, 1);
        
        notifyListeners_l(kClockStateReset);
    }

    sp<Clock> SharedClock::getClock(eClockRole role) {
        if (role == kClockRoleMaster) {
            int old = atomic_add(&mMasterClock, 1);
            CHECK_EQ(old , 0);
        }
        return new Clock(*this, role);
    }
    
    void SharedClock::_regListener(const sp<ClockEvent> &ce) {
        AutoLock _l(mLock);
        mListeners.push(ce);
    }
    
    void SharedClock::_unregListener(const sp<ClockEvent> &ce) {
        AutoLock _l(mLock);
        List<sp<ClockEvent> >::iterator it = mListeners.begin();
        for (; it != mListeners.end(); ++it) {
            if (ce == (*it)) {
                mListeners.erase(it);
                break;
            }
        }
    }
    
    void SharedClock::notifyListeners_l(eClockState state) {
        List<sp<ClockEvent> >::iterator it = mListeners.begin();
        for (; it != mListeners.end(); ++it) {
            (*it)->fire(state);
        }
    }

    Clock::Clock(SharedClock& sc, eClockRole role) :
        mClock(sc), mRole(role), mListener(NULL),
        mGeneration(0)
    {
        reload();
    }

    Clock::~Clock() {
        if (mRole == kClockRoleMaster) {
            atomic_sub(&mClock.mMasterClock, 1);
        }
    }

    void Clock::reload() const {
        int gen = atomic_load(&mClock.mGeneration);
        if (gen == mGeneration) return;

        AutoLock _l(mClock.mLock);
        mGeneration = gen;
        mClockInt = mClock.mClockInt;
    }
    
    void Clock::setListener(const sp<ClockEvent> &ce) {
        if (ce == NULL) {
            if (mListener != NULL) mClock._unregListener(mListener);
        } else {
            mClock._regListener(ce);
        }
        mListener = ce;
    }
 
    void Clock::update(const MediaTime& t) {
        CHECK_EQ(mRole, (uint32_t)kClockRoleMaster);
        mClockInt.mMediaTime = t;
        mClockInt.mSystemTime = SystemTimeUs();
        if (mClockInt.mPaused) {
            mClockInt.mPaused = false;
        }
        mClock.update(mClockInt);
        reload();
    }

    void Clock::update(const MediaTime& t, int64_t real) {
        CHECK_EQ(mRole, (uint32_t)kClockRoleMaster);
        mClockInt.mMediaTime = t;
        mClockInt.mSystemTime = real;
        if (mClockInt.mPaused) {
            mClockInt.mPaused = false;
        }
        mClock.update(mClockInt);
        reload();
    }

    bool Clock::isPaused() const {
        reload();
        return mClockInt.mPaused;
    }

    double Clock::speed() const {
        reload();
        return mClockInt.mSpeed;
    }

    MediaTime Clock::get() const {
        reload();
        if (mClockInt.mPaused) {
            return mClockInt.mMediaTime;
        }

        MediaTime now = SystemTimeUs();
        return mClockInt.mMediaTime + (now - mClockInt.mSystemTime) * mClockInt.mSpeed;
    }
    
    void Clock::set(const MediaTime& t) {
        mClockInt.mMediaTime = t;
        mClockInt.mSystemTime = SystemTimeUs();
        mClock.update(mClockInt);
    }
}
