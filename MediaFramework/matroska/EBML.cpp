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


// File:    EBML.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG   "EBML"
//#define LOG_NDEBUG 0
#include "MediaDefs.h"

#include "EBML.h"

//#define VERBOSE
#ifdef VERBOSE
#define DEBUGV(fmt, ...) DEBUG(fmt, ##__VA_ARGS__)
#else
#define DEBUGV(fmt, ...) do {} while(0)
#endif

// reference:
// https://matroska.org/technical/specs/index.html
// https://matroska.org/files/matroska.pdf

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(EBML)

static __ABE_INLINE size_t EBMLCodeBytesLength(uint64_t x) {
    size_t n = 0;
    while (x) { ++n; x >>= 8; }
    return n;
}

static __ABE_INLINE uint64_t EBMLCodeInteger(uint64_t x) {
    size_t n = EBMLCodeBytesLength(x);
    x |= (1 << (7 * n));
    return x;
}

EBMLInteger::EBMLInteger(uint64_t x) : u64(x), length(EBMLCodeBytesLength(x)) { }

static EBMLInteger EBMLIntegerNull(0);     // with value & length == 0

static __ABE_INLINE bool operator==(EBMLInteger& lhs, EBMLInteger& rhs) {
    return lhs.u64 == rhs.u64 && lhs.length == rhs.length;
}

#define MASK(n) ((1ULL<<(n))-1)
static __ABE_INLINE uint8_t EBMLGetBytesLength(uint8_t v) {
    return __builtin_clz((unsigned int)v) - 24 + 1;
}

// vint with leading 1-bit
static __ABE_INLINE EBMLInteger EBMLGetCodedInteger(BitReader& br) {
    EBMLInteger vint;
    vint.u8     = br.r8();
    vint.length = EBMLGetBytesLength(vint.u8);
    CHECK_GT(vint.length, 0);
    CHECK_LE(vint.length, 8);
    if (br.numBitsLeft() < vint.length * 8) {
        return EBMLIntegerNull;
    }

    if (vint.length > 1) {
        for (size_t i = 1; i < vint.length; ++i) {
            vint.u64 = (vint.u64 << 8) | br.r8();
        }
    }
    return vint;
}

static __ABE_INLINE uint64_t EBMLClearLeadingBit(uint64_t x, size_t length) {
    return x & MASK(7 * length);
}

// vint without leading 1-bit removed
static __ABE_INLINE EBMLInteger EBMLGetInteger(BitReader& br) {
    EBMLInteger vint = EBMLGetCodedInteger(br);
    vint.u64 = EBMLClearLeadingBit(vint.u64, vint.length);
    return vint;
}

// vint with or without leading 1-bit
static __ABE_INLINE EBMLInteger EBMLGetInteger(BitReader& br, size_t n) {
    CHECK_GT(n, 0);
    CHECK_LE(n, 8);
    EBMLInteger vint;
    vint.u8 = br.r8();
    vint.length = n;
    if (n > 1) {
        for (size_t i = 1; i < n; ++i) {
            vint.u64 = (vint.u64 << 8) | br.r8();
        }
    }
    return vint;
}

// vint -> element length
static __ABE_INLINE EBMLInteger EBMLGetLength(BitReader& br) {
    return EBMLGetInteger(br);
}

struct Int2Float {
    float       flt;
    uint32_t    u32;
};

struct Int2Double {
    double      dbl;
    uint64_t    u64;
};

static __ABE_INLINE double EBMLGetFloat(BitReader& br, size_t n) {
    if (n == 8) {
        CHECK_EQ(sizeof(double), 8);
        Int2Double a;
        a.u64 = br.rb64();
        return a.dbl;
    } else {
        CHECK_EQ(n, 4);
        Int2Float a;
        a.u32 = br.rb32();
        return a.flt;
    }
    return 0;
}

static __ABE_INLINE EBMLInteger EBMLGetCodedInteger(sp<Content>& pipe) {
    if (pipe->tell() >= pipe->size()) {
        return EBMLIntegerNull;
    }
    //CHECK_LT(pipe->tell(), pipe->size());
    sp<Buffer> data = pipe->read(1);
    EBMLInteger vint;
    vint.u64    = data->at(0);
    vint.length = EBMLGetBytesLength(vint.u8);
    CHECK_GT(vint.length, 0);
    CHECK_LE(vint.length, 8);
    if (pipe->tell() + vint.length >= pipe->size()) {
        return EBMLIntegerNull;
    }

    if (vint.length > 1) {
        if (pipe->tell() + vint.length >= pipe->size()) {
            vint.length = 0;    // invalid
            return vint;
        }
        sp<Buffer> ex = pipe->read(vint.length - 1);
        for (size_t i = 0; i < vint.length - 1; ++i) {
            vint.u64 = (vint.u64 << 8) | (uint8_t)ex->at(i);
        }
    }
    return vint;
}

static __ABE_INLINE EBMLInteger EBMLGetInteger(sp<Content>& pipe) {
    EBMLInteger vint = EBMLGetCodedInteger(pipe);
    vint.u64 = EBMLClearLeadingBit(vint.u64, vint.length);
    return vint;
}

#define EBMLGetLength   EBMLGetInteger

uint64_t vsint_subtr[] = {
    0x3F, 0x1FFF, 0x0FFFFF, 0x07FFFFFF,
    0x03FFFFFFFF, 0x01FFFFFFFFFF,
    0x00FFFFFFFFFFFF, 0x007FFFFFFFFFFFFF
};

static __ABE_INLINE EBMLSignedInteger EBMLGetSignedInteger(BitReader& br) {
    EBMLSignedInteger svint;
    EBMLInteger vint    = EBMLGetInteger(br);
    svint.i64           = vint.u64 - vsint_subtr[vint.length - 1];
    svint.length        = vint.length;
    return svint;
}

static __ABE_INLINE EBMLSignedInteger EBMLGetSignedInteger(BitReader& br, size_t n) {
    EBMLSignedInteger svint;
    EBMLInteger vint    = EBMLGetInteger(br, n);
    svint.i64           = vint.u64 - vsint_subtr[vint.length - 1];
    svint.length        = vint.length;
    return svint;
}

status_t EBMLIntegerElement::parse(BitReader& br, size_t size) {
    vint = EBMLGetInteger(br, size);
    DEBUGV("integer %#x", vint.u64);
    return OK;
}

String EBMLIntegerElement::string() const {
    return String::format("%#x", vint.u64);

}

status_t EBMLSignedIntegerElement::parse(BitReader& br, size_t size) {
    svint = EBMLGetSignedInteger(br, size);
    DEBUGV("integer %#x", svint.i64);
    return OK;
}

String EBMLSignedIntegerElement::string() const {
    return String::format("%#x", svint.i64);

}

status_t EBMLStringElement::parse(BitReader& br, size_t size) {
    str = br.readS(size);
    DEBUGV("string %s", str.c_str());
    return OK;
}

String EBMLStringElement::string() const { return str; }

status_t EBMLUTF8Element::parse(BitReader& br, size_t size) {
    utf8 = br.readS(size);
    DEBUGV("utf8 %s", utf8.c_str());
    return OK;
}

String EBMLUTF8Element::string() const { return utf8; }

status_t EBMLBinaryElement::parse(BitReader& br, size_t size) {
    data = br.readB(size);
    DEBUGV("binary %s", data->string(true).c_str());
    return OK;
}

String EBMLBinaryElement::string() const {
    return String::format("%zu bytes binary", data->size());
}

status_t EBMLFloatElement::parse(BitReader& br, size_t size) {
    flt = EBMLGetFloat(br, size);
    DEBUGV("float %f", flt);
    return OK;
}

String EBMLFloatElement::string() const {
    return String(flt);
}

status_t EBMLSkipElement::parse(BitReader& br, size_t size) {
    br.skipBytes(size);
    return OK;
}

String EBMLSkipElement::string() const { return "*"; }

status_t EBMLBlockElement::parse(BitReader& br, size_t size) {
    const size_t offset = br.offset();
    TrackNumber = EBMLGetInteger(br);
    TimeCode    = br.rb16();
    Flags       = br.r8();
    DEBUGV("[%zu] block size %zu, %#x", TrackNumber.u32, size, Flags);
    if (Flags & kBlockFlagLace) {
        size_t count = 1 + br.r8();     // frame count
        CHECK_GT(count, 1);
        size_t frame[count - 1];
        if ((Flags & kBlockFlagLace) == kBlockFlagEBML) {
            EBMLInteger vint = EBMLGetInteger(br);
            frame[0] = vint.u32;
            for (size_t i = 1; i < count - 1; ++i) {
                EBMLSignedInteger svint = EBMLGetSignedInteger(br);
                frame[i] = frame[i-1] + svint.i32;
                //DEBUG("frame %zu", frame[i]);
            }
        } else if ((Flags & kBlockFlagLace) == kBlockFlagXiph) {
            for (size_t i = 0; i < count - 1; ++i) {
                uint8_t u8 = br.r8();
                frame[i] = u8;
                while (u8 == 255) {
                    u8 = br.r8();
                    frame[i] += u8;
                }
            }
        } else if ((Flags & kBlockFlagLace) == kBlockFlagFixed) {
            const size_t total = size - (br.offset() - offset) / 8;
            CHECK_EQ(total % count, 0);
            const size_t length = total / count;
            for (size_t i = 0; i < count - 1; ++i) {
                frame[i] = length;
            }
        }

        for (size_t i = 0; i < count - 1; ++i) {
            //CHECK_GE(size, (br.offset() - offset) / 8 + frame[i]);
            data.push(br.readB(frame[i]));
        }
    }

    //CHECK_GT(size, (br.offset() - offset) / 8);
    data.push(br.readB(size - (br.offset() - offset) / 8)); // last frame length is not stored

    return OK;
}

String EBMLBlockElement::string() const {
    return String::format("[%zu] % " PRId16 " Flags %#x",
            (size_t)TrackNumber.u64, TimeCode, Flags);
}

static __ABE_INLINE sp<EBMLElement> MakeEBMLElement(EBMLInteger);

status_t EBMLMasterElement::parse(BitReader& br, size_t size) {
    size_t remains = size;
    while (remains) {
        EBMLInteger id      = EBMLGetCodedInteger(br);
        EBMLInteger length  = EBMLGetLength(br);
        if (id == EBMLIntegerNull || length == EBMLIntegerNull) {
            ERROR("invalid id or length");
            break;
        }

        CHECK_GE(remains, id.length + length.length + length.u32);
        remains -= (id.length + length.length + length.u32);

        sp<EBMLElement> elem = MakeEBMLElement(id);
        if (elem == NULL) {
            DEBUG("%s: unknown element %#x", id.u64);
            br.skip(length.u32);
            continue;
        }

        const int64_t offset = br.offset();
        if (elem->parse(br, length.u32) != OK) {  // u32 is logical ok
            ERROR("parse element %s failed", elem->name);
            br.skip(br.offset() - offset);
            continue;
        }
        DEBUGV("%s: + %s @ { %#x[%zu], %" PRIu32 " bytes[%zu], %s }",
                name, elem->name, id.u64, id.length,
                length.u32, length.length,
                elem->string().c_str());

        children.push(elem);
    }

    return OK;
}

String EBMLMasterElement::string() const {
    return String::format("%zu children", children.size());
}

#define ITEM(X, T)  { .NAME = #X, .ID = ID_##X, .TYPE = T   }
static const struct {
    const char *        NAME;
    uint64_t            ID;
    EBMLElementType     TYPE;
} ELEMENTS[] = {
    // top level elements
    ITEM(   EBMLHEADER,                 kEBMLElementMaster      ),
    ITEM(   SEGMENT,                    kEBMLElementMaster      ),
    // top level EBML element's children
    ITEM(   EBMLVERSION,                kEBMLElementInteger     ),
    ITEM(   EBMLREADVERSION,            kEBMLElementInteger     ),
    ITEM(   EBMLMAXIDLENGTH,            kEBMLElementInteger     ),
    ITEM(   EBMLMAXSIZELENGTH,          kEBMLElementInteger     ),
    ITEM(   DOCTYPEVERSION,             kEBMLElementInteger     ),
    ITEM(   DOCTYPEREADVERSION,         kEBMLElementInteger     ),
    ITEM(   DOCTYPE,                    kEBMLElementString      ),
    // SEGMENT
    ITEM(   SEGMENTINFO,                kEBMLElementMaster      ),
    ITEM(   SEEKHEAD,                   kEBMLElementMaster      ),
    ITEM(   CLUSTER,                    kEBMLElementMaster      ),
    ITEM(   TRACKS,                     kEBMLElementMaster      ),
    ITEM(   CUES,                       kEBMLElementMaster      ),
    ITEM(   ATTACHMENTS,                kEBMLElementMaster      ),
    ITEM(   CHAPTERS,                   kEBMLElementMaster      ),
    ITEM(   TAGS,                       kEBMLElementMaster      ),
    // SEGMENTINFO
    ITEM(   SEGMENTUID,                 kEBMLElementBinary      ),
    ITEM(   SEGMENTFILENAME,            kEBMLElementUTF8        ),
    ITEM(   PREVUID,                    kEBMLElementBinary      ),
    ITEM(   PREVFILENAME,               kEBMLElementUTF8        ),
    ITEM(   NEXTUID,                    kEBMLElementBinary      ),
    ITEM(   NEXTFILENAME,               kEBMLElementUTF8        ),
    ITEM(   TIMECODESCALE,              kEBMLElementInteger     ),
    ITEM(   DURATION,                   kEBMLElementFloat       ),
    ITEM(   TITLE,                      kEBMLElementUTF8        ),
    ITEM(   MUXINGAPP,                  kEBMLElementString      ),
    ITEM(   WRITINGAPP,                 kEBMLElementUTF8        ),
    ITEM(   DATEUTC,                    kEBMLElementSignedInteger   ),
    // SEEKHEAD
    ITEM(   SEEK,                       kEBMLElementMaster      ),
    ITEM(   SEEKID,                     kEBMLElementInteger     ),
    ITEM(   SEEKPOSITION,               kEBMLElementInteger     ),
    // TRACKS
    ITEM(   TRACKENTRY,                 kEBMLElementMaster      ),
    ITEM(   TRACKNUMBER,                kEBMLElementInteger     ),
    ITEM(   TRACKUID,                   kEBMLElementInteger     ),
    ITEM(   TRACKTYPE,                  kEBMLElementInteger     ),
    ITEM(   FLAGENABLED,                kEBMLElementInteger     ),
    ITEM(   FLAGDEFAULT,                kEBMLElementInteger     ),
    ITEM(   FLAGFORCED,                 kEBMLElementInteger     ),
    ITEM(   FLAGLACING,                 kEBMLElementInteger     ),
    ITEM(   MINCACHE,                   kEBMLElementInteger     ),
    ITEM(   MAXCACHE,                   kEBMLElementInteger     ),
    ITEM(   DEFAULTDURATION,            kEBMLElementInteger     ),
    ITEM(   TRACKTIMECODESCALE,         kEBMLElementFloat       ),
    ITEM(   NAME,                       kEBMLElementUTF8        ),
    ITEM(   LANGUAGE,                   kEBMLElementString      ),
    ITEM(   CODECID,                    kEBMLElementString      ),
    ITEM(   CODECPRIVATE,               kEBMLElementBinary      ),
    ITEM(   CODECNAME,                  kEBMLElementUTF8        ),
    ITEM(   ATTACHMENTLINK,             kEBMLElementInteger     ),
    // VIDEO
    ITEM(   VIDEO,                      kEBMLElementMaster      ),
    ITEM(   PIXELWIDTH,                 kEBMLElementInteger     ),
    ITEM(   PIXELHEIGHT,                kEBMLElementInteger     ),
    ITEM(   PIXELCROPBOTTOM,            kEBMLElementInteger     ),
    ITEM(   PIXELCROPTOP,               kEBMLElementInteger     ),
    ITEM(   PIXELCROPLEFT,              kEBMLElementInteger     ),
    ITEM(   PIXELCROPRIGHT,             kEBMLElementInteger     ),
    ITEM(   DISPLAYWIDTH,               kEBMLElementInteger     ),
    ITEM(   DISPLAYHEIGHT,              kEBMLElementInteger     ),
    ITEM(   DISPLAYUNIT,                kEBMLElementInteger     ),
    ITEM(   FLAGINTERLACED,             kEBMLElementInteger     ),
    // AUDIO
    ITEM(   AUDIO,                      kEBMLElementMaster      ),
    ITEM(   SAMPLINGFREQUENCY,          kEBMLElementFloat       ),
    ITEM(   OUTPUTSAMPLINGFREQUENCY,    kEBMLElementFloat       ),
    ITEM(   CHANNELS,                   kEBMLElementInteger     ),
    ITEM(   BITDEPTH,                   kEBMLElementInteger     ),
    // CONTENTENCODINGS
    ITEM(   CONTENTENCODINGS,           kEBMLElementMaster      ),
    // SEEKHEAD
    ITEM(   SEEK,                       kEBMLElementMaster      ),
    ITEM(   SEEKID,                     kEBMLElementInteger     ),
    ITEM(   SEEKPOSITION,               kEBMLElementInteger     ),
    // CLUSTER
    ITEM(   CRC32,                      kEBMLElementInteger     ),
    ITEM(   TIMECODE,                   kEBMLElementInteger     ),
    ITEM(   POSITION,                   kEBMLElementInteger     ),
    ITEM(   PREVSIZE,                   kEBMLElementInteger     ),
    ITEM(   BLOCKGROUP,                 kEBMLElementMaster      ),
    ITEM(   SIMPLEBLOCK,                kEBMLElementBlock       ),  // kEBMLElementBinary
    // BLOCKGROUP
    ITEM(   BLOCK,                      kEBMLElementBlock       ),  // kEBMLElementBinary
    ITEM(   REFERENCEBLOCK,             kEBMLElementSignedInteger   ),
    ITEM(   REFERENCEPRIORITY,          kEBMLElementInteger     ),
    ITEM(   BLOCKDURATION,              kEBMLElementSignedInteger   ),
    ITEM(   BLOCKVIRTUAL,               kEBMLElementBinary      ),
    ITEM(   BLOCKADDITIONS,             kEBMLElementMaster      ),
    ITEM(   CODECSTATE,                 kEBMLElementBinary      ),
    ITEM(   DISCARDPADDING,             kEBMLElementSignedInteger   ),
    // CUES
    ITEM(   CUEPOINT,                   kEBMLElementMaster      ),
    // CUEPOINT
    ITEM(   CUETIME,                    kEBMLElementInteger     ),
    ITEM(   CUETRACKPOSITIONS,          kEBMLElementMaster      ),
    // CUETRACKPOSITIONS
    ITEM(   CUETRACK,                   kEBMLElementInteger     ),
    ITEM(   CUECLUSTERPOSITION,         kEBMLElementInteger     ),
    ITEM(   CUERELATIVEPOSITION,        kEBMLElementInteger     ),
    ITEM(   CUEBLOCKNUMBER,             kEBMLElementInteger     ),
    ITEM(   CUECODECSTATE,              kEBMLElementInteger     ),
    ITEM(   CUEREFERENCE,               kEBMLElementMaster      ),
    // CUEREFERENCE
    ITEM(   CUEREFTIME,                 kEBMLElementInteger     ),
    ITEM(   CUEREFCLUSTER,              kEBMLElementInteger     ),
    ITEM(   CUEREFNUMBER,               kEBMLElementInteger     ),
    ITEM(   CUEREFCODECSTATE,           kEBMLElementInteger     ),
    // TAGS
    ITEM(   TAG,                        kEBMLElementMaster      ),
    ITEM(   TARGETS,                    kEBMLElementMaster      ),
    ITEM(   SIMPLETAG,                  kEBMLElementMaster      ),
    // TARGETS
    ITEM(   TARGETTYPEVALUE,            kEBMLElementInteger     ),
    ITEM(   TARGETTYPE,                 kEBMLElementUTF8        ),
    ITEM(   TARGETTRACKUID,             kEBMLElementInteger     ),
    ITEM(   TARGETEDITIONUID,           kEBMLElementInteger     ),
    ITEM(   TARGETCHAPTERUID,           kEBMLElementInteger     ),
    ITEM(   ATTACHMENTUID,              kEBMLElementInteger     ),
    // SIMPLETAG
    ITEM(   TAGNAME,                    kEBMLElementUTF8        ),
    ITEM(   TAGLANGUAGE,                kEBMLElementString      ),
    ITEM(   TAGORIGINAL,                kEBMLElementInteger     ),
    ITEM(   TAGSTRING,                  kEBMLElementString      ),
    ITEM(   TAGBINARY,                  kEBMLElementBinary      ),
    // CHAPTERS
    ITEM(   EDITIONENTRY,               kEBMLElementMaster      ),
    ITEM(   EDITIONUID,                 kEBMLElementInteger     ),
    ITEM(   EDITIONFLAGHIDDEN,          kEBMLElementInteger     ),
    ITEM(   EDITIONFLAGDEFAULT,         kEBMLElementInteger     ),
    ITEM(   EDITIONFLAGORDERED,         kEBMLElementInteger     ),
    ITEM(   CHAPTERATOM,                kEBMLElementMaster      ),
    // CHAPTERATOM
    ITEM(   CHAPTERUID,                 kEBMLElementInteger     ),
    ITEM(   CHAPTERTIMESTART,           kEBMLElementInteger     ),
    ITEM(   CHAPTERTIMEEND,             kEBMLElementInteger     ),
    ITEM(   CHAPTERFLAGHIDDEN,          kEBMLElementInteger     ),
    ITEM(   CHAPTERFLAGENABLED,         kEBMLElementInteger     ),
    ITEM(   CHAPTERSEGMENTUID,          kEBMLElementBinary      ),
    ITEM(   CHAPTERSEGMENTEDITIONUID,   kEBMLElementInteger     ),
    ITEM(   CHAPTERTRACKS,              kEBMLElementMaster      ),
    ITEM(   CHAPTERDISPLAY,             kEBMLElementMaster      ),
    // CHAPTERTRACKS
    ITEM(   CHAPTERTRACKNUMBER,         kEBMLElementInteger     ),
    // CHAPTERDISPLAY
    ITEM(   CHAPSTRING,                 kEBMLElementUTF8        ),
    ITEM(   CHAPLANGUAGE,               kEBMLElementString      ),
    ITEM(   CHAPCOUNTRY,                kEBMLElementUTF8        ),
    // END OF LIST
};
#define NELEM(x)    sizeof(x)/sizeof(x[0])

static __ABE_INLINE sp<EBMLElement> MakeEBMLElement(EBMLInteger id) {
    for (size_t i = 0; i < NELEM(ELEMENTS); ++i) {
        if (ELEMENTS[i].ID == id.u64) {
            //DEBUG("make element %s[%#x]", ELEMENTS[i].NAME, ELEMENTS[i].ID);
            switch (ELEMENTS[i].TYPE) {
                case kEBMLElementInteger:
                    return new EBMLIntegerElement(ELEMENTS[i].NAME, id);
                case kEBMLElementSignedInteger:
                    return new EBMLSignedIntegerElement(ELEMENTS[i].NAME, id);
                case kEBMLElementString:
                    return new EBMLStringElement(ELEMENTS[i].NAME, id);
                case kEBMLElementUTF8:
                    return new EBMLUTF8Element(ELEMENTS[i].NAME, id);
                case kEBMLElementFloat:
                    return new EBMLFloatElement(ELEMENTS[i].NAME, id);
                case kEBMLElementMaster:
                    return new EBMLMasterElement(ELEMENTS[i].NAME, id);
                case kEBMLElementBinary:
                    return new EBMLBinaryElement(ELEMENTS[i].NAME, id);
                case kEBMLElementSkip:
                    return new EBMLSkipElement(ELEMENTS[i].NAME, id);
                case kEBMLElementBlock:
                    return new EBMLBlockElement(ELEMENTS[i].NAME, id);
                default:
                    FATAL("FIXME");
                    break;
            }
        }
    }
#if LOG_NDEBUG == 0
    FATAL("unknown element %#x", id.u64);
    return NULL;
#else
    ERROR("unknown element %#x", id.u64);
    return new EBMLSkipElement("UNKNOWN", id);
#endif
}

sp<EBMLElement> FindEBMLElement(const sp<EBMLMasterElement>& master, uint64_t id) {
    CHECK_EQ(master->type, kEBMLElementMaster);
    List<sp<EBMLElement> >::const_iterator it = master->children.cbegin();
    for (; it != master->children.cend(); ++it) {
        if ((*it)->id.u64 == id) return *it;
    }
    return NULL;
}

sp<EBMLElement> FindEBMLElementInside(const sp<EBMLMasterElement>& master, uint64_t inside, uint64_t target) {
    sp<EBMLMasterElement> target1 = FindEBMLElement(master, inside);
    if (target1 == NULL) return NULL;
    CHECK_EQ(target1->type, kEBMLElementMaster);
    return FindEBMLElement(target1, target);
}

void PrintEBMLElements(const sp<EBMLElement>& elem, size_t level) {
    String line;
    for (size_t i = 0; i < level; ++i) line.append(" ");
    line.append("+");
    line.append(elem->name);
    line.append("\t @ ");
    line.append(elem->string());
    INFO("%s", line.c_str());

    if (elem->type == kEBMLElementMaster) {
        sp<EBMLMasterElement> master = elem;
        List<sp<EBMLElement> >::const_iterator it = master->children.cbegin();
        for (; it != master->children.cend(); ++it) {
            PrintEBMLElements(*it, level + 1);
        }
    }
}

sp<EBMLElement> EnumEBMLElement(sp<Content>& pipe) {
    sp<EBMLMasterElement> top = new EBMLMasterElement("mkv", EBMLIntegerNull);
    sp<EBMLMasterElement> master = top;
    uint64_t n = pipe->size() - pipe->tell();
    for (uint64_t offset = 0; offset < n; ) {
        EBMLInteger id      = EBMLGetCodedInteger(pipe);
        EBMLInteger size    = EBMLGetLength(pipe);

        if (id.u64 == 0 || size.u64 == 0) {
            ERROR("bad file?");
            break;
        }
        offset += (id.length + size.length + size.u64);

        DEBUG("found element %#x[%zu], %" PRIu64 " bytes[%zu]",
                id.u64, id.length,
                size.u64, size.length);

        sp<EBMLElement> element;
        if (id.length == 1 && id.u8 == ID_VOID) {
            pipe->skip(size.u64);
        } else if (id.u32 == ID_SEGMENT) {
            // SEGMENT is too big
            sp<EBMLElement> segment = MakeEBMLElement(ID_SEGMENT);
            master->children.push(segment);
            master = segment;
            offset = 0;
            n = size.u64;
            continue;
        } else {
            //CHECK_LE(size.u32, 1024 * 1024);
            element = MakeEBMLElement(id);
            DEBUG("found %s @ %#x", element->name, pipe->tell());
            if (element == NULL) {
                ERROR("unknown ebml element %#x", id.u64);
                pipe->skip(size.u32);
            } else {
                sp<Buffer> data = pipe->read(size.u32);
                BitReader br(*data);
                if (element->parse(br, data->size()) != OK) {
                    ERROR("ebml element %#x parse failed.", id.u64);
                }
            }
        }

        if (element != NULL) {
            master->children.push(element);
        }
    }
    return top;
}

sp<EBMLElement> ParseMatroska(sp<Content>& pipe, int64_t *segment_offset, int64_t *clusters_offset) {
    sp<EBMLMasterElement> top = new EBMLMasterElement("mkv", EBMLIntegerNull);
    sp<EBMLMasterElement> master = top;
    uint64_t n = pipe->size() - pipe->tell();
    for (uint64_t offset = 0; offset < n; ) {
        EBMLInteger id      = EBMLGetCodedInteger(pipe);
        EBMLInteger size    = EBMLGetLength(pipe);

        if (id == EBMLIntegerNull || size == EBMLIntegerNull) {
            ERROR("bad file?");
            break;
        }
        offset += (id.length + size.length + size.u64);

        DEBUG("found element %#x[%zu], %" PRIu64 " bytes[%zu]",
                id.u64, id.length,
                size.u64, size.length);

        sp<EBMLElement> element;
        if (id.length == 1 && id.u8 == ID_VOID) {   // skip void element
            pipe->skip(size.u64);
        } else if (id.u32 == ID_SEGMENT) {          // enum segment
            if (segment_offset) *segment_offset = pipe->tell();
            // SEGMENT is too big
            sp<EBMLElement> segment = MakeEBMLElement(ID_SEGMENT);
            master->children.push(segment);
            master = segment;
            offset = 0;
            n = size.u64;
        } else if (id.u32 == ID_CLUSTER) {
            INFO("found CLUSTER @ %#x", pipe->tell());
            if (clusters_offset) *clusters_offset = pipe->tell() - id.length - size.length;
            break;
        } else {
            //CHECK_LE(size.u32, 1024 * 1024);
            element = MakeEBMLElement(id);
            DEBUG("found %s @ %#x", element->name, pipe->tell());
            if (element == NULL) {
                ERROR("unknown ebml element %#x", id.u64);
                pipe->skip(size.u32);
            } else {
                sp<Buffer> data = pipe->read(size.u32);
                BitReader br(*data);
                if (element->parse(br, data->size()) != OK) {
                    ERROR("ebml element %#x parse failed.", id.u64);
                } else {
                    master->children.push(element);
                }
            }
        }

        // If all non-CLUSTER precede all CLUSTERs (→ section 5.5),
        // a SEEKHEAD is not really necessary, otherwise, a missing
        // SEEKHEAD leads to long file loading times or the inability
        // to access certain data.
    }
    return top;
}

sp<EBMLElement> ReadEBMLElement(sp<Content>& pipe) {
    // end of pipe
    if (pipe->tell() == pipe->size()) return NULL;

    EBMLInteger id = EBMLGetCodedInteger(pipe);
    EBMLInteger size = EBMLGetLength(pipe);
    if (id == EBMLIntegerNull || size == EBMLIntegerNull) {
        ERROR("bad element");
        return NULL;
    }

    sp<EBMLElement> ebml = MakeEBMLElement(id);
    if (ebml == NULL) {
        ERROR("unknown element id %#x", id.u64);
        return NULL;
    }

    if (pipe->tell() + size.length >= pipe->size()) {
        ERROR("bad pipe");
        return NULL;
    }

    sp<Buffer> data = pipe->read(size.u32);     // u32 is logical ok
    BitReader br(*data);

    if (ebml->parse(br, br.size()) != OK) {
        ERROR("element %s parse failed", ebml->name);
        return NULL;
    }
    return ebml;
}

__END_NAMESPACE(EBML)
__END_NAMESPACE_MPX
