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
    mMediaTime(0), mSystemTime(0),
    mPaused(true), mTicking(false), mSpeed(1.0f)
{
}

SharedClock::SharedClock() :
    mGeneration(1), mMasterClock(0), mClockInt()
{
}

void SharedClock::start() {
    AutoLock _l(mLock);
    if (!mClockInt.mPaused) return;
    mClockInt.mPaused       = false;
    mClockInt.mSystemTime   = SystemTimeUs();

    if (mMasterClock.load()) {
        // wait master clock to update
    } else {
        mClockInt.mTicking  = true;
    }
    ++mGeneration;
    notifyListeners_l(kClockStateTicking);
}

void SharedClock::set(int64_t us) {
    // NOT alter clock state here
    AutoLock _l(mLock);
    mClockInt.mMediaTime  = us;
    mClockInt.mSystemTime = SystemTimeUs();
    ++mGeneration;
}

void SharedClock::update(const ClockInt& c) {
    AutoLock _l(mLock);
    if (mClockInt.mPaused) return;
    mClockInt = c;
    ++mGeneration;
}

int64_t SharedClock::get() const {
    AutoLock _l(mLock);
    return get_l();
}

int64_t SharedClock::get_l() const {
    if (mClockInt.mPaused || !mClockInt.mTicking) {
        return mClockInt.mMediaTime;
    }

    int64_t now = SystemTimeUs();
    return mClockInt.mMediaTime +
        (now - mClockInt.mSystemTime) * mClockInt.mSpeed;
}

void SharedClock::pause() {
    AutoLock _l(mLock);
    if (mClockInt.mPaused) return;

    mClockInt.mMediaTime    = get_l();
    mClockInt.mSystemTime   = SystemTimeUs();
    mClockInt.mPaused       = true;
    mClockInt.mTicking      = false;
    ++mGeneration;
    notifyListeners_l(kClockStatePaused);
}

bool SharedClock::isPaused() const {
    AutoLock _l(mLock);
    return mClockInt.mPaused;
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

void SharedClock::reset() {
    AutoLock _l(mLock);

    mClockInt.mMediaTime    = 0;
    mClockInt.mSystemTime   = 0;
    mClockInt.mPaused       = true;
    mClockInt.mTicking      = false;
    ++mGeneration;

    notifyListeners_l(kClockStateReset);
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
        CHECK_EQ(++mClock->mMasterClock, 1);   // only one master clock allowed
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
    CHECK_EQ(mRole, (uint32_t)kClockRoleMaster);
    mClockInt.mMediaTime    = us;
    mClockInt.mSystemTime   = SystemTimeUs();
    mClockInt.mTicking      = true;
    mClock->update(mClockInt);
    reload();
}

void Clock::update(int64_t us, int64_t real) {
    CHECK_EQ(mRole, (uint32_t)kClockRoleMaster);
    mClockInt.mMediaTime    = us;
    mClockInt.mSystemTime   = real;
    mClockInt.mTicking      = true;
    mClock->update(mClockInt);
    reload();
}

bool Clock::isPaused() const {
    reload();
    return mClockInt.mPaused;
}

bool Clock::isTicking() const {
    reload();
    return mClockInt.mTicking;
}

double Clock::speed() const {
    reload();
    return mClockInt.mSpeed;
}

int64_t Clock::get() const {
    reload();
    if (mClockInt.mPaused || !mClockInt.mTicking) {
        return mClockInt.mMediaTime;
    }

    int64_t now = SystemTimeUs();
    return mClockInt.mMediaTime + (now - mClockInt.mSystemTime) * mClockInt.mSpeed;
}

void Clock::set(int64_t us) {
    mClockInt.mMediaTime    = us;
    mClockInt.mSystemTime   = SystemTimeUs();
    mClock->update(mClockInt);
    reload();
}
__END_NAMESPACE_MPX
