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


// File:    WaveFile.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_MODULES_WAVE_FILE_H
#define _MEDIA_MODULES_WAVE_FILE_H

#include <modules/MediaDefs.h>
#include <modules/asf/Asf.h>

namespace mtdcy {
    class WaveFile : public MediaFile {
        public:
            WaveFile(const sp<Content>& pipe, const sp<Message>& formats);

            virtual ~WaveFile();

        public:
            virtual String          string() const;
            virtual status_t        status() const;
            virtual sp<Message>     formats() const;
            virtual status_t        configure(const sp<Message>& options) { return INVALID_OPERATION; }
            virtual sp<Buffer>      readFrame();
            virtual status_t        seekTo(int64_t timeus, int option = 0);

        private:
            bool                    parseFormat(const sp<Buffer>& ck); 

            sp<Content>             mContent;
            sp<Message>             mFormats;
            ASF::WAVEFORMATEX       mWave;

            int64_t                 mDataOffset;
            int64_t                 mDataLength;
            int64_t                 mDataRemains;

            int64_t                 mBytesRead;

            double                  mBytesPerSec;

            struct {
                int num;            ///< numerator
                int den;            ///< denominator
            } mBitRate;

            bool                    mOutputEOS;

        private:
            DISALLOW_EVILS(WaveFile);
    };
};

#endif // _MEDIA_MODULES_WAVE_FILE_H

