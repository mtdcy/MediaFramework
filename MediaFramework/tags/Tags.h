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


// File:    Tags.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#ifndef _MPX_MEDIA_TAGS_H
#define _MPX_MEDIA_TAGS_H 

#include "MediaTypes.h"

__BEGIN_NAMESPACE_MPX

namespace Tag {
    class API_EXPORT Parser {
        public:
            FORCE_INLINE Parser() { }
            FORCE_INLINE virtual ~Parser() { }
            virtual MediaError parse(const Buffer& data) = 0;
            FORCE_INLINE const sp<Message>& values() const { return mValues; }

        protected:
            sp<Message>     mValues;
    };

    class API_EXPORT Writter {
        public:
            FORCE_INLINE Writter() { }
            FORCE_INLINE virtual ~Writter() { }
            virtual MediaError synth(const Message& data) = 0;
            //const Buffer&       values() const {  }

        protected:
            //Buffer              mBuffer;
    };
};

__END_NAMESPACE_MPX

#endif // _MPX_MEDIA_TAGS_H 
