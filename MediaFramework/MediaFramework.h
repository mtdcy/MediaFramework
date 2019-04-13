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
#include <MediaFramework/MediaDefs.h>
#include <MediaFramework/MediaExtractor.h>
#include <MediaFramework/MediaDecoder.h>
#include <MediaFramework/MediaOut.h>
#include <MediaFramework/ColorConvertor.h>
#include <MediaFramework/MediaClock.h>
#include <MediaFramework/MediaSession.h>
#include <MediaFramework/MediaPlayer.h>

__BEGIN_DECLS

// MediaFrame
typedef SharedObjectRef         MediaFrameRef;

API_EXPORT MediaFrameRef        AudioFrameCreate(const AudioFormat *);
API_EXPORT MediaFrameRef        ImageFrameCreate(const ImageFormat *);
#define MediaFrameRelease(x)    SharedObjectRelease((SharedObjectRef)x)

API_EXPORT uint8_t *            MediaFrameGetPlaneData(MediaFrameRef, size_t);
API_EXPORT size_t               MediaFrameGetPlaneSize(MediaFrameRef, size_t);

API_EXPORT AudioFormat *        MediaFrameGetAudioFormat(MediaFrameRef);
API_EXPORT ImageFormat *        MediaFrameGetImageFormat(MediaFrameRef);

API_EXPORT void *               MediaFrameGetOpaque(MediaFrameRef);

// Clock, get a clock from MediaPlayerRef, used to update ui
typedef SharedObjectRef         MediaClockRef;
#define MediaClockRelease(x)    SharedObjectRelease((SharedObjectRef)x)

API_EXPORT int64_t              MediaClockGetTime(MediaClockRef);

// MediaPlayer
typedef SharedObjectRef         MediaPlayerRef;

API_EXPORT MediaPlayerRef       MediaPlayerCreate(MessageRef, MessageRef);
API_EXPORT MediaError           MediaPlayerRelease(MediaPlayerRef);

/**
 * @note remember to release after use
 */
API_EXPORT MediaClockRef        MediaPlayerGetClock(const MediaPlayerRef);
API_EXPORT MessageRef           MediaPlayerGetInfo(const MediaPlayerRef);

API_EXPORT MediaError           MediaPlayerPrepare(MediaPlayerRef, int64_t);
API_EXPORT MediaError           MediaPlayerStart(MediaPlayerRef);
API_EXPORT MediaError           MediaPlayerPause(MediaPlayerRef);
API_EXPORT MediaError           MediaPlayerFlush(MediaPlayerRef);
API_EXPORT eStateType           MediaPlayerGetState(const MediaPlayerRef);

// Events
typedef SharedObjectRef         FrameEventRef;
typedef SharedObjectRef         PositionEventRef;

API_EXPORT FrameEventRef        FrameEventCreate(void (*Callback)(MediaFrameRef, void *), void *);
#define FrameEventRelease(x)    SharedObjectRelease((SharedObjectRef)x)

API_EXPORT PositionEventRef     PositionEventCreate(void (*Callback)(int64_t, void *), void *);
#define PositionEventRelease(x) SharedObjectRelease((SharedObjectRef)x)

// MediaOut
typedef SharedObjectRef         MediaOutRef;

API_EXPORT MediaOutRef          MediaOutCreate(eCodecType);
#define MediaOutRelease(x)      SharedObjectRelease((SharedObjectRef)x)

API_EXPORT MediaError           MediaOutPrepare(MediaOutRef, MessageRef, MessageRef);
API_EXPORT MediaError           MediaOutWrite(MediaOutRef, MediaFrameRef);
API_EXPORT MediaError           MediaOutFlush(MediaOutRef);

__END_DECLS

#endif // _MEDIA_MODULES_ALL_H
