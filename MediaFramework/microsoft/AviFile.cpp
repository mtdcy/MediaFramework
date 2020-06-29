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


// File:    WaveFile.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "AviFile"
#define LOG_NDEBUG 0
#include "MediaTypes.h"
#include "MediaFile.h"
#include "Microsoft.h"
#include "RIFF.h"

// references:
// 1. https://wiki.multimedia.cx/index.php/Microsoft_Audio/Video_Interleaved
// 2. https://docs.microsoft.com/en-us/previous-versions//ms779636(v=vs.85)?redirectedfrom=MSDN

__BEGIN_NAMESPACE_MPX

enum {
    ID_RIFF = FOURCC('RIFF'),
    ID_AVI  = FOURCC('AVI '),
    ID_LIST = FOURCC('LIST'),
    ID_HDRL = FOURCC('hdrl'),
    ID_AVIH = FOURCC('avih'),
    ID_STRL = FOURCC('strl'),
    ID_STRH = FOURCC('strh'),
    ID_STRF = FOURCC('strf'),
    ID_STRD = FOURCC('strd'),
    ID_STRN = FOURCC('strn'),
    ID_INFO = FOURCC('INFO'),
    ID_MOVI = FOURCC('movi'),
    ID_00DB = FOURCC('00DB'),   // uncompressed video
    ID_00DC = FOURCC('00dc'),   // compressed video
    ID_01WB = FOURCC('01wb'),   // audio
    ID_IDX1 = FOURCC('idx1'),
    
    // OpenDML
    ID_VPRP = FOURCC('vprp'),
    ID_ISFT = FOURCC('ISFT'),
    // ..
    ID_JUNK = FOURCC('JUNK'),
};

struct JUNKChunk : RIFF::SKIPChunk {
    JUNKChunk(uint32_t size) : RIFF::SKIPChunk(ID_JUNK, size) { }
};

struct ListChunk : RIFF::MasterChunk {
    uint32_t                ckListType;     // FOURCC
    
    // RIFF is a bad structure, different ckListType for different structure
    ListChunk(uint32_t size, uint32_t type) : RIFF::MasterChunk(ID_LIST, size), ckListType(type) { }
    virtual String string() const { return MasterChunk::Chunk::string() + String::format(", ckListType = %.4s", (const char *)&ckListType); }
};

enum {
    kStreamTypeAudio    = FOURCC('auds'),
    kStreamTypeVideo    = FOURCC('vids'),
    kStreamTypeSubtitle = FOURCC('txts'),
};

// https://docs.microsoft.com/en-us/previous-versions//ms779632(v=vs.85)
// ckListType == 'hdrl', fcc = 'avih'
struct MainHeaderListChunk : public ListChunk {
    uint32_t    fcc;
    uint32_t    cb;
    uint32_t    dwMicroSecPerFrame;
    uint32_t    dwMaxBytesPerSec;
    uint32_t    dwPaddingGranularity;
    uint32_t    dwFlags;
    uint32_t    dwTotalFrames;
    uint32_t    dwInitialFrames;
    uint32_t    dwStreams;
    uint32_t    dwSuggestedBufferSize;
    uint32_t    dwWidth;
    uint32_t    dwHeight;
    uint32_t    dwReserved[4];
    // 16 * 4 = 64 bytes
    
    MainHeaderListChunk(uint32_t size) : ListChunk(size, ID_AVIH) { }
    
    virtual MediaError parse(const sp<ABuffer>& buffer) {
        // TODO
        return kMediaNoError;
    }
    virtual size_t size() const { return 64; }
    virtual String string() const { return ListChunk::string() + String::format(", ..."); }
};

// https://docs.microsoft.com/en-us/previous-versions//ms779638(v=vs.85)
struct StreamHeaderChunk : RIFF::Chunk {
    uint32_t fcc;
    uint32_t  cb;
    uint32_t fccType;
    uint32_t fccHandler;
    uint32_t  dwFlags;
    uint16_t   wPriority;
    uint16_t   wLanguage;
    uint32_t  dwInitialFrames;
    uint32_t  dwScale;
    uint32_t  dwRate;
    uint32_t  dwStart;
    uint32_t  dwLength;
    uint32_t  dwSuggestedBufferSize;
    uint32_t  dwQuality;
    uint32_t  dwSampleSize;
    struct {
        short int left;
        short int top;
        short int right;
        short int bottom;
    }  rcFrame;
    
    StreamHeaderChunk(uint32_t size) : RIFF::Chunk(ID_STRH, size) { }
    
    virtual MediaError parse(const sp<ABuffer>& buffer) {
        return kMediaNoError;
    }
};

struct StreamFormatChunk : RIFF::Chunk {
    // WAVEFORMATEX or BITMAPINFOHEADER
    sp<Buffer>  Format;
    
    StreamFormatChunk(uint32_t size) : RIFF::Chunk(ID_STRF, size) { }
    
    virtual MediaError parse(const sp<ABuffer>& buffer) {
        return kMediaNoError;
    }
};

struct StreamDataChunk : RIFF::Chunk {
    sp<Buffer>  Data;
    
    StreamDataChunk(uint32_t size) : RIFF::Chunk(ID_STRD, size) { }
    
    virtual MediaError parse(const sp<ABuffer>& buffer) {
        return kMediaNoError;
    }
};

// optional
struct StreamNameChunk : RIFF::Chunk {
    String  Name;
    StreamNameChunk(uint32_t size) : RIFF::Chunk(ID_STRN, size) { }
    
    virtual MediaError parse(const sp<ABuffer>& buffer) {
        return kMediaNoError;
    }
};

// OpenDML ...
struct VideoPropHeader : public RIFF::Chunk {
    uint32_t VideoFormatToken;
     uint32_t VideoStandard;
     uint32_t dwVerticalRefreshRate;
     uint32_t dwHTotalInT;
     uint32_t dwVTotalInLines;
     uint32_t dwFrameAspectRatio;
     uint32_t dwFrameWidthInPixels;
     uint32_t dwFrameHeightInLines;
     // uint32_t nbFieldPerFrame;
     struct VIDEO_FIELD_DESC {
     uint32_t CompressedBMHeight;
     uint32_t CompressedBMWidth;
     uint32_t ValidBMHeight;
     uint32_t ValidBMWidth;
     uint32_t ValidBMXOffset;
     uint32_t ValidBMYOffset;
     uint32_t VideoXOffsetInT;
     uint32_t VideoYValidStartLine;
    };
    Vector<VIDEO_FIELD_DESC>    FieldInfo;  // size() == nbFieldPerFrame
    
    VideoPropHeader(uint32_t size) : RIFF::Chunk(ID_VPRP, size) { }
    virtual MediaError parse(const sp<ABuffer>& buffer) {
        return kMediaNoError;
    }
};

struct InfoSoftware : public RIFF::Chunk {
    String      Name;
    InfoSoftware(uint32_t size) : RIFF::Chunk(ID_ISFT, size) { }
    virtual MediaError parse(const sp<ABuffer>& buffer) {
        Name = buffer->rs(buffer->size());
        return kMediaNoError;
    }
    virtual String string() const {
        return RIFF::Chunk::string() + String::format(", %s", Name.c_str());
    }
};

typedef sp<RIFF::Chunk> (*create_ck_t)(uint32_t);
static HashTable<uint32_t, create_ck_t> ckRegister;
struct ChunkRegister {
    
    template <typename TYPE>
    static sp<RIFF::Chunk> Create(uint32_t size) { return new TYPE(size); }
    
    ChunkRegister() {
        ckRegister.insert(ID_RIFF,  Create<RIFF::RIFFChunk>         );
        ckRegister.insert(ID_HDRL,  Create<MainHeaderListChunk>     );
        ckRegister.insert(ID_STRH,  Create<StreamHeaderChunk>       );
        ckRegister.insert(ID_STRF,  Create<StreamFormatChunk>       );
        ckRegister.insert(ID_STRD,  Create<StreamDataChunk>         );
        ckRegister.insert(ID_STRN,  Create<StreamNameChunk>         );
        ckRegister.insert(ID_JUNK,  Create<JUNKChunk>               );
        
        // OpenDML
        ckRegister.insert(ID_VPRP,  Create<VideoPropHeader>         );
        ckRegister.insert(ID_ISFT,  Create<InfoSoftware>            );
    }
    
    sp<RIFF::Chunk> alloc(uint32_t id, uint32_t size) {
        if (ckRegister.find(id)) {
            sp<RIFF::Chunk> ck = ckRegister[id](size);
            return ck;
        }
        ERROR("unknown chunk %.4s[%d]", (const char *)&id, size);
        return NULL;
    }
};

struct Entry {
    sp<RIFF::MasterChunk>   ckMaster;
    uint32_t                ckSize;
    Entry(sp<RIFF::MasterChunk>& master, uint32_t size) :
    ckMaster(master), ckSize(size) { }
};

static sp<RIFF::Chunk> EnumChunks(const sp<ABuffer>& buffer) {
    static ChunkRegister    cks;
    List<Entry>             ckParents;
    sp<RIFF::MasterChunk>   ckMaster;
    uint32_t                ckMasterLength;
    
    uint32_t ckID           = buffer->rl32();
    uint32_t ckSize         = buffer->rl32();
    
    sp<RIFF::RIFFChunk> RIFF = new RIFF::RIFFChunk(ckSize);
    RIFF->parse(buffer);
    
    if (RIFF->ckID != ID_RIFF || RIFF->ckFileType != ID_AVI) {
        ERROR("missing RIFF/AVI header");
        return NULL;
    }
    
    DEBUG("[%zu] %s", ckParents.size(), RIFF->string().c_str());
    
    ckMaster        = RIFF;
    ckMasterLength  = ckSize;
    
    for (;;) {
        uint32_t ckID   = buffer->rl32();
        uint32_t ckSize = buffer->rl32();
        
        // workaound for RIFF LIST
        // LIST is a very bad structure, it may contains other LIST
        // or raw struct, this make LIST parse() very hard
        if (ckID == ID_LIST) {
            ckID        = buffer->rl32();
        }
        
        sp<RIFF::Chunk> ck = cks.alloc(ckID, ckSize);
        if (ck.isNIL()) {
#if LOG_NDEBUG == 1
            break;
#else
            continue;
#endif
        }
        
        ckMasterLength -= ck->size() + 8;
        
        DEBUG("[%zu] %s, remains ckLength = %zu",
              ckParents.size() + 1, ck->string().c_str(), ckMasterLength);
        
        if (ck->parse(buffer) != kMediaNoError) {
            ERROR("ck %.4s parse failed", (const char *)&ckID);
            break;
        }
                
        ckMaster->ckChildren.push(ck);
        
        if (ck->ckType == RIFF::kChunkTypeMaster) {
            ckParents.push(Entry(ckMaster, ckMasterLength));
            ckMaster        = ck;
            ckMasterLength  = ckSize;
            continue;
        }
        
        CHECK_FALSE(ckMaster.isNIL());
        
        // ckSize SHOULD excluding ckID & ckSize & padding,
        // but someone may set it wrong
        while (ckMasterLength < RIFF_CHUNK_MIN_LENGTH) {
            if (ckParents.empty()) break;
            Entry& e = ckParents.back();
            ckMaster        = e.ckMaster;
            ckMasterLength  = e.ckSize;
            ckParents.pop_back();
        }
        
        if (ckParents.empty() && ckMasterLength < RIFF_CHUNK_MIN_LENGTH) break;
    }
    return ckMaster;
}

struct AviFile : public MediaFile {
    AviFile() { }
    
    MediaError init(const sp<ABuffer>& buffer) {
        sp<RIFF::RIFFChunk> RIFF = EnumChunks(buffer);
    }
    
    virtual sp<Message> formats() const {
        return NULL;
    }
    
    virtual MediaError configure(const sp<Message>& options) {
        return kMediaErrorInvalidOperation;
    }
    
    virtual sp<MediaFrame> read(const eReadMode& mode, const MediaTime& ts) {
        return NULL;
    }
};

sp<MediaFile> OpenAviFile(const sp<ABuffer>& buffer) {
    sp<AviFile> file = new AviFile;
    if (file->init(buffer) == kMediaNoError) return file;
    return NULL;
}

int IsAviFile(const sp<ABuffer>& buffer) {
    if (buffer->size() < RIFF_CHUNK_LENGTH) return 0;
        
    // RIFF CHUNK
    uint32_t name   = buffer->rl32();
    uint32_t length = buffer->rl32();
    uint32_t wave   = buffer->rl32();
    if (name != FOURCC('RIFF') || wave != FOURCC('AVI '))
        return 0;
    
    // DO EXTRA CHECK
    return 100;
}
__END_NAMESPACE_MPX
