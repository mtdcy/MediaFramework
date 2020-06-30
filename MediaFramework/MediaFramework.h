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


// File:    MediaFramework.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_MODULES_ALL_H
#define _MEDIA_MODULES_ALL_H

// MediaFramework
#include <MediaFramework/MediaTypes.h>
#include <MediaFramework/MediaDevice.h>
#include <MediaFramework/MediaClock.h>
#include <MediaFramework/MediaSession.h>
#include <MediaFramework/MediaPlayer.h>
#include <MediaFramework/ImageFile.h>

//#ifndef __cplusplus
__BEGIN_DECLS

// ImageFile
typedef SharedObjectRef         ImageFileRef;
API_EXPORT ImageFileRef         ImageFileOpen(ContentObjectRef);

// MediaFrame
typedef SharedObjectRef         MediaFrameRef;

API_EXPORT MediaFrameRef        AudioFrameCreate(const AudioFormat *);
API_EXPORT MediaFrameRef        ImageFrameCreate(const ImageFormat *);
#define MediaFrameRelease(x)    SharedObjectRelease((SharedObjectRef)x)

API_EXPORT MediaFrameRef        ImageFrameGenerate(const ImageFormat *, BufferObjectRef);

API_EXPORT uint8_t *            MediaFrameGetPlaneData(MediaFrameRef, size_t);
API_EXPORT size_t               MediaFrameGetPlaneSize(MediaFrameRef, size_t);

API_EXPORT AudioFormat *        MediaFrameGetAudioFormat(MediaFrameRef);
API_EXPORT ImageFormat *        MediaFrameGetImageFormat(MediaFrameRef);

API_EXPORT void *               MediaFrameGetOpaque(MediaFrameRef);

API_EXPORT MediaError           ImageFrameSwapCbCr(MediaFrameRef);

API_EXPORT MediaError           ImageFrameReversePixel(MediaFrameRef);

API_EXPORT MediaError           ImageFramePlanarization(MediaFrameRef);

API_EXPORT MediaError           ImageFrameToRGB(MediaFrameRef);

// Clock, get a clock from MediaPlayerRef, used to update ui
typedef SharedObjectRef         MediaClockRef;
#define MediaClockRelease(x)    SharedObjectRelease((SharedObjectRef)x)

API_EXPORT int64_t              MediaClockGetTime(MediaClockRef);

// MediaPlayer
typedef SharedObjectRef         MediaPlayerRef;

API_EXPORT MediaPlayerRef       MediaPlayerCreate(MessageObjectRef, MessageObjectRef);

/**
 * @note remember to release after use
 */
API_EXPORT MediaClockRef        MediaPlayerGetClock(const MediaPlayerRef);

API_EXPORT void                 MediaPlayerPrepare(MediaPlayerRef, int64_t);
API_EXPORT void                 MediaPlayerStart(MediaPlayerRef);
API_EXPORT void                 MediaPlayerPause(MediaPlayerRef);

// Events
typedef SharedObjectRef         FrameEventRef;
typedef SharedObjectRef         PlayerInfoEventRef;

typedef void (*FrameCallback)(MediaFrameRef, void *);

API_EXPORT FrameEventRef        FrameEventCreate(LooperObjectRef, void (*Callback)(MediaFrameRef, void *), void *);
#define FrameEventRelease(x)    SharedObjectRelease((SharedObjectRef)x)

typedef void (*PlayerInfoCallback)(ePlayerInfoType, const MessageObjectRef, void *);
API_EXPORT PlayerInfoEventRef   PlayerInfoEventCreate(LooperObjectRef, PlayerInfoCallback, void *);
#define PlayerInfoEventRelease(x) SharedObjectRelease((SharedObjectRef)x)

// MediaDevice
typedef SharedObjectRef         MediaDeviceRef;

API_EXPORT MediaDeviceRef       MediaDeviceCreate(MessageObjectRef, MessageObjectRef);
//API_EXPORT MediaOutRef          MediaOutCreateForImage(const ImageFormat *, MessageObjectRef);
#define MediaDeviceRelease(x)   SharedObjectRelease((SharedObjectRef)x)

API_EXPORT MessageObjectRef     MediaDeviceFormats(MediaDeviceRef);
API_EXPORT MediaError           MediaDeviceConfigure(MediaDeviceRef, MessageObjectRef);
API_EXPORT MediaError           MediaDevicePush(MediaDeviceRef, MediaFrameRef);
API_EXPORT MediaFrameRef        MediaDevicePull(MediaDeviceRef);
API_EXPORT MediaError           MediaDeviceReset(MediaDeviceRef);

__END_DECLS
//#endif // __cplusplus

#endif // _MEDIA_MODULES_ALL_H
