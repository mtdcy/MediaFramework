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

#ifndef MFWK_JPEG_JFIF_H
#define MFWK_JPEG_JFIF_H

#include "MediaTypes.h"
#include "JPEG.h"
#include "Exif.h"

__BEGIN_NAMESPACE_MFWK
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
    // APP0 : UInt16
    // length : UInt16
    UInt16      version;
    UInt8       units;      // 0: no units; 1: dots per inch; 2: dots per cm
    UInt16      x;          // horizontal pixel density
    UInt16      y;          // vertical pixel density
    UInt8       width0;     // thumbnail horizontal pixel count [optional]
    UInt8       height0;    // thumbnail vertical pixel count [optional]
    sp<Buffer>  thunmbnail; // packed RGB thumbnail [optional]
    
    // "JFXX\0": thumbnail coded using JPEG
    UInt8     extension;  // extension code: 0x10 - JPEG, 0x11 - 1 byte/pixel, 0x13 - 3 bytes/pixel
    sp<JPEG::JIFObject> jif;// JPEG thumbnail
};

/**
 * BitReader: without marker & length
 */
sp<AppHeader>   readAppHeader(const sp<ABuffer>&, UInt32);
MediaError      extendAppHeader(sp<AppHeader>&, const sp<ABuffer>&, UInt32);
void            printAppHeader(const sp<AppHeader>&);

__END_NAMESPACE(JFIF)

/**
 * Object for JPEG/JFIF & JPEG/Exif
 * as most writer will write both APP0(JFIF) & APP1(Exif), so it always looks like JFIF
 * so we prefer JFIFObject instead of ExifObject
 */
struct JFIFObject : public JPEG::JIFObject {
    JFIFObject() : JPEG::JIFObject(True) { }
    
    sp<JFIF::AppHeader>                 mAppHeader;             ///< JFIF APP0, exist only for JFIF
    sp<EXIF::AttributeInformation>      mAttributeInformation;  ///< Exif APP1, may exists in both JFIF & Exif
};

sp<JFIFObject> openJFIF(const sp<ABuffer>&);

void printJFIFObject(const sp<JFIFObject>&);

__END_NAMESPACE_MFWK

#endif // MFWK_JPEG_JFIF_H
