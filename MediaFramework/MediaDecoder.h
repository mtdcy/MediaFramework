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

#include <MediaToolkit/Toolkit.h>
#include <MediaFramework/MediaPacket.h>
#include <MediaFramework/MediaFrame.h>

__BEGIN_DECLS

__END_DECLS

#ifdef __cplusplus
namespace mtdcy {

    /**
     * base class for audio/video codecs
     */
    class MediaDecoder {
    public:
        /**
         * create a codec based on provided formats.
         * @param formats   @see MediaExtractor::formats()
         * @return return reference to new codec if supported and
         *         initilized successful. otherwise return NULL.
         */
        static sp<MediaDecoder> Create(const Message& formats);
        virtual ~MediaDecoder() { }
        
    public:
        /**
         * get information of this codec.
         * @return return a string of information
         */
        virtual String          string() const = 0;
        /**
         * get status of this codec.
         * @return return OK if everything is OK, otherwise error code
         */
        virtual status_t        status() const = 0;
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
         * configure this codec
         * @param options   option and parameter
         * @return return OK on success, otherwise error code.
         */
        virtual status_t        configure(const Message& options) = 0;
        /**
         * push MediaPacket to codec in decoding order.
         * @param input     reference of MediaPacket
         * @return return OK on success, return TRY_AGAIN if input is full,
         *         otherwise return error code. if TRY_AGAIN, read from
         *         codec before write new packets.
         * @note push a NULL packet to notify codec of eos
         */
        virtual status_t        write(const sp<MediaPacket>& input) = 0;
        /**
         * pull MediaFrame from codec in presentation order.
         * @return  return reference of new MediaFrame.
         * @note if no packet ready or eos, NULL frame will be returned.
         */
        virtual sp<MediaFrame>  read() = 0;
        /**
         * flush context and delayed frame
         * @return return OK on success, otherwise error code
         */
        virtual status_t        flush() = 0;
        
    protected:
        MediaDecoder() { }
        
    private:
        DISALLOW_EVILS(MediaDecoder);
    };


};
#endif

#endif
