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

#include <MediaFramework/MediaDefs.h>

__BEGIN_DECLS

typedef enum {
    kStateInvalid,      ///< mp just allocated
    kStateInitial,      ///< mp initialized with media
    kStateReady,        ///< mp ready to play
    kStatePlaying,      ///< mp is playing
    kStateIdle,         ///< mp is not playing
    kStateFlushed,      ///< mp context is reset, can be prepare again
    kStateReleased,     ///< mp context is released
    kStateMax,
} eStateType;

__END_DECLS

#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

/**
 * for MediaPlayer streaming MediaFrame to client
 */
typedef TypedEvent<Object<MediaFrame> >     MediaFrameEvent;

/**
 * For update render position to target.
 */
typedef TypedEvent<MediaTime>               RenderPositionEvent;

struct API_EXPORT IMediaPlayer : public SharedObject {
    IMediaPlayer() { }
    virtual ~IMediaPlayer() { }

    /**
     * create a player with options
     * about options:
     *  "StatusEvent"           - [sp<StatusEvent>]         - optional
     *  "RenderPositionEvent"   - [sp<RenderPositionEvent>] - optional
     * @param options   option and parameter for player
     */
    static sp<IMediaPlayer> Create(const Message& options);

    /**
     *
     */
    virtual eStateType state() const = 0;

    /**
     * add a media to player.
     * about options:
     *  "url"                   - [String]                  - mandatory, url of the media
     *  "MediaOut"              - [sp<MediaOut>]            - optional
     *  "StartTime"             - [double|seconds]          - optional
     *  "EndTime"               - [double|seconds]          - optional
     * if MediaOut exists, external renderer will be used.
     * @param media option and parameter for this media
     * @return return OK on success, otherwise error code
     */
    virtual MediaError init(const Message& media) = 0;
    /**
     * prepare player after add media
     * @return return OK on success, otherwise error code
     */
    virtual MediaError prepare(const MediaTime& ts) = 0;
    /**
     * start this player.
     * @return return OK on success, otherwise error code.
     * @note there must be at least one media exists and start success.
     */
    virtual MediaError start() = 0;
    /**
     * pause this player.
     * @return return OK on success, otherwise error code.
     */
    virtual MediaError pause() = 0;
    /**
     * flush this player
     */
    virtual MediaError flush() = 0;
    /**
     * release this player
     */
    virtual MediaError release() = 0;
};
__END_NAMESPACE_MPX
#endif

#endif // _MEDIA_PLAYER_H

