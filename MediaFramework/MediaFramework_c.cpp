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


#define LOG_TAG "MediaFramework.c"
#include "MediaFramework.h"

__BEGIN_DECLS

__USING_NAMESPACE_MPX

MediaFrameRef AudioFrameCreate(const AudioFormat * audio) {
    Object<MediaFrame> frame = MediaFrameCreate(*audio);
    return (MediaFrameRef)frame->RetainObject();
}

MediaFrameRef ImageFrameCreate(const ImageFormat * image) {
    // TODO
}

uint8_t * MediaFrameGetPlaneData(MediaFrameRef ref, size_t index) {
    Object<MediaFrame> frame = ref;
    CHECK_LT(index, MEDIA_FRAME_NB_PLANES);
    return frame->planes[index].data;
}

size_t MediaFrameGetPlaneSize(MediaFrameRef ref, size_t index) {
    Object<MediaFrame> frame = ref;
    CHECK_LT(index, MEDIA_FRAME_NB_PLANES);
    return frame->planes[index].size;
}

AudioFormat * MediaFrameGetAudioFormat(MediaFrameRef ref) {
    Object<MediaFrame> frame = ref;
    return &frame->a;
}

ImageFormat * MediaFrameGetImageFormat(MediaFrameRef ref) {
    Object<MediaFrame> frame = ref;
    return &frame->v;
}

void * MediaFrameGetOpaque(MediaFrameRef ref) {
    Object<MediaFrame> frame = ref;
    return frame->opaque;
}

struct UserFrameEvent : public MediaFrameEvent {
    void (*callback)(MediaFrameRef, void *);
    void * opaque;
    
    UserFrameEvent(void (*cb)(MediaFrameRef, void *), void * user) :
    MediaFrameEvent(), callback(cb), opaque(user) { }
    
    void onEvent(const Object<MediaFrame>& frame) {
        callback(frame.get(), opaque);
    }
};

FrameEventRef FrameEventCreate(void (*callback)(MediaFrameRef, void *), void * user) {
    Object<UserFrameEvent> event = new UserFrameEvent(callback, user);
    return event->RetainObject();
}

MediaPlayerRef MediaPlayerCreate(MessageRef media, MessageRef options) {
    INFO("media: %s", media->string().c_str());
    INFO("options: %s", options->string().c_str());
    Object<IMediaPlayer> mp = IMediaPlayer::Create(*options);
    mp->init(*media);
    return mp->RetainObject();
}

MediaClockRef MediaPlayerGetClock(MediaPlayerRef player) {
    Object<IMediaPlayer> _player = player;
    Object<Clock> clock = _player->clock();
    if (clock == NULL) return NULL;
    else return (MediaClockRef)clock->RetainObject();
}

MessageRef MediaPlayerGetInfo(const MediaPlayerRef ref) {
    const Object<IMediaPlayer> mp = ref;
    Object<Message> info = mp->info();
    if (info == NULL) return NULL;
    else return (MessageRef)info->RetainObject();
}

MediaError MediaPlayerPrepare(MediaPlayerRef ref, int64_t us) {
    Object<IMediaPlayer> mp = ref;
    return mp->prepare(us);
}

MediaError MediaPlayerStart(MediaPlayerRef ref) {
    Object<IMediaPlayer> mp = ref;
    return mp->start();
}

MediaError MediaPlayerPause(MediaPlayerRef ref) {
    Object<IMediaPlayer> mp = ref;
    return mp->pause();
}

MediaError MediaPlayerFlush(MediaPlayerRef ref) {
    Object<IMediaPlayer> mp = ref;
    return mp->flush();
}

MediaError MediaPlayerRelease(MediaPlayerRef ref) {
    Object<IMediaPlayer> mp = ref;
    MediaError st = mp->release();
    mp->ReleaseObject();
    return st;
}

eStateType MediaPlayerGetState(const MediaPlayerRef ref) {
    const Object<IMediaPlayer> mp = ref;
    return mp->state();
}

MediaOutRef MediaOutCreate(eCodecType type) {
    Object<MediaOut> out = MediaOut::Create(type);
    return (MediaOutRef)out->RetainObject();
}

MediaError MediaOutPrepare(MediaOutRef ref, MessageRef format, MessageRef options) {
    Object<MediaOut> out = ref;
    Object<Message> _format = format;
    Object<Message> _options = options;
    return out->prepare(*_format, *_options);
}

MediaError MediaOutWrite(MediaOutRef ref, MediaFrameRef frame) {
    Object<MediaOut> out = ref;
    return out->write(frame);
}

MediaError MediaOutFlush(MediaOutRef ref) {
    Object<MediaOut> out = ref;
    return out->flush();
}

int64_t MediaClockGetTime(MediaClockRef ref) {
    Object<Clock> clock = ref;
    return clock->get();
}

__END_DECLS
