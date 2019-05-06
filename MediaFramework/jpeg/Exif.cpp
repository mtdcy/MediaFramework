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


// File:    Exif.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "Exif"
#define LOG_NDEBUG 0
#include "Exif.h"


__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(EXIF)


sp<AttributeInformation> readAttributeInformation(const BitReader& br) {
    
    if (br.readS(6) != "Exif") {
        ERROR("bad Exif Attribute Information");
        return NIL;
    }
    
    // TIFF Header
    String id           = br.readS(2);
    uint32_t version    = br.rb32();
    uint32_t offset     = br.rb32();    // offset of first image
    DEBUG("id %s, version %#x, offset %u", id.c_str(), version, offset);
    
    // IFDs (Image File Directory)
    while (br.numBitsLeft() > 18 * 8) {
        uint32_t fields = br.rb16();    // number of fields
        uint16_t tag    = br.rb16();
        uint16_t type   = br.rb16();
        uint32_t count  = br.rb32();
        uint32_t offset = br.rb32();    // value offset
        uint32_t next   = br.rb32();    // offset to next IFD
        DEBUG("%u fields, tag %u, type %u, count %u, offset %u, next %u",
              fields, tag, type, count, offset, next);
        
    }
    
    return NIL;
}

__END_NAMESPACE(EXIF)
__END_NAMESPACE_MPX
