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


// File:    JFIF.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#ifndef _MEDIA_JPEG_JFIF_H
#define _MEDIA_JPEG_JFIF_H

#include "MediaTypes.h"
#include "JPEG.h"
#include "Exif.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(JFIF)

/**
 * ITU-R T871
 * ISO/IEC 10918-5:2012 (E)
 *
 *
 * APP0 segment: 0xFFE0
 * with identifier : "JFIF\0"
 * length >= 16
 */
struct AppHeader : public SharedObject {
    // APP0 : uint16_t
    // length : uint16_t
    uint16_t    version;
    uint8_t     units;      // 0: no units; 1: dots per inch; 2: dots per cm
    uint16_t    x;          // horizontal pixel density
    uint16_t    y;          // vertical pixel density
    uint8_t     width0;     // thumbnail horizontal pixel count [optional]
    uint8_t     height0;    // thumbnail vertical pixel count [optional]
    sp<Buffer>  thunmbnail; // packed RGB thumbnail [optional]
    
    // "JFXX\0": thumbnail coded using JPEG
    uint8_t     extension;  // extension code: 0x10 - JPEG, 0x11 - 1 byte/pixel, 0x13 - 3 bytes/pixel
    sp<JPEG::JIFObject> jif;// JPEG thumbnail
};

/**
 * BitReader: without marker & length
 */
sp<AppHeader>   readAppHeader(const BitReader&, size_t);
MediaError      extendAppHeader(sp<AppHeader>&, const BitReader&, size_t);
void            printAppHeader(const sp<AppHeader>&);

__END_NAMESPACE(JFIF)

/**
 * Object for JPEG/JFIF & JPEG/Exif
 * as most writer will write both APP0(JFIF) & APP1(Exif), so it always looks like JFIF
 * so we prefer JFIFObject instead of ExifObject
 */
struct JFIFObject : public JPEG::JIFObject {
    JFIFObject() : JPEG::JIFObject(true) { }
    
    sp<JFIF::AppHeader>                 mAppHeader;             ///< JFIF APP0, exist only for JFIF
    sp<EXIF::AttributeInformation>      mAttributeInformation;  ///< Exif APP1, may exists in both JFIF & Exif
};

sp<JFIFObject> openJFIF(sp<Content>&);

void printJFIFObject(const sp<JFIFObject>&);

__END_NAMESPACE_MPX

#endif // _MEDIA_JPEG_JFIF_H
