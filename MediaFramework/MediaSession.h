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

#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

/**
 * For pushing packets to target. when a packet is ready,
 * fire this event, and target will get the packet.
 */
typedef TypedEvent<sp<MediaPacket> > PacketReadyEvent;

/**
 * For pull packets from packet source.
 *
 * @see MediaExtractor::read
 */
struct PacketRequest {
    eModeReadType           mode;   ///< read mode, @see eModeReadType
    MediaTime               ts;     ///< timestamp of the packet
    sp<PacketReadyEvent>    event;  ///< event for return the packet, @see PacketReadyEvent
};
typedef TypedEvent<PacketRequest> PacketRequestEvent;

/**
 * For pushing frames to target. when a frame is ready,
 * fire this event, and target will receive the frame.
 */
typedef TypedEvent<sp<MediaFrame> >  FrameReadyEvent;

/**
 * For pull frames from frame source
 */
struct FrameRequest {
    MediaTime               ts;     ///< timestamp of the start frame
    sp<FrameReadyEvent>     event;  ///< event for return the frame, @see FrameReadyEvent
};
typedef TypedEvent<FrameRequest>  FrameRequestEvent;

/**
 * For handle error status
 */
typedef TypedEvent<MediaError> StatusEvent;

struct API_EXPORT IMediaSession : public SharedObject {
    IMediaSession() { }
    virtual ~IMediaSession() { }
    
    static sp<IMediaSession> Create(const Message& formats, const Message& options);
    virtual void prepare(const Message& options) = 0;
    virtual void flush() = 0;
    virtual void release() = 0;
};

__END_NAMESPACE_MPX
#endif

#endif // _MPX_MEDIA_TRACK_H
