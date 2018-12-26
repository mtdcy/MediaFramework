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

#include <MediaToolkit/Toolkit.h>
#include <MediaFramework/MediaDefs.h>
#include <MediaFramework/MediaPacket.h>
#include <MediaFramework/MediaFrame.h>
#include <MediaFramework/MediaDecoder.h>
#include <MediaFramework/MediaOut.h>
#include <MediaFramework/ColorConvertor.h>
#include <MediaFramework/MediaClock.h>

namespace mtdcy {
    /**
     * For pushing packets to target. when a packet is ready,
     * fire this event, and target will get the packet.
     */
    typedef Event<sp<MediaPacket> > PacketReadyEvent;

    /**
     * For pull packets from packet source.
     *
     * @see MediaExtractor::read
     */
    struct PacketRequestPayload {
        eModeReadType           mode;   ///< read mode, @see eModeReadType
        MediaTime               ts;     ///< timestamp in us
        PacketReadyEvent *      event;  ///< @see PacketReadyEvent
    };
    typedef Event<PacketRequestPayload> PacketRequestEvent;

    /**
     * For pushing frames to target. when a frame is ready,
     * fire this event, and target will receive the frame.
     */
    typedef Event<sp<MediaFrame> >  FrameReadyEvent;

    /**
     * For pull frames from frame source
     */
    typedef Event<MediaTime>        RequestFrameEvent;

    /**
     * For update render position to target.
     */
    typedef Event<MediaTime> RenderPositionEvent;

    /**
     * For render frames to external renderer.
     */
    typedef Event<sp<MediaFrame> > RenderEvent;

    /**
     * For handling error or working as async callback
     */
    typedef Event<status_t> StatusEvent;

    /**
     * media session for audio/video/subtile decoding and render.
     * supporting both internal and external renderer.
     * what we do:
     * 1. request packets from source
     * 2. decode packet to frame
     * 3. do format convert if neccessary
     * 4. render at time
     * so, client don't have to care about decode, timing, thread, etc...
     * @note MediaTrack takes one or two threads.
     */
    class MediaSession {
        public:
            /**
             * create a session packet stream.
             * about formats, @see MediaExtractor
             * about options:
             *  "PacketRequestEvent"    - [sp<PacketRequestEvent>]  - mandatory
             *  "RenderPositionEvent"   - [sp<RenderPositionEvent>] - optional
             *  "Clock"                 - [sp<Clock>]               - mandatory     //TODO: make this optional
             *  "RenderEvent"           - [sp<RenderEvent>]         - optional
             *  "SDL_Window"            - [pointer|void *]          - optional
             * if RenderEvent exists, external renderer will be used, and
             * internal renderer will not create.
             * if RenderEvent not exists, internal renderer will be used, and
             * SDL_Window must exists.
             *
             * @param formats   formats of packet stream. @see MediaExtractor
             * @param options   option and parameter for this session, see notes before
             */
            MediaSession(const Message& formats, const Message& options);
            ~MediaSession();

            /**
             * get status of this session
             */
            status_t    status() const;

            /**
             * prepare codec and renderer, and other context. must
             * flush before prepare again.
             * TODO: prepare again without flush
             * @param ts    when we want to prepare
             * @return return OK on success, otherwise error code
             * @note prepare will display the first image of video
             */
            status_t    prepare(const MediaTime& ts, const sp<StatusEvent>& se = NULL);

            /**
             * flush codec and renderer, and reset context.
             * @return return OK on success, otherwise error code
             */
            status_t    flush(const sp<StatusEvent>& se = NULL);

        private:
            struct DecodeSession;
            struct RenderSession;

            eCodecFormat        mID;
            sp<Looper>          mDecodeLooper;
            sp<Looper>          mRenderLooper;
            sp<DecodeSession>   mDecodeSession;
            sp<RenderSession>   mRenderSession;

        private:
            DISALLOW_EVILS(MediaSession);
    };


};

#endif 
