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

USING_NAMESPACE_MFWK

MediaFrameRef MediaFrameCreate(UInt32 n) {
    sp<MediaFrame> frame = MediaFrame::Create(n);
    if (frame.isNil()) return Nil;
    return frame->RetainObject();
}

MediaFrameRef MediaFrameCreateWithBuffer(BufferObjectRef ref) {
    sp<Buffer> buffer = static_cast<Buffer *>(ref);
    sp<MediaFrame> frame = MediaFrame::Create(buffer);
    if (frame.isNil()) return Nil;
    return frame->RetainObject();
}

MediaFrameRef MediaFrameCreateWithAudioFormat(const AudioFormat * audio) {
    sp<MediaFrame> frame = MediaFrame::Create(*audio);
    if (frame.isNil()) return Nil;
    return frame->RetainObject();
}

MediaFrameRef MediaFrameCreateWithImageFormat(const ImageFormat * image) {
    sp<MediaFrame> frame = MediaFrame::Create(*image);
    if (frame.isNil()) return Nil;
    return frame->RetainObject();
}

MediaFrameRef MediaFrameCreateWithImageBuffer(const ImageFormat * image, BufferObjectRef ref) {
    sp<Buffer> buffer = static_cast<Buffer *>(ref);
    sp<MediaFrame> frame = MediaFrame::Create(*image, buffer);
    if (frame.isNil()) return Nil;
    return frame->RetainObject();
}

UInt32 MediaFrameGetPlaneCount(const MediaFrameRef ref) {
    return static_cast<const MediaFrame*>(ref)->planes.count;
}

UInt32 MediaFrameGetPlaneSize(const MediaFrameRef ref, UInt32 index) {
    return static_cast<MediaFrame*>(ref)->planes.buffers[index].size;
}

UInt8 * MediaFrameGetPlaneData(MediaFrameRef ref, UInt32 index) {
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
    sp<Looper> lp = static_cast<Looper *>(ref);
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
    sp<Looper> lp = static_cast<Looper *>(ref);
    sp<UserInfoEvent> event = new UserInfoEvent(lp, cb, user);
    return (PlayerInfoEventRef)event->RetainObject();
}

MediaPlayerRef MediaPlayerCreate(MessageObjectRef media, MessageObjectRef options) {
    sp<IMediaPlayer> mp = IMediaPlayer::Create(static_cast<Message *>(media),
                                               static_cast<Message *>(options));
    return mp->RetainObject();
}

MediaClockRef MediaPlayerGetClock(MediaPlayerRef ref) {
    sp<Clock> clock = static_cast<IMediaPlayer *>(ref)->clock();
    if (clock == Nil) return Nil;
    else return (MediaClockRef)clock->RetainObject();
}

void MediaPlayerPrepare(MediaPlayerRef ref, Int64 us) {
    return static_cast<IMediaPlayer *>(ref)->prepare(Time::MicroSeconds(us));
}

void MediaPlayerStart(MediaPlayerRef ref) {
    return static_cast<IMediaPlayer *>(ref)->start();
}

void MediaPlayerPause(MediaPlayerRef ref) {
    return static_cast<IMediaPlayer *>(ref)->pause();
}

MediaDeviceRef MediaDeviceCreate(MessageObjectRef format, MessageObjectRef options) {
    sp<MediaDevice> device = MediaDevice::create(static_cast<Message *>(format),
                                                 static_cast<Message *>(options));
    if (device.isNil()) return Nil;
    return (MediaDeviceRef)device->RetainObject();
}

MessageObjectRef MediaDeviceFormats(MediaDeviceRef ref) {
    return static_cast<MediaDevice *>(ref)->formats()->RetainObject();
}

MediaFrameRef MediaDevicePull(MediaDeviceRef ref) {
    sp<MediaFrame> frame = static_cast<MediaDevice *>(ref)->pull();
    if (frame.isNil()) return Nil;
    return frame->RetainObject();
}

MediaError MediaDevicePush(MediaDeviceRef ref, MediaFrameRef frame) {
    return static_cast<MediaDevice *>(ref)->push(static_cast<MediaFrame *>(frame));
}

MediaError MediaDeviceReset(MediaDeviceRef ref) {
    return static_cast<MediaDevice *>(ref)->reset();
}

MediaError MediaDeviceConfigure(MediaDeviceRef ref, MessageObjectRef options) {
    if (options == Nil) return kMediaErrorBadValue;
    return static_cast<MediaDevice *>(ref)->configure(static_cast<Message *>(options));
}

MediaDeviceRef ColorConverterCreate(const ImageFormat * input, const ImageFormat * output, MessageObjectRef options) {
    sp<MediaDevice> cc = CreateColorConverter(*input, *output,
                                              static_cast<Message *>(options));
    if (cc.isNil()) return Nil;
    return cc->RetainObject();
}

Int64 MediaClockGetTime(MediaClockRef ref) {
    return static_cast<Clock *>(ref)->get().useconds();
}

__END_DECLS
