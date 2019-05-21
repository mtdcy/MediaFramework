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

#ifndef _MPX_MEDIA_TRACK_H
#define _MPX_MEDIA_TRACK_H

#include <MediaFramework/MediaDefs.h>
#include <MediaFramework/MediaFrame.h>

#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

/**
 * For pushing packets to target. when a packet is ready,
 * fire this event, and target will get the packet.
 */
typedef TypedEvent<sp<MediaPacket> > PacketReadyEvent;

/**
 * For pull packets from packet source.
 */
typedef TypedEvent<sp<PacketReadyEvent> > PacketRequestEvent;

/**
 * For pushing frames to target. when a frame is ready,
 * fire this event, and target will receive the frame.
 */
typedef TypedEvent<sp<MediaFrame> > FrameReadyEvent;

/**
 * For pull frames from frame source
 */
typedef TypedEvent<sp<FrameReadyEvent> > FrameRequestEvent;

/**
 * For MediaSession Info
 */
typedef enum {
    kSessionInfoReady,      ///< we are ready
    kSessionInfoBegin,      ///< begin of stream, before first frame
    kSessionInfoEnd,        ///< end of stream, after last frame
    kSessionInfoError,      ///< we don't need the error code, just release the session after error
} eSessionInfoType;
typedef TypedEvent<eSessionInfoType>    SessionInfoEvent;

struct API_EXPORT IMediaSession : public SharedObject {
    IMediaSession() { }
    virtual ~IMediaSession() { }
    
    /**
     * create a new media session
     * "SessionInfoEvent"   - [sp<SessionInfoEvent>]    - optional
     * @param formats   format of the media stream
     * @param options   options of the media session
     * @return return reference to the media session, or NIL if failed.
     */
    static sp<IMediaSession> create(const sp<Message>& formats, const sp<Message>& options);
    
    /**
     * prepare media session
     */
    virtual void prepare() = 0;
    
    /**
     * flush media session
     */
    virtual void flush() = 0;
    
    /**
     * release media session
     */
    virtual void release() = 0;
};

__END_NAMESPACE_MPX
#endif

#endif // _MPX_MEDIA_TRACK_H
