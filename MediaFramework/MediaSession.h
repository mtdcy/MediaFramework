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


/**
 * File:    MediaSession.h
 * Author:  mtdcy.chen
 * Changes:
 *          1. 20181214     initial version
 *
 */

#ifndef _MPX_MEDIA_SESSION_H
#define _MPX_MEDIA_SESSION_H

#include <MediaFramework/MediaDefs.h>
#include <MediaFramework/MediaFrame.h>

#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

/**
 * For choose tracks
 */
typedef MediaEvent<size_t> TrackSelectEvent;

/**
 * For pushing packets to target. when a packet is ready,
 * fire this event, and target will get the packet.
 */
typedef MediaEvent<sp<MediaPacket> > PacketReadyEvent;

/**
 * For pull packets from packet source.
 */
typedef MediaEvent2<sp<PacketReadyEvent>, MediaTime> PacketRequestEvent;

/**
 * For pushing frames to target. when a frame is ready,
 * fire this event, and target will receive the frame.
 */
typedef MediaEvent<sp<MediaFrame> > FrameReadyEvent;

/**
 * For pull frames from frame source
 */
typedef MediaEvent2<sp<FrameReadyEvent>, MediaTime> FrameRequestEvent;

typedef enum {
    kSessionInfoReady,
    kSessionInfoEnd,
    kSessionInfoError,
} eSessionInfoType;

typedef MediaEvent2<eSessionInfoType, sp<Message> > SessionInfoEvent;

class API_EXPORT IMediaSession : public SharedObject {
    public:
        static Object<IMediaSession> Create(const sp<Message>&, const sp<Message>&);
    
    public:
        IMediaSession(const sp<Looper>& lp) : mDispatch(new DispatchQueue(lp)) { }

        virtual ~IMediaSession() { }

    protected:
        virtual void onFirstRetain();   //> onInit()
        virtual void onLastRetain();    //> onRelease()

        // these routine always run inside looper
        struct InitJob;
        struct ReleaseJob;
        virtual void onInit() = 0;
        virtual void onRelease() = 0;

        sp<DispatchQueue> mDispatch;

        DISALLOW_EVILS(IMediaSession);
};

__END_NAMESPACE_MPX
#endif // __cplusplus

#endif // _MPX_MEDIA_SESSION_H
