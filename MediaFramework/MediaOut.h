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

#ifndef _MEDIA_MODULES_OUT_H
#define _MEDIA_MODULES_OUT_H

#include <MediaToolkit/Toolkit.h>
#include <MediaFramework/MediaFrame.h>

__BEGIN_DECLS

__END_DECLS

#ifdef __cplusplus
namespace mtdcy {

    
    /**
     * base class for audio/video output device
     */
    class MediaOut {
    public:
        /**
         * create a output device based on provided parameters
         * @param type      codec type @see eCodecType
         * @param options   option and parameter for creating device
         * @return return reference to a new output device
         */
        static sp<MediaOut> Create(eCodecType type, const Message& options);
        virtual ~MediaOut() { }
        
    public:
        /**
         * get information of this output device.
         * @return return a string of information
         */
        virtual String          string() const = 0;
        /**
         * get status of this output device.
         * @return return OK if everything is OK, otherwise error code
         */
        virtual status_t        status() const = 0;
        /**
         * get information of this output device
         * @return return message reference of this output device.
         */
        virtual const Message&  formats() const = 0;
        /**
         * configure this output device
         * @param options   option and parameter
         * @return return OK on success, otherwise error code.
         */
        virtual status_t        configure(const Message& options) = 0;
        /**
         * push a MediaFrame to this output device.
         * @param input     reference of MediaFrame
         * @return return OK on success
         * @note push a NULL packet to notify codec of eos
         * @note write in block way, always return OK if no error happens.
         */
        virtual status_t        write(const sp<MediaFrame>& input) = 0;
        /**
         * flush context of this output device
         * @return return OK on success, otherwise error code
         */
        virtual status_t        flush() = 0;
        
    protected:
        MediaOut() { }
        
    private:
        DISALLOW_EVILS(MediaOut);
    };


};
#endif

#endif