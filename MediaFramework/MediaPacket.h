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


// File:    MediaPacket.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_PACKET_H
#define _MEDIA_PACKET_H

#include <MediaToolkit/Toolkit.h>
#include <MediaFramework/MediaTime.h>

__BEGIN_DECLS 

__END_DECLS 

#ifdef __cplusplus
namespace mtdcy {

    /**
     * media packet class for compressed audio and video packets
     */
    struct MediaPacket {
        size_t              index;      ///< sample index, 0 based value
        sp<Buffer>          data;       ///< data buffer and data size
        eCodecFormat        format;     ///< packet format @see eCodecFormat
        uint32_t            flags;      ///< @see kFrameFlag*
        MediaTime           dts;        ///< dts in track's timescale
        MediaTime           pts;        ///< pts in track's timecasle
        sp<Message>         extra;      ///< extra informations

        MediaPacket() : index(0), data(NULL), format(kCodecFormatUnknown),
        flags(kFrameFlagNone), dts(kTimeInvalid), pts(kTimeInvalid), extra(NULL) { }
    };

};
#endif // __cplusplus

#endif // _MEDIA_PACKET_H