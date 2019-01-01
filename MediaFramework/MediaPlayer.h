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
 * File:    mpx.h 
 * Author:  mtdcy.chen
 * Changes: 
 *          1. 20181214     initial version
 *
 */

#ifndef _MEDIA_PLAYER_H
#define _MEDIA_PLAYER_H

#include <MediaToolkit/Toolkit.h>
#include <MediaFramework/MediaClock.h>
#include <MediaFramework/MediaSession.h>

namespace mtdcy {
    struct MPContext;
    class MediaPlayer {
        public:
            virtual ~MediaPlayer() { }

            /**
             * create a player with options
             * about options:
             *  "StatusEvent"           - [sp<StatusEvent>]         - optional
             *  "RenderPositionEvent"   - [sp<RenderPositionEvent>] - optional
             * @param options   option and parameter for player
             */
            static sp<MediaPlayer> Create(const Message& options);

            /**
             * add a media to player.
             * about options:
             *  "RenderEvent"   - [sp<RenderEvent>]     - optional
             *  "SDL_Window"    - [pointer|void *]      - optional
             *  "StartTime"     - [double|seconds]      - optional
             *  "EndTime"       - [double|seconds]      - optional
             * if RenderEvent exists, external renderer will be used.
             * if RenderEvent not exists, SDL_Window must exists for
             * init internal renderer.
             * @param url   url of the media
             * @param options option and parameter for this media
             * @return return OK on success, otherwise error code
             */
            status_t addMedia(const String& url, const Message& options);
            /**
             * prepare player after add media
             * @return return OK on success, otherwise error code
             */
            status_t prepare(const MediaTime& ts);
            /**
             * start this player.
             * @return return OK on success, otherwise error code.
             * @note there must be at least one media exists and start success.
             */
            status_t start();
            /**
             * pause this player.
             * @return return OK on success, otherwise error code.
             */
            status_t pause();
            /**
             * stop this player.
             * @return return OK on success, otherwise error code.
             */
            status_t stop();
            /**
             * reset this player.
             * @return return OK on success, otherwise error code.
             */
            status_t reset();

        private:
            sp<MPContext>   mContext;
        
        private:
            MediaPlayer() { }
            DISALLOW_EVILS(MediaPlayer);
    };
}

#endif // _MEDIA_PLAYER_H
