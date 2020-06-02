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

#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

/**
 * media frame structure for decompressed audio and video frames
 * the properties inside this structure have to make sure this
 * frame can be renderred properly without additional informations.
 */
#define MEDIA_FRAME_NB_PLANES   (8)
struct API_EXPORT MediaFrame : public SharedObject {
    MediaTime               timecode;   ///< frame display timestamp
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
    
    /** features below is not designed for realtime playback **/
    
    /**
     * read backend buffer of hwaccel frame
     * @return should return NULL if plane is not exists
     * @note default implementation: read directly from planes
     */
    virtual sp<Buffer> readPlane(size_t) const;
    
    /**
     * keep luma component and swap two chroma components of Y'CbCr image
     * @return return kMediaErrorInvalidOperation if source is not Y'CbCr
     * @return return kMediaErrorNotSupported if no implementation
     */
    virtual MediaError swapCbCr();
    
    /**
     * convert pixel bytes-order <-> word-order, like rgba -> abgr
     * @return return kMediaErrorInvalidOperation if source is planar
     * @return return kMediaErrorNotSupported if no implementation
     */
    virtual MediaError reversePixel();
    
    /**
     * convert to planar pixel format
     * @return return kMediaNoError on success or source is planar
     * @return return kMediaErrorNotSupported if no implementation
     * @note planarization may or may NOT be in place convert
     * @note target pixel format is variant based on the implementation
     */
    virtual MediaError planarization();
    
    /**
     * convert yuv -> rgb
     * @return return kMediaErrorInvalidOperation if source is rgb or target is not rgb
     * @return return kMediaErrorNotSupported if no implementation
     * @return target pixel is rgba by default, but no guarentee.
     */
    enum eConversion { kBT601, kBT709, kJFIF };
    virtual MediaError yuv2rgb(const ePixelFormat& = kPixelFormatRGB32, const eConversion& = kBT601);
    
    /**
     * rotate image
     * @return kMediaErrorNotSupported if no implementation
     */
    enum eRotation { kRotate0, kRotate90, kRotate180, kRotate270 };
    virtual MediaError rotate(const eRotation&) { return kMediaErrorNotSupported; }
    
protected:
    MediaFrame();
    virtual ~MediaFrame() { }
    sp<Buffer>  mBuffer;
};

// ePixelFormat
API_EXPORT String   GetPixelFormatString(const ePixelFormat&);
API_EXPORT String   GetImageFormatString(const ImageFormat&);
API_EXPORT String   GetImageFrameString(const sp<MediaFrame>&);
API_EXPORT size_t   GetImageFormatPlaneLength(const ImageFormat&, size_t);
API_EXPORT size_t   GetImageFormatBufferLength(const ImageFormat& image);

// AudioFormat
API_EXPORT String   GetAudioFormatString(const AudioFormat&);

// get MediaFrame human readable string, for debug
API_EXPORT String   GetAudioFrameString(const sp<MediaFrame>&);

__END_NAMESPACE_MPX
#endif

#endif // _MPX_MEDIA_FRAME_H
