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


// File:    MediaDefs.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#ifndef _MEDIA_MODULES_CODEC_H
#define _MEDIA_MODULES_CODEC_H

#include <MediaFramework/MediaDefs.h>

__BEGIN_DECLS

enum eModeType {
    kModeTypeNormal     = 0,                ///< use hardware accel if available
    kModeTypeSoftware,                      ///< no hardware accel.
    kModeTypePreview,
    kModeTypeDefault    = kModeTypeNormal
};

__END_DECLS

#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

/**
 * base class for audio/video codecs
 */
struct API_EXPORT MediaDecoder : public SharedObject {
    MediaDecoder() { }
    virtual ~MediaDecoder() { }

    /**
     * allocate a codec object
     * @param format    @see eCodecFormat
     * @param mode      @see eModeType
     * @return return reference to new codec if supported. otherwise return NULL.
     */
    static sp<MediaDecoder> Create(eCodecFormat format, eModeType mode);

    /**
     * get information of this codec.
     * @return return a string of information
     */
    virtual String          string() const = 0;
    /**
     * get output format information of this codec.
     * about the output format:
     *  kKeyFormat      - [ePixelFormat/eSampleFormat]  - mandatory
     * for audio codec:
     *  kKeySampleRate  - [int32_t]                     - mandatory
     *  kKeyChannels    - [int32_t]                     - mandatory
     * for video codec:
     *  kKeyWidth       - [int32_t]                     - mandatory
     *  kKeyHeight      - [int32_t]                     - mandatory
     * @return return Message contains output format information
     */
    virtual Message         formats() const = 0;
    /**
     * initial codec object with format and options
     * @param format    stream format
     * @param options   option and parameter for initial the object
     * @return return kMediaNoError on success.
     */
    virtual MediaError      init(const Message& format, const Message& options) = 0;
    /**
     * push MediaPacket to codec in decoding order.
     * @param input     reference of MediaPacket
     * @return return kMediaNoError on success.
     *         return kMediaErrorResourceBusy if input is full,
     *         otherwise return error code.
     * @note push a NULL packet to notify codec of eos
     */
    virtual MediaError      write(const sp<MediaPacket>& input) = 0;
    /**
     * pull MediaFrame from codec in presentation order.
     * @return  return reference of new MediaFrame.
     * @note if no packet ready or eos, NULL frame will be returned.
     */
    virtual sp<MediaFrame>  read() = 0;
    /**
     * flush context and delayed frame
     * @return return kMediaNoError on success, otherwise error code
     */
    virtual MediaError      flush() = 0;
    /**
     * configure this codec
     * @param options   option and parameter
     * @return return kMediaNoError on success, otherwise error code.
     */
    virtual MediaError      configure(const Message& options) { return kMediaErrorNotSupported; }
};


__END_NAMESPACE_MPX
#endif

#endif
