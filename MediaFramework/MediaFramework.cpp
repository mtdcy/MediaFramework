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

MediaFrameRef MediaFrameCreate(size_t n) {
    sp<MediaFrame> frame = MediaFrame::Create(n);
    if (frame.isNIL()) return NULL;
    return frame->RetainObject();
}

MediaFrameRef MediaFrameCreateWithBuffer(BufferObjectRef ref) {
    sp<Buffer> buffer = ref;
    sp<MediaFrame> frame = MediaFrame::Create(buffer);
    if (frame.isNIL()) return NULL;
    return frame->RetainObject();
}

MediaFrameRef MediaFrameCreateWithAudioFormat(const AudioFormat * audio) {
    sp<MediaFrame> frame = MediaFrame::Create(*audio);
    if (frame.isNIL()) return NULL;
    return frame->RetainObject();
}

MediaFrameRef MediaFrameCreateWithImageFormat(const ImageFormat * image) {
    sp<MediaFrame> frame = MediaFrame::Create(*image);
    if (frame.isNIL()) return NULL;
    return frame->RetainObject();
}

MediaFrameRef MediaFrameCreateWithImageBuffer(const ImageFormat * image, BufferObjectRef ref) {
    sp<Buffer> buffer = ref;
    sp<MediaFrame> frame = MediaFrame::Create(*image, buffer);
    if (frame.isNIL()) return NULL;
    return frame->RetainObject();
}

size_t MediaFrameGetPlaneCount(const MediaFrameRef ref) {
    return static_cast<const MediaFrame*>(ref)->planes.count;
}

size_t MediaFrameGetPlaneSize(const MediaFrameRef ref, size_t index) {
    return static_cast<MediaFrame*>(ref)->planes.buffers[index].size;
}

uint8_t * MediaFrameGetPlaneData(MediaFrameRef ref, size_t index) {
    return static_cast<MediaFrame*>(ref)->planes.buffers[index].data;
}

AudioFormat * MediaFrameGetAudioFormat(MediaFrameRef ref) {
    return &(static_cast<MediaFrame*>(ref)->audio);
}

ImageFormat * MediaFrameGetImageFormat(MediaFrameRef ref) {
    return &(static_cast<MediaFrame*>(ref)->video);
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

MediaDeviceRef MediaDeviceCreate(MessageObjectRef format, MessageObjectRef options) {
    sp<MediaDevice> device = MediaDevice::create(format, options);
    if (device.isNIL()) return NULL;
    return (MediaDeviceRef)device->RetainObject();
}

MessageObjectRef MediaDeviceFormats(MediaDeviceRef ref) {
    sp<MediaDevice> device = ref;
    return device->formats()->RetainObject();
}

MediaFrameRef MediaDevicePull(MediaDeviceRef ref) {
    sp<MediaDevice> device = ref;
    sp<MediaFrame> frame = device->pull();
    if (frame.isNIL()) return NULL;
    return frame->RetainObject();
}

MediaError MediaDevicePush(MediaDeviceRef ref, MediaFrameRef frame) {
    sp<MediaDevice> device = ref;
    return device->push(frame);
}

MediaError MediaDeviceReset(MediaDeviceRef ref) {
    sp<MediaDevice> device = ref;
    return device->reset();
}

MediaError MediaDeviceConfigure(MediaDeviceRef ref, MessageObjectRef options) {
    if (options == NULL) return kMediaErrorBadValue;
    sp<MediaDevice> device = ref;
    return device->configure(options);
}

MediaDeviceRef ColorConverterCreate(const ImageFormat * input, const ImageFormat * output, MessageObjectRef ref) {
    sp<Message> options = ref;
    sp<MediaDevice> cc = CreateColorConverter(*input, *output, options);
    if (cc.isNIL()) return NULL;
    return cc->RetainObject();
}

int64_t MediaClockGetTime(MediaClockRef ref) {
    sp<Clock> clock = ref;
    return clock->get();
}

__END_DECLS
