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

ImageFileRef ImageFileOpen(ContentObjectRef ref) {
    Object<Content> pipe = ref;
    Object<ImageFile> file = ImageFile::Create();
    if (file->init(pipe, NULL) == kMediaNoError) {
        return file->RetainObject();
    }
    return NULL;
}

MediaFrameRef AudioFrameCreate(const AudioFormat * audio) {
    Object<MediaFrame> frame = MediaFrame::Create(*audio);
    return (MediaFrameRef)frame->RetainObject();
}

MediaFrameRef ImageFrameCreate(const ImageFormat * image) {
    // TODO
}

MediaFrameRef ImageFrameGenerate(const ImageFormat * image, BufferObjectRef buffer) {
    Object<MediaFrame> frame = MediaFrame::Create(*image, buffer);
    return frame->RetainObject();
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

MediaError ImageFrameSwapCbCr(MediaFrameRef ref) {
    Object<MediaFrame> frame = ref;
    return frame->swapCbCr();
}

MediaError ImageFrameReversePixel(MediaFrameRef ref) {
    Object<MediaFrame> frame = ref;
    return frame->reversePixel();
}

MediaError ImageFramePlanarization(MediaFrameRef ref) {
    Object<MediaFrame> frame = ref;
    return frame->planarization();
}

MediaError ImageFrameToRGB(MediaFrameRef ref) {
    Object<MediaFrame> frame = ref;
    return frame->yuv2rgb();
}

struct UserFrameEvent : public MediaFrameEvent {
    FrameCallback Callback;
    void * opaque;
    
    UserFrameEvent(const Object<Looper>& lp, FrameCallback cb, void * user) :
    MediaFrameEvent(lp), Callback(cb), opaque(user) { }
    
    virtual void onEvent(const Object<MediaFrame>& frame) {
        Callback(frame.get(), opaque);
    }
};

FrameEventRef FrameEventCreate(LooperObjectRef ref, FrameCallback cb, void * user) {
    Object<Looper> lp = ref;
    Object<UserFrameEvent> event = new UserFrameEvent(lp, cb, user);
    return (FrameEventRef)event->RetainObject();
}

struct UserInfoEvent : public PlayerInfoEvent {
    void (*callback)(ePlayerInfoType, void *);
    void * opaque;
    UserInfoEvent(const Object<Looper>& lp, void (*cb)(ePlayerInfoType, void *), void * user) :
    PlayerInfoEvent(lp), callback(cb), opaque(user) { }
    
    virtual void onEvent(const ePlayerInfoType& info) {
        callback(info, opaque);
    }
};

PlayerInfoEventRef PlayerInfoEventCreate(LooperObjectRef ref, void (*callback)(ePlayerInfoType, void *), void * user) {
    Object<Looper> lp = ref;
    Object<UserInfoEvent> event = new UserInfoEvent(lp, callback, user);
    return (PlayerInfoEventRef)event->RetainObject();
}

MediaPlayerRef MediaPlayerCreate(MessageObjectRef media, MessageObjectRef options) {
    Object<IMediaPlayer> mp = IMediaPlayer::Create(media, options);
    return mp->RetainObject();
}

MediaClockRef MediaPlayerGetClock(MediaPlayerRef ref) {
    Object<IMediaPlayer> mp = ref;
    Object<Clock> clock = mp->clock();
    if (clock == NULL) return NULL;
    else return (MediaClockRef)clock->RetainObject();
}

void MediaPlayerPrepare(MediaPlayerRef ref, int64_t us) {
    Object<IMediaPlayer> mp = ref;
    return mp->prepare(us);
}

void MediaPlayerStart(MediaPlayerRef ref) {
    Object<IMediaPlayer> mp = ref;
    return mp->start();
}

void MediaPlayerPause(MediaPlayerRef ref) {
    Object<IMediaPlayer> mp = ref;
    return mp->pause();
}

MediaOutRef MediaOutCreate(MessageObjectRef format, MessageObjectRef options) {
    Object<MediaOut> out = MediaOut::Create(format, options);
    return (MediaOutRef)out->RetainObject();
}

#if 0
MediaOutRef MediaOutCreateForImage(const ImageFormat * image, MessageObjectRef options) {
    Object<MediaOut> out = MediaOut::Create(kCodecTypeVideo);
    Object<Message> format = new Message;
    format->setInt32(kKeyFormat, image->format);
    format->setInt32(kKeyWidth, image->width);
    format->setInt32(kKeyHeight, image->height);
    if (out->prepare(format, options) != kMediaNoError) {
        return NULL;
    }
    return (MediaOutRef)out->RetainObject();
}
#endif

MediaError MediaOutWrite(MediaOutRef ref, MediaFrameRef frame) {
    Object<MediaOut> out = ref;
    return out->write(frame);
}

MediaError MediaOutFlush(MediaOutRef ref) {
    Object<MediaOut> out = ref;
    return out->flush();
}

MediaError MediaOutConfigure(MediaOutRef ref, MessageObjectRef options) {
    if (options == NULL) return kMediaErrorBadValue;
    Object<MediaOut> out = ref;
    return out->configure(options);
}

int64_t MediaClockGetTime(MediaClockRef ref) {
    Object<Clock> clock = ref;
    return clock->get();
}

__END_DECLS
