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
    mMediaTime(kTimeBegin), mSystemTime(kTimeBegin),
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

    if (atomic_load(&mMasterClock)) {
        // wait master clock to update
    } else {
        mClockInt.mTicking  = true;
    }
    atomic_add(&mGeneration, 1);
    notifyListeners_l(kClockStateTicking);
}

void SharedClock::set(const MediaTime& t) {
    // NOT alter clock state here
    AutoLock _l(mLock);
    mClockInt.mMediaTime  = t;
    mClockInt.mSystemTime = SystemTimeUs();
    atomic_add(&mGeneration, 1);
}

void SharedClock::update(const ClockInt& c) {
    AutoLock _l(mLock);
    if (mClockInt.mPaused) return;
    mClockInt = c;
    atomic_add(&mGeneration, 1);
}

MediaTime SharedClock::get() const {
    AutoLock _l(mLock);
    return get_l();
}

MediaTime SharedClock::get_l() const {
    if (mClockInt.mPaused || !mClockInt.mTicking) {
        return mClockInt.mMediaTime;
    }

    MediaTime now = SystemTimeUs();
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
    atomic_add(&mGeneration, 1);
    notifyListeners_l(kClockStatePaused);
}

bool SharedClock::isPaused() const {
    AutoLock _l(mLock);
    return mClockInt.mPaused;
}

void SharedClock::setSpeed(double s) {
    AutoLock _l(mLock);
    mClockInt.mSpeed = s;
    atomic_add(&mGeneration, 1);
}

double SharedClock::speed() const {
    AutoLock _l(mLock);
    return mClockInt.mSpeed;
}

void SharedClock::reset() {
    AutoLock _l(mLock);

    mClockInt.mMediaTime    = kTimeBegin;
    mClockInt.mSystemTime   = kTimeBegin;
    mClockInt.mPaused       = true;
    mClockInt.mTicking      = false;
    atomic_add(&mGeneration, 1);

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
        int old = atomic_add(&mClock->mMasterClock, 1);
        CHECK_EQ(old, 0);   // only one master clock allowed
    }
    reload();
}

Clock::~Clock() {
    if (mRole == kClockRoleMaster) {
        atomic_sub(&mClock->mMasterClock, 1);
    }
    mClock.clear();
}

void Clock::reload() const {
    int gen = atomic_load(&mClock->mGeneration);
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

void Clock::update(const MediaTime& t) {
    CHECK_EQ(mRole, (uint32_t)kClockRoleMaster);
    mClockInt.mMediaTime    = t;
    mClockInt.mSystemTime   = SystemTimeUs();
    mClockInt.mTicking      = true;
    mClock->update(mClockInt);
    reload();
}

void Clock::update(const MediaTime& t, int64_t real) {
    CHECK_EQ(mRole, (uint32_t)kClockRoleMaster);
    mClockInt.mMediaTime    = t;
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

MediaTime Clock::get() const {
    reload();
    if (mClockInt.mPaused || !mClockInt.mTicking) {
        return mClockInt.mMediaTime;
    }

    MediaTime now = SystemTimeUs();
    return mClockInt.mMediaTime + (now - mClockInt.mSystemTime) * mClockInt.mSpeed;
}

void Clock::set(const MediaTime& t) {
    mClockInt.mMediaTime    = t;
    mClockInt.mSystemTime   = SystemTimeUs();
    mClock->update(mClockInt);
    reload();
}
__END_NAMESPACE_MPX
