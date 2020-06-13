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


// File:    RIFF.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#ifndef _MPX_MEDIA_RIFF_H
#define _MPX_MEDIA_RIFF_H

#include "MediaTypes.h"
#include "Microsoft.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(RIFF)

#define RIFF_CHUNK_MIN_LENGTH (8)
struct Chunk : public SharedObject {
    uint32_t    Name;       // FourCC
    uint32_t    Length;     // little endian, data bytes
    
    Chunk(uint32_t name, uint32_t length) : Name(name), Length(length) { }
    virtual ~Chunk() { }
    
    // parse chunk data
    virtual MediaError parse(BitReader& br) = 0;
};

#define RIFF_CHUNK_LENGTH   (12)
struct RIFFChunk : public Chunk {
    uint32_t    Data;       // FourCC
    
    RIFFChunk(uint32_t length) : Chunk(FOURCC('RIFF'), length) { }
    
    virtual MediaError parse(BitReader& br) {
        if (br.remianBytes() < 4)
            return kMediaErrorBadContent;
        Data    = br.rl32();
        return kMediaNoError;
    }
};

struct VOIDChunk : public Chunk {
    // NO DATA
    VOIDChunk(uint32_t name, uint32_t length) : Chunk(name, length) { }
    virtual MediaError parse(BitReader&) { return kMediaNoError; }
};

struct SKIPChunk : public Chunk {
    // SKIP DATA
    SKIPChunk(uint32_t name, uint32_t length) : Chunk(name, length) { }
    virtual MediaError parse(BitReader& br) {
        br.skipBytes(Length);
        return kMediaNoError;
    }
};

__END_NAMESPACE(RIFF)
__END_NAMESPACE_MPX

#endif // _MPX_MEDIA_RIFF_H
