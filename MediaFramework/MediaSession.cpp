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
#include "MediaDevice.h"

__BEGIN_NAMESPACE_MPX

struct IMediaSession::InitJob : public Job {
    IMediaSession * thiz;
    sp<Message>     formats;
    sp<Message>     options;
    InitJob(IMediaSession * session) : thiz(session) { }
    virtual void onJob() {
        thiz->onInit(formats, options);
    }
};

struct IMediaSession::ReleaseJob : public Job {
    IMediaSession * thiz;
    ReleaseJob(IMediaSession * session) : thiz(session) { }
    virtual void onJob() {
        thiz->onRelease();
    }
};

void IMediaSession::onFirstRetain() {
}

void IMediaSession::onLastRetain() {
    mDispatch->flush();
    mDispatch->sync(new ReleaseJob(this));
    // wait jobs complete and release disptch queue
    INFO("MediaSession released, %zu", mDispatch.refsCount());
    mDispatch.clear();
}

sp<IMediaSession> CreateMediaSource(const sp<Looper>&);
sp<IMediaSession> CreateDecodeSession(const sp<Looper>&);
sp<IMediaSession> CreateRenderSession(const sp<Looper>&);
sp<IMediaSession> IMediaSession::Create(const sp<Message>& format, const sp<Message>& options) {
    sp<Looper> looper = options->findObject(kKeyLooper);
    if (looper.isNIL()) {
        // create a looper
        String name;
        if (format->contains(kKeyURL))    name = "source";
        else {
            int32_t value = format->findInt32(kKeyFormat);
            name = String::format("%.4s", (const char *)&value);
        }
        looper = new Looper(name);
    }
    
    sp<IMediaSession> session;
    if (format->contains(kKeyURL)) {
        session = CreateMediaSource(looper);
    } else if (options->contains(kKeyPacketRequestEvent)) {
        session = CreateDecodeSession(looper);
    } else if (options->contains(kKeyFrameRequestEvent)) {
        session = CreateRenderSession(looper);
    }
    if (session.isNIL()) {
        ERROR("create session failed << %s << %s", format->string().c_str(), options->string().c_str());
    }
    sp<InitJob> init = new InitJob(session.get());
    init->formats = format;
    init->options = options;
    session->mDispatch->dispatch(init);
    return session;
}

__END_NAMESPACE_MPX
