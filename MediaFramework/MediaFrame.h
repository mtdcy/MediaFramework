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
//          1. 20160701     initial version
//

#ifndef _MPX_MEDIA_FRAME_H
#define _MPX_MEDIA_FRAME_H

#include <MediaFramework/MediaDefs.h>

__BEGIN_NAMESPACE_MPX

/**
 * media frame structure for decompressed audio and video frames
 * the properties inside this structure have to make sure this
 * frame can be renderred properly without additional informations.
 */
#define MEDIA_FRAME_NB_PLANES   (8)
struct API_EXPORT MediaFrame : public SharedObject {
    MediaTime               pts;        ///< display time in us
    MediaTime               duration;   ///< duration of this frame
    /**
     * plane data struct.
     * for planar frame, multi planes must exist. the backend memory may be
     * or may not be continueslly.
     * for packed frame, only one plane exists.
     */
    struct {
        uint8_t *           data;       ///< plane data
        size_t              size;       ///< data size in bytes
    } planes[MEDIA_FRAME_NB_PLANES];    ///< for packed frame, only one plane exists
    
    union {
        int32_t             format;     ///< sample format, @see ePixelFormat, @see eSampleFormat
        AudioFormat         a;
        ImageFormat         v;
    };
    void                    *opaque;    ///< opaque
    
    /**
     * create a media frame backend by Buffer
     */
    static sp<MediaFrame>   Create(const ImageFormat&);
    static sp<MediaFrame>   Create(const ImageFormat&, const sp<Buffer>&);
    static sp<MediaFrame>   Create(const AudioFormat&);
    
    /**
     * read backend buffer of hwaccel frame
     * @return should return NULL if plane is not exists
     * @note default implementation: read directly from planes
     */
    virtual sp<Buffer> getData(size_t) const;
    
    /**
     * keep luma component and swap two chroma components of YUV image
     * @return return kMediaErrorInvalidOperation if source is not YUV
     * @return return kMediaErrorNotSupported if no implementation
     */
    virtual MediaError swapUVChroma();
    
    /**
     * reverse pixel bytes, like rgba -> abgr
     * @return return kMediaErrorInvalidOperation if source is planar
     * @return return kMediaErrorNotSupported if no implementation
     */
    virtual MediaError reverseBytes();
    
    /**
     * convert to planar pixel format
     * @return return kMediaNoError on success or source is planar
     * @return return kMediaErrorNotSupported if no implementation
     * @note planarization may or may NOT be in place convert
     */
    virtual MediaError planarization();
    
    /**
     * convert pixel format
     * @return return NULL if not supported or no implementation
     * @note the resolution and display rect remains
     */
    virtual sp<MediaFrame> convert(const ePixelFormat&) { return NULL; }    // TODO
    
protected:
    MediaFrame();
    virtual ~MediaFrame() { }
    sp<Buffer>  mBuffer;
};

// AudioFormat
API_EXPORT String   GetAudioFormatString(const AudioFormat& format);

// get MediaFrame human readable string, for debug
API_EXPORT String   GetAudioFrameString(const sp<MediaFrame>&);
API_EXPORT String   GetImageFrameString(const sp<MediaFrame>&);

__END_NAMESPACE_MPX

#endif // _MPX_MEDIA_FRAME_H
