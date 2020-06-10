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


// File:    MediaTypes.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#ifndef _MEDIA_MODULES_EXTRACTOR_H
#define _MEDIA_MODULES_EXTRACTOR_H

#include <MediaFramework/MediaTypes.h>

__BEGIN_DECLS

/**
 * constrants should export
 * @note framework internal constrants should define inside MediaFile
 */

__END_DECLS

#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

/**
 * base class for different files
 */
struct API_EXPORT MediaFile : public SharedObject {
    /**
     * allocate an file object
     * @param mode      file mode @see eMode
     * @return return reference to new file
     */
    enum eMode { Read, Write, Modify };
    static sp<MediaFile>    Create(sp<Content>& pipe, const eMode mode = Read);

    /**
     * configure this codec
     * @param options   option and parameter
     * @return return kMediaNoError on success, otherwise error code.
     */
    virtual MediaError      configure(const sp<Message>& options) { return kMediaErrorInvalidOperation; }

    /**
     * get output format information of this codec.
     * about the output format:
     *  kKeyFormat      - [eFileFormat] - mandatory, file format @see eFileFormat
     *  kKeyDuration    - [int64_t]         - mandatory, file duration in us
     *  kKeyCount       - [int32_t]         - mandatory, number tracks
     *  "track-%zu"     - [sp<Message>]     - mandatory, track formats
     * about the track format:
     *  kKeyFormat      - [eCodecFormat]    - mandatory, @see eCodecFormat
     *  kKeyType        - [eCodecType]      - mandatory, @see eCodecType
     *  kKeyDuration    - [int64_t]         - optional, track duration in us
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
    virtual sp<Message>     formats() const = 0;

    /**
     * read packets for each track.
     * @param index     index of the track
     * @param mode      read mode @see eModeReadyType
     * @param ts        time in track's timescale
     * @return return reference to new packet if not eos and no
     *         error happens.
     * @note the file may have to avoid seek too much, as we
     *       are read each track seperately
     */
    virtual sp<MediaPacket> read(const eReadMode& mode = kReadModeDefault,
            const MediaTime& ts = kMediaTimeInvalid) = 0;

    /**
     * write packets to file object
     * @param
     */
    virtual MediaError  write(const sp<MediaPacket>& packet) { return kMediaErrorInvalidOperation; }
};
__END_NAMESPACE_MPX
#endif

#endif
