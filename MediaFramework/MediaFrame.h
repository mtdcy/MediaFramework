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


// File:    MediaFrame.h
// Author:  mtdcy.chen
// Changes:
//          1. 20200630     initial version
//

#ifndef _MEDIA_MODULES_FRAME_H
#define _MEDIA_MODULES_FRAME_H

#include <MediaFramework/MediaTypes.h>

__BEGIN_DECLS

typedef struct MediaBuffer {
    size_t          capacity;           ///< max number bytes in data, readonly
    size_t          size;               ///< number bytes polluted in data, read & write
    uint8_t *       data;               ///< pointer to memory, read & write
#ifdef __cplusplus
    MediaBuffer() : capacity(0), size(0), data(NULL) { }
#endif
} MediaBuffer;

typedef struct MediaBufferList {
    size_t          count;              ///< number buffer in list
    MediaBuffer     buffers[1];         ///< a variable length array with min length = 1
#ifdef __cplusplus
    MediaBufferList() : count(1) { }
#endif
} MediaBufferList;

__END_DECLS

#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

API_EXPORT String   GetMediaBufferListString(const MediaBufferList&);

/**
 * @note MediaFrame MUST be in decoding order for compressed frame.
 * @note MediaFrame MUST be in presentation order for uncompressed frame.
 * @note NO dts field, the codec just need to decoding frames one by one, dts is useless.
 */
struct API_EXPORT MediaFrame : public SharedObject {
    uint32_t            id;             ///< frame id, can be track index or frame index
    uint32_t            flags;          ///< frame flags, @see eFrameType
    MediaTime           timecode;       ///< frame presentation time. no dts field.
    MediaTime           duration;       ///< frame duration
    union {
        uint32_t        format;
        AudioFormat     audio;          ///< audio format
        ImageFormat     video;          ///< video format
        ImageFormat     image;          ///< image format
    };
    void *              opaque;         ///< invisible, for special purposes
    MediaBufferList     planes;         ///< this SHOULD be the last data member

    /**
     * create a media frame with underlying buffer
     * the underlying buffer is always continues, a single buffer for all planes
     */
    static sp<MediaFrame>   Create(size_t);                             ///< create a one plane frame with n bytes underlying buffer
    static sp<MediaFrame>   Create(sp<Buffer>&);                        ///< create a one plane frame with Buffer
    static sp<MediaFrame>   Create(const AudioFormat&);                 ///< create a audio frame
    static sp<MediaFrame>   Create(const ImageFormat&);                 ///< create a video/image frame
    static sp<MediaFrame>   Create(const ImageFormat&, sp<Buffer>&);    ///< create a video/image frame with Buffer
    
    // DEBUGGING: get a human readable string
    virtual String          string() const;

    /** features below is not designed for realtime playback **/

    /**
     * read backend buffer of hwaccel frame
     * @return should return NULL if plane is not exists
     * @note default implementation: read directly from planes
     */
    virtual sp<ABuffer> readPlane(size_t) const;
    
    protected:
    MediaFrame();
    virtual ~MediaFrame() { }
    DISALLOW_EVILS(MediaFrame);
};

__END_NAMESPACE_MPX
#endif // __cplusplus

#endif // _MEDIA_MODULES_FRAME_H
