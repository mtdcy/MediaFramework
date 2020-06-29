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
    sp<ABuffer> pipe = ref;
    sp<ImageFile> file = ImageFile::Create();
    if (file->init(pipe, NULL) == kMediaNoError) {
        return file->RetainObject();
    }
    return NULL;
}

MediaFrameRef AudioFrameCreate(const AudioFormat * audio) {
    sp<MediaFrame> frame = MediaFrame::Create(*audio);
    return (MediaFrameRef)frame->RetainObject();
}

MediaFrameRef ImageFrameCreate(const ImageFormat * image) {
    sp<MediaFrame> frame = MediaFrame::Create(*image);
    return frame->RetainObject();
}

MediaFrameRef ImageFrameGenerate(const ImageFormat * image, BufferObjectRef ref) {
    sp<Buffer> buffer = ref;
    sp<MediaFrame> frame = MediaFrame::Create(*image, buffer);
    return frame->RetainObject();
}

uint8_t * MediaFrameGetPlaneData(MediaFrameRef ref, size_t index) {
    sp<MediaFrame> frame = ref;
    CHECK_LT(index, frame->planes.count);
    return frame->planes.buffers[index].data;
}

size_t MediaFrameGetPlaneSize(MediaFrameRef ref, size_t index) {
    sp<MediaFrame> frame = ref;
    CHECK_LT(index, frame->planes.count);
    return frame->planes.buffers[index].size;
}

AudioFormat * MediaFrameGetAudioFormat(MediaFrameRef ref) {
    sp<MediaFrame> frame = ref;
    return &frame->audio;
}

ImageFormat * MediaFrameGetImageFormat(MediaFrameRef ref) {
    sp<MediaFrame> frame = ref;
    return &frame->video;
}

void * MediaFrameGetOpaque(MediaFrameRef ref) {
    sp<MediaFrame> frame = ref;
    return frame->opaque;
}

MediaError ImageFrameSwapCbCr(MediaFrameRef ref) {
    sp<MediaFrame> frame = ref;
    return frame->swapCbCr();
}

MediaError ImageFrameReversePixel(MediaFrameRef ref) {
    sp<MediaFrame> frame = ref;
    return frame->reversePixel();
}

MediaError ImageFramePlanarization(MediaFrameRef ref) {
    sp<MediaFrame> frame = ref;
    //return frame->planarization(); FIXME
}

MediaError ImageFrameToRGB(MediaFrameRef ref) {
    sp<MediaFrame> frame = ref;
    //return frame->yuv2rgb(); FIXME
}

struct UserFrameEvent : public MediaFrameEvent {
    FrameCallback Callback;
    void * opaque;
    
    UserFrameEvent(const sp<Looper>& lp, FrameCallback cb, void * user) :
    MediaFrameEvent(lp), Callback(cb), opaque(user) { }
    
    virtual void onEvent(const sp<MediaFrame>& frame) {
        Callback(frame.get(), opaque);
    }
};

FrameEventRef FrameEventCreate(LooperObjectRef ref, FrameCallback cb, void * user) {
    sp<Looper> lp = ref;
    sp<UserFrameEvent> event = new UserFrameEvent(lp, cb, user);
    return (FrameEventRef)event->RetainObject();
}

struct UserInfoEvent : public PlayerInfoEvent {
    PlayerInfoCallback Callback;
    void * User;
    UserInfoEvent(const sp<Looper>& lp, PlayerInfoCallback cb, void * user) :
    PlayerInfoEvent(lp), Callback(cb), User(user) { }
    
    virtual void onEvent(const ePlayerInfoType& info, const sp<Message>& payload) {
        Callback(info, payload.get(), User);
    }
};

PlayerInfoEventRef PlayerInfoEventCreate(LooperObjectRef ref, PlayerInfoCallback cb, void * user) {
    sp<Looper> lp = ref;
    sp<UserInfoEvent> event = new UserInfoEvent(lp, cb, user);
    return (PlayerInfoEventRef)event->RetainObject();
}

MediaPlayerRef MediaPlayerCreate(MessageObjectRef media, MessageObjectRef options) {
    sp<IMediaPlayer> mp = IMediaPlayer::Create(media, options);
    return mp->RetainObject();
}

MediaClockRef MediaPlayerGetClock(MediaPlayerRef ref) {
    sp<IMediaPlayer> mp = ref;
    sp<Clock> clock = mp->clock();
    if (clock == NULL) return NULL;
    else return (MediaClockRef)clock->RetainObject();
}

void MediaPlayerPrepare(MediaPlayerRef ref, int64_t us) {
    sp<IMediaPlayer> mp = ref;
    return mp->prepare(us);
}

void MediaPlayerStart(MediaPlayerRef ref) {
    sp<IMediaPlayer> mp = ref;
    return mp->start();
}

void MediaPlayerPause(MediaPlayerRef ref) {
    sp<IMediaPlayer> mp = ref;
    return mp->pause();
}

MediaOutRef MediaOutCreate(MessageObjectRef format, MessageObjectRef options) {
    sp<MediaOut> out = MediaOut::Create(format, options);
    return (MediaOutRef)out->RetainObject();
}

#if 0
MediaOutRef MediaOutCreateForImage(const ImageFormat * image, MessageObjectRef options) {
    sp<MediaOut> out = MediaOut::Create(kCodecTypeVideo);
    sp<Message> format = new Message;
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
    sp<MediaOut> out = ref;
    return out->write(frame);
}

MediaError MediaOutFlush(MediaOutRef ref) {
    sp<MediaOut> out = ref;
    return out->flush();
}

MediaError MediaOutConfigure(MediaOutRef ref, MessageObjectRef options) {
    if (options == NULL) return kMediaErrorBadValue;
    sp<MediaOut> out = ref;
    return out->configure(options);
}

int64_t MediaClockGetTime(MediaClockRef ref) {
    sp<Clock> clock = ref;
    return clock->get();
}

__END_DECLS
