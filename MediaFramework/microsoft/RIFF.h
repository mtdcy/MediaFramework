/******************************************************************************
 * Copyright (c) 2020, Chen Fang <mtdcy.chen@gmail.com>
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

#ifndef MFWK_RIFF_H
#define MFWK_RIFF_H

#include "MediaTypes.h"
#include "Microsoft.h"

__BEGIN_NAMESPACE_MFWK
__BEGIN_NAMESPACE(RIFF)

enum {
    kChunkTypeLeaf,
    kChunkTypeMaster,
};

#define RIFF_CHUNK_MIN_LENGTH (8)
struct Chunk : public SharedObject {
    UInt32      ckType;     //
    UInt32      ckID;       // FourCC
    UInt32      ckSize;     // little endian, data bytes
    
    Chunk(UInt32 id, UInt32 size) : ckType(kChunkTypeLeaf), ckID(id), ckSize(size) { }
    Chunk(UInt32 id, UInt32 size, UInt32 type) : ckType(type), ckID(id), ckSize(size) { }
    virtual ~Chunk() { }
    
    // parse chunk data
    virtual MediaError parse(const sp<ABuffer>&) = 0;
    // RIFF chunk is a bad structure, for master chunk, always has datas before children.
    // this make it hard to parse the data. so size() return data size except children
    // for master chunk, return all data size for leaf chunk
    virtual UInt32 size() const { return ckSize; }
    virtual String string() const { return String::format("%.4s[%zu]", (const Char *)&ckID, (UInt32)ckSize); }
};

struct MasterChunk : public Chunk {
    Vector<sp<Chunk> >  ckChildren;
    
    MasterChunk(UInt32 id, UInt32 size) : Chunk(id, size, kChunkTypeMaster) { }
    // make size() abstract, master chunk always have to return data size except children.
    virtual UInt32 size() const = 0;
    virtual String string() const { return Chunk::string() + String::format(", %zu children", ckChildren.size()); }
};

#define RIFF_CHUNK_LENGTH   (12)
struct RIFFChunk : public MasterChunk {
    UInt32  ckFileType;     // FourCC
    
    RIFFChunk(UInt32 size) : MasterChunk(FOURCC('RIFF'), size) { }
    
    virtual MediaError parse(const sp<ABuffer>& buffer) {
        if (buffer->size() < 4)
            return kMediaErrorBadContent;
        ckFileType = buffer->rl32();
        return kMediaNoError;
    }
    
    virtual UInt32 size() const { return sizeof(UInt32); }
    virtual String string() const { return MasterChunk::string() + String::format(", ckFileType = %.4s", (const Char *)&ckFileType); }
};

struct VOIDChunk : public Chunk {
    // NO DATA
    VOIDChunk(UInt32 id, UInt32 size) : Chunk(id, size) { }
    virtual MediaError parse(const sp<ABuffer>&) { return kMediaNoError; }
};

struct SKIPChunk : public Chunk {
    // SKIP DATA
    SKIPChunk(UInt32 id, UInt32 size) : Chunk(id, size) { }
    virtual MediaError parse(const sp<ABuffer>& buffer) {
        buffer->skipBytes(ckSize);
        return kMediaNoError;
    }
};

__END_NAMESPACE(RIFF)
__END_NAMESPACE_MFWK

#endif // MFWK_RIFF_H
