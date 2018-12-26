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


// File:    ID3.h
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#ifndef _MEDIA_MODULES_ID3_H
#define _MEDIA_MODULES_ID3_H

#include <MediaToolkit/Toolkit.h>
#include <MediaFramework/MediaDefs.h> 

namespace mtdcy {

    namespace ID3 {
        class ID3v2 : public Tag::Parser {
            public:
                ID3v2() : Tag::Parser() { }
                virtual ~ID3v2() { }
                virtual status_t        parse(const Buffer& data);

            public:
                // is data contains an id3v2 ? 
                // if yes, return id3v2 length excluding the header length
                // else return < 0
                // header should be at least kHeaderLength bytes
                static const size_t     kHeaderLength;
                static ssize_t          isID3v2(const Buffer& header);
        };

        class ID3v1 : public Tag::Parser {
            public:
                ID3v1() : Tag::Parser() { }
                virtual ~ID3v1() { }
                virtual status_t        parse(const Buffer& data);

            public:
                // is data contains an id3v1 ? 
                // return true or false 
                // data should be at least kLength and id3v1 is always
                // kLength bytes
                static bool             isID3v1(const Buffer& data);
                static const size_t     kLength;
        };
    };
};


#endif // _MEDIA_MODULES_ID3_H
