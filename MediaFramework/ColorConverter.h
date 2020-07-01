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


/**
 * File:    ImageConverter.h
 * Author:  mtdcy.chen
 * Changes:
 *          1. 20200630     initial version
 *
 */

#ifndef _MEDIA_MODULES_IMAGE_CONVERTER_H
#define _MEDIA_MODULES_IMAGE_CONVERTER_H
#include <MediaFramework/MediaTypes.h>
#include <MediaFramework/MediaFrame.h>
#include <MediaFramework/MediaUnit.h>

__BEGIN_DECLS

API_EXPORT const MediaUnit * ColorUnitFindNext(const MediaUnit *, const ePixelFormat);

enum eConversion { kBT601, kBT709, kJFIF };

__END_DECLS

#ifdef __cplusplus
__BEGIN_NAMESPACE_MPX

class API_EXPORT ColorConverter : public SharedObject {
    public:
        static sp<ColorConverter> create(const ImageFormat&, const ImageFormat&);

        virtual sp<MediaFrame> convert(const sp<MediaFrame>&) = 0;

    protected:
        ColorConverter() { }
        virtual ~ColorConverter() { }
    
        DISALLOW_EVILS(ColorConverter);
};

__END_NAMESPACE_MPX
#endif // __cplusplus

#endif //_MEDIA_MODULES_IMAGE_CONVERTER_H
