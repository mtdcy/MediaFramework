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


// File:    mpx.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20181126     initial version
//

#define LOG_TAG "Player"
//#define LOG_NDEBUG 0
#include "MediaPlayer.h"
#include "MediaSession.h"
#include "MediaFile.h"
#include "MediaClock.h"
#include "MediaDecoder.h"
#include "MediaOut.h"

__USING_NAMESPACE_MPX

IMediaPlayer::IMediaPlayer(const sp<Looper>& lp) : IMediaSession(lp), mClock(new SharedClock)
{
    
}

sp<Clock> IMediaPlayer::clock() const {
    return new Clock(mClock);
}

struct IMediaPlayer::StartJob : public Job {
    IMediaPlayer *thiz;
    
    StartJob(IMediaPlayer *p) : thiz(p) { }
    
    virtual void onJob() {
        thiz->onStart();
    }
};

struct IMediaPlayer::PauseJob : public Job {
    IMediaPlayer *thiz;
    
    PauseJob(IMediaPlayer *p) : thiz(p) { }
    
    virtual void onJob() {
        thiz->onPause();
    }
};

struct IMediaPlayer::PrepareJob : public Job {
    IMediaPlayer *thiz;
    MediaTime when;
    
    PrepareJob(IMediaPlayer *p, const MediaTime& pos) : thiz(p), when(pos) { }
    
    virtual void onJob() {
        thiz->onPrepare(when);
    }
};

void IMediaPlayer::start() {
    mDispatch->dispatch(new StartJob(this));
}

void IMediaPlayer::pause() {
    mDispatch->dispatch(new PauseJob(this));
}

void IMediaPlayer::prepare(const MediaTime& pos) {
    mDispatch->dispatch(new PrepareJob(this, pos));
}

sp<IMediaPlayer> CreateTiger(const sp<Message>& media, const sp<Message>& options);
sp<IMediaPlayer> IMediaPlayer::Create(const sp<Message>& media, const sp<Message>& options) {
    return CreateTiger(media, options);
}
