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

__USING_NAMESPACE_MPX

struct IMediaSession::InitJob : public Job {
    IMediaSession * thiz;
    InitJob(IMediaSession * session) : thiz(session) { }
    virtual void onJob() {
        thiz->onInit();
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
    mLooper->post(new InitJob(this));
}

void IMediaSession::onLastRetain() {
    mLooper->post(new ReleaseJob(this));
    // FIXME: if we share Looper with others, this will be a problem.
    mLooper->flush();
    // after this mLooper is accessable in onRelease()
    mLooper.clear();    // wait jobs complete and release looper
}

sp<IMediaSession> CreateMediaSource(const sp<Message>& media, const sp<Message>& options);
Object<IMediaSession> CreateDecodeSession(const Object<Message>& format, const Object<Message>& options);
Object<IMediaSession> CreateRenderSession(const Object<Message>& format, const Object<Message>& options);
Object<IMediaSession> IMediaSession::Create(const Object<Message>& format, const Object<Message>& options) {
    if (format->contains("url")) {
        return CreateMediaSource(format, options);
    } else if (options->contains("PacketRequestEvent")) {
        return CreateDecodeSession(format, options);
    } else if (options->contains("FrameRequestEvent")) {
        return CreateRenderSession(format, options);
    }
    return NULL;
}
