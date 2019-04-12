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

#ifndef _MEDIA_MODULES_EXTRACTOR_H
#define _MEDIA_MODULES_EXTRACTOR_H

#include <MediaFramework/MediaDefs.h>

#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

/**
 * detect file format
 * @param url   url to the content
 * @param pipe  content pipe
 * @return return file format @see eFileFormat
 */
eFileFormat MediaFormatDetect(const String& url);
eFileFormat MediaFormatDetect(Content& pipe);

/**
 * base class for different files
 */
struct API_EXPORT MediaExtractor : public SharedObject {
    MediaExtractor() { }
    virtual ~MediaExtractor() { }

    /**
     * allocate an extractor object
     * @return return reference to new extractor
     */
    static sp<MediaExtractor> Create(eFileFormat);

    virtual MediaError      init(sp<Content>& pipe, const Message& options) = 0;
    /**
     * get information of this extractor.
     * @return return a string of information
     */
    virtual String          string() const = 0;
    /**
     * configure this codec
     * @param options   option and parameter
     * @return return OK on success, otherwise error code.
     */
    virtual MediaError      configure(const Message& options) { return kMediaErrorNotSupported; }
    /**
     * get output format information of this codec.
     * about the output format:
     *  kKeyFormat      - [eFileFormat]     - mandatory, file format
     *  kKeyDuration    - [MediaTime]       - mandatory, file duration
     *  kKeyCount       - [int32_t]         - mandatory,
     *  "track-%zu"     - [Message]         - mandatory
     * about the track output format:
     *  kKeyFormat      - [eCodecFormat]    - mandatory
     *  kKeyType        - [eCodecType]      - mandatory
     *  kKeyDuration    - [MediaTime]       - optional, track duration
     *  "****"          - [Buffer]          - optional, codec csd data, may have different names
     * for audio track:
     *  kKeySampleRate  - [int32_t]         - mandatory
     *  kKeyChannels    - [int32_t]         - mandatory
     * for video track:
     *  kKeyWidth       - [int32_t]         - mandatory
     *  kKeyHeight      - [int32_t]         - mandatory
     * @return return a Message contains output formats
     * @note the best practice is to build format message at runtime and
     *       don't hold the message for whole life time. as message if
     *       not a good structure for frequently access, and it is waste
     *       of memory.
     */
    virtual Message         formats() const = 0;
    /**
     * read packets for each track.
     * @param index     index of the track
     * @param mode      read mode @see eModeReadyType
     * @param ts        time in track's timescale
     * @return return reference to new packet if not eos and no
     *         error happens.
     * @note the extractor may have to avoid seek too much, as we
     *       are read each track seperately
     */
    virtual sp<MediaPacket> read(size_t index,
            eModeReadType mode,
            const MediaTime& ts = kTimeInvalid) = 0;
};
__END_NAMESPACE_MPX
#endif

#endif
