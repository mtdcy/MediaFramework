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

#define LOG_TAG "Clock"
//#define LOG_NDEBUG 0
#include <ABE/ABE.h>
#include "MediaClock.h"


__BEGIN_NAMESPACE_MPX

SharedClock::ClockInt::ClockInt() :
    mMediaTime(0LL), mSystemTime(0LL),
    mStarted(false), mTicking(false), mSpeed(1.0f)
{
}

SharedClock::SharedClock() : SharedObject(),
    mGeneration(0), mMasterClock(0), mClockInt()
{
}

void SharedClock::start() {
    AutoLock _l(mLock);
    if (mClockInt.mStarted) return;
    
    // start clock without alter mMediaTime
    mClockInt.mStarted      = true;
    mClockInt.mSystemTime   = SystemTimeUs();

    // if master clock exists, wait master clock to update
    if (mMasterClock.load()) {
        // wait master clock to update
    } else {
        mClockInt.mTicking  = true;
    }
    ++mGeneration;
    notifyListeners_l(kClockStateTicking);
}

void SharedClock::set(int64_t us) {
    AutoLock _l(mLock);
    
    // set clock time without alter its state
    mClockInt.mMediaTime  = us;
    mClockInt.mSystemTime = SystemTimeUs();
    ++mGeneration;
    notifyListeners_l(kClockStateTimeChanged);
}

void SharedClock::update(const ClockInt& c) {
    AutoLock _l(mLock);
    
    // update can NOT alter clock state
    // when shadow clock update, start()|pause() may be called
    if (!mClockInt.mStarted) return;
    
    CHECK_EQ(mClockInt.mStarted, c.mStarted);
    
    mClockInt = c;
    ++mGeneration;
}

// get clock time with speed
int64_t SharedClock::get() const {
    AutoLock _l(mLock);
    return get_l() * mClockInt.mSpeed;
}

// get clock time without speed
int64_t SharedClock::get_l() const {
    if (!mClockInt.mStarted || !mClockInt.mTicking) {
        return mClockInt.mMediaTime;
    }

    int64_t now = SystemTimeUs();
    return mClockInt.mMediaTime + (now - mClockInt.mSystemTime);
}

void SharedClock::pause() {
    AutoLock _l(mLock);
    
    if (!mClockInt.mStarted) return;

    mClockInt.mMediaTime    = get_l();
    mClockInt.mSystemTime   = SystemTimeUs();
    mClockInt.mStarted      = false;
    mClockInt.mTicking      = false;
    ++mGeneration;
    notifyListeners_l(kClockStatePaused);
}

bool SharedClock::isPaused() const {
    AutoLock _l(mLock);
    return !mClockInt.mStarted;
}

void SharedClock::setSpeed(double s) {
    AutoLock _l(mLock);
    mClockInt.mSpeed = s;
    ++mGeneration;
}

double SharedClock::speed() const {
    AutoLock _l(mLock);
    return mClockInt.mSpeed;
}

void SharedClock::_regListener(const void * who, const sp<ClockEvent> &ce) {
    AutoLock _l(mLock);
    mListeners.erase(who);
    mListeners.insert(who, ce);
}

void SharedClock::_unregListener(const void * who) {
    AutoLock _l(mLock);
    mListeners.erase(who);
}

void SharedClock::notifyListeners_l(eClockState state) {
    HashTable<const void *, sp<ClockEvent> >::iterator it = mListeners.begin();
    for (; it != mListeners.end(); ++it) {
        it.value()->fire(state);
    }
}

Clock::Clock(const sp<SharedClock>& sc, eClockRole role) :
    mClock(sc), mRole(role), mGeneration(0)
{
    if (role == kClockRoleMaster) {
        // only one master clock allowed
        CHECK_EQ(++mClock->mMasterClock, 1, "only allow one master clock");
    }
    reload();
}

Clock::~Clock() {
    if (mRole == kClockRoleMaster) {
        mClock->mMasterClock -= 1;
    }
    mClock.clear();
}

void Clock::reload() const {
    // compare generation without lock
    int gen = mClock->mGeneration.load();
    CHECK_GE(gen, mGeneration);
    if (gen == mGeneration) return;

    AutoLock _l(mClock->mLock);
    mGeneration = gen;
    mClockInt   = mClock->mClockInt;
}

void Clock::setListener(const sp<ClockEvent> &ce) {
    if (ce == NULL) {
        mClock->_unregListener(this);
    } else {
        mClock->_regListener(this, ce);
    }
}

void Clock::update(int64_t us) {
    CHECK_EQ(mRole, (uint32_t)kClockRoleMaster, "only master clock can update");
    if (mClockInt.mTicking) {
        int64_t delta = us - get();
        mClockInt.mMediaTime    += delta;
    } else {
        mClockInt.mMediaTime    = us;
        mClockInt.mSystemTime   = SystemTimeUs();
        mClockInt.mTicking      = true;
    }
    
    mClock->update(mClockInt);
}

bool Clock::isPaused() const {
    reload();
    return !mClockInt.mStarted;
}

#if 0
bool Clock::isTicking() const {
    reload();
    return mClockInt.mTicking;
}
#endif

double Clock::speed() const {
    reload();
    return mClockInt.mSpeed;
}

int64_t Clock::get() const {
    reload();
    if (!mClockInt.mStarted || !mClockInt.mTicking) {
        return mClockInt.mMediaTime * mClockInt.mSpeed;
    }

    int64_t now = SystemTimeUs();
    return (mClockInt.mMediaTime + (now - mClockInt.mSystemTime)) * mClockInt.mSpeed;
}

__END_NAMESPACE_MPX
