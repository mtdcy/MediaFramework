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


// File:    EBML.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#ifndef MFWK_EBML_H
#define MFWK_EBML_H

#include <MediaFramework/MediaTypes.h>

__BEGIN_NAMESPACE_MFWK
__BEGIN_NAMESPACE(EBML)

#define ID_VOID                 0xEC

// the EMBL top level element's children
#define ID_EBMLHEADER           0x1A45DFA3
#define ID_EBMLVERSION          0x4286      // uint, #<=1, def:1
#define ID_EBMLREADVERSION      0x42F7      // uint, #<=1, def:1
#define ID_EBMLMAXIDLENGTH      0x42F2      // uint, #<=1, def:4
#define ID_EBMLMAXSIZELENGTH    0x42F3      // uint, #<=1, def:8
#define ID_DOCTYPE              0x4282      // string, #<=1, def:matroska
#define ID_DOCTYPEVERSION       0x4287      // uint, #<=1, def:1
#define ID_DOCTYPEREADVERSION   0x4285      // uint, #<=1, def:1

// the Segment top level element & its children
#define ID_SEGMENT              0x18538067  //
#define ID_SEGMENTINFO          0x1549A966  // master, #=1,
#define ID_SEEKHEAD             0x114D9B74  // master, #>=0,
#define ID_CLUSTER              0x1F43B675  // master, #>=0,
#define ID_TRACKS               0x1654AE6B  // master, #>=0,
#define ID_CUES                 0x1C53BB6B  // master, #<=1,
#define ID_ATTACHMENTS          0x1941A469  // master, #<=1,
#define ID_CHAPTERS             0x1043A770  // master, #=1,
#define ID_TAGS                 0x1254C367  // master, #<=1,

// the SEGMENTINFO element's children
#define ID_SEGMENTUID           0x73A4      // Char[16], #=1,
#define ID_SEGMENTFILENAME      0x7384      // utf8, #<=1,
#define ID_PREVUID              0x3CB923    // Char[16], #<=1,
#define ID_PREVFILENAME         0x3C83AB    // utf8, #<=1
#define ID_NEXTUID              0x3EB923    // Char[16], #<=1
#define ID_NEXTFILENAME         0x3E83BB    // utf8, #<=1
#define ID_TIMECODESCALE        0x2AD7B1    // uint, #<=1
#define ID_DURATION             0x4489      // Float32, #<=1
#define ID_TITLE                0x7BA9      // utf8, #<=1,
#define ID_MUXINGAPP            0x4D80      // string, #=1,
#define ID_WRITINGAPP           0x5741      // utf8, #=1,
#define ID_DATEUTC              0x4461      // Int, #<=1

// the SEEKHEAD element's children
#define ID_SEEK                 0x4DBB      // master, #>=1
// the SEEK element's children
#define ID_SEEKID               0x53AB      // uint, #=1,
#define ID_SEEKPOSITION         0x53AC      // uint, #=1,

// the TRACKS element's children
#define ID_TRACKENTRY           0xAE        // master, #>=1
// the TRACKENTRY element's children
#define ID_TRACKNUMBER          0xD7        // uint, #=1, >0
#define ID_TRACKUID             0x73C5      // uint, #=1, >0
#define ID_TRACKTYPE            0x83        // uint, #=1
#define ID_FLAGENABLED          0xB9        // Bool, #<=1
#define ID_FLAGDEFAULT          0x88        // Bool, #<=1
#define ID_FLAGFORCED           0x55AA      // Bool, #<=1
#define ID_FLAGLACING           0x9C        // Bool, #<=1
#define ID_MINCACHE             0x6DE7      // uint, #<=1
#define ID_MAXCACHE             0x6DF8      // uint, #<=1
#define ID_DEFAULTDURATION      0x23E383    // uint, #<=1
#define ID_TRACKTIMECODESCALE   0x23314F    // Float32, #<=1
#define ID_NAME                 0x536E      // utf8, #<=1
#define ID_LANGUAGE             0x22B59C    // string, #<=1
#define ID_CODECID              0x86        // string, #=1
#define ID_CODECPRIVATE         0x63A2      // binary, #<=1
#define ID_CODECNAME            0x258688    // utf8, #<=1
#define ID_ATTACHMENTLINK       0x7446      // uint
#define ID_VIDEO                0xE0        // master, #<=1
#define ID_AUDIO                0xE1        // master, #<=1
#define ID_CONTENTENCODINGS     0x6D80      // master, #<=1
#define ID_CONTENTENCODING      0x6240      // master, #>=1
#define ID_CONTENTENCODINGORDER 0x5031      // uint, #=1
#define ID_CONTENTENCODINGSCOPE 0x5032      // uint, #=1
#define ID_CONTENTENCODINGTYPE  0x5033      // uint, #=1
#define ID_CONTENTCOMPRESSION   0x5034      // master, #<=1
#define ID_CONTENTCOMPALGO      0x4254      // uint, #=1
#define ID_CONTENTCOMPSETTINGS  0x4255      // binary, #<=1

// the VIDEO element's children
#define ID_PIXELWIDTH           0xB0        // uint, #=1
#define ID_PIXELHEIGHT          0xBA        // uint, #=1
#define ID_PIXELCROPBOTTOM      0x54AA      // uint, #<=1
#define ID_PIXELCROPTOP         0x54BB      // uint, #<=1
#define ID_PIXELCROPLEFT        0x54CC      // uint, #<=1
#define ID_PIXELCROPRIGHT       0x54DD      // uint, #<=1
#define ID_DISPLAYWIDTH         0x54B0      // uint, #<=1
#define ID_DISPLAYHEIGHT        0x54BA      // uint, #<=1
#define ID_DISPLAYUNIT          0x54B2      // uint, #<=1
#define ID_FLAGINTERLACED       0x9A        // uint, #<=1

// the AUDIO element's children
#define ID_SAMPLINGFREQUENCY    0xB5        // uint, #<=1, def:8k
#define ID_OUTPUTSAMPLINGFREQUENCY  0x78B5  // uint, #<=1
#define ID_CHANNELS             0x9F        // uint, #<=1, def:1
#define ID_BITDEPTH             0x6264

// CLUSTER
#define ID_CRC32                0xBF        // uint
#define ID_TIMECODE             0xE7        // uint, #<=1, def:0
#define ID_POSITION             0xA7        // uint, #<=1
#define ID_PREVSIZE             0xAB        // uint, #<=1
#define ID_BLOCKGROUP           0xA0        // master, #>=0
#define ID_SIMPLEBLOCK          0xA3        // binary, #>=0
// BLOCKGROUP
#define ID_BLOCK                0xA1        // binary, #=1
#define ID_BLOCKVIRTUAL         0xA2        // binary, #<=1
#define ID_REFERENCEPRIORITY    0xFA        // uint, #=1, def:0
#define ID_REFERENCEBLOCK       0xFB        // Int, #>=0
#define ID_BLOCKDURATION        0x9B        // Int, #<=1
#define ID_BLOCKADDITIONS       0x75A1      // master, #<=1
#define ID_CODECSTATE           0xA4        // binary, #<=1
#define ID_DISCARDPADDING       0x75A2      // Int, #<=1
// CUES
#define ID_CUEPOINT             0xBB        // master, #>=1
// CUEPOINT
#define ID_CUETIME              0xB3        // uint, #=1
#define ID_CUETRACKPOSITIONS    0xB7        // master, #>=1
// CUETRACKPOSITIONS
#define ID_CUETRACK             0xF7        // uint, #>=1
#define ID_CUECLUSTERPOSITION   0xF1        // uint, #>=1
#define ID_CUERELATIVEPOSITION  0xF0        // uint, #>=0
#define ID_CUEBLOCKNUMBER       0x5378      // uint, #<=1
#define ID_CUECODECSTATE        0xEA        // uint, #<=0, def:0
#define ID_CUEREFERENCE         0xDB        // master, #>=0
// CUEREFERENCE
#define ID_CUEREFTIME           0x96        // uint, #=1
#define ID_CUEREFCLUSTER        0x97        // uint, #=1
#define ID_CUEREFNUMBER         0x535F      // uint, #<=1
#define ID_CUEREFCODECSTATE     0xEB        // uint, #<=1
// TAGS
#define ID_TAG                  0x7373      // master, #>=1
// TAG
#define ID_TARGETS              0x63C0      // master, #<=1
#define ID_SIMPLETAG            0x67C8      // master, #>=1
// TARGETS
#define ID_TARGETTYPEVALUE      0x68CA      // uint, #<=1, def:50
#define ID_TARGETTYPE           0x63CA      // utf8, #<=1
#define ID_TARGETTRACKUID       0x63C5      // uint, #>=0
#define ID_TARGETEDITIONUID     0x63C9      // uint, #>=0
#define ID_TARGETCHAPTERUID     0x63C4      // uint, #>=0
#define ID_ATTACHMENTUID        0x63C6      // uint, #>=0
// SIMPLETAG
#define ID_TAGNAME              0x45A3      // utf8, #>=1
#define ID_TAGLANGUAGE          0x447A      // string, #<=1, def:und
#define ID_TAGORIGINAL          0x4484      // Bool, #<=1, def:1
#define ID_TAGSTRING            0x4487      // utf8, #<=1,
#define ID_TAGBINARY            0x4485      // binary, #<=1
// CHAPTERS
#define ID_EDITIONENTRY         0x45B9      // master, #>=1
// EDITIONENTRY
#define ID_EDITIONUID           0x45BC      // uint, #<=1
#define ID_EDITIONFLAGHIDDEN    0x45BD      // Bool, #<=1, def:0
#define ID_EDITIONFLAGDEFAULT   0x45DB      // Bool, #<=1, def:0
#define ID_EDITIONFLAGORDERED   0x45DD      // Bool, #<=1, def:0
#define ID_CHAPTERATOM          0xB6        // master, #>=1
// CHAPTERATOM
#define ID_CHAPTERUID           0x73C4      // uint, #=1
#define ID_CHAPTERTIMESTART     0x91        // uint, #<=1
#define ID_CHAPTERTIMEEND       0x92        // uint, #<=1
#define ID_CHAPTERFLAGHIDDEN    0x98        // Bool, #<=1, def:0
#define ID_CHAPTERFLAGENABLED   0x4598      // Bool, #<=1, def:1
#define ID_CHAPTERSEGMENTUID    0x6E67      // Char[16], #<=1
#define ID_CHAPTERSEGMENTEDITIONUID 0x6EBC  // uint, #<=1
#define ID_CHAPTERTRACKS        0x8F        // master, #<=1
#define ID_CHAPTERDISPLAY       0x80        // master, #>=0
// CHAPTERTRACKS
#define ID_CHAPTERTRACKNUMBER   0x89        // uint, #>=1
// CHAPTERDISPLAY
#define ID_CHAPSTRING           0x85        // utf8, #<=1
#define ID_CHAPLANGUAGE         0x437C      // string, #>=0, def:eng
#define ID_CHAPCOUNTRY          0x437E      // utf8, #>=0
// ATTACHMENTS
#define ID_ATTACHEDFILE         0x61A7      // master, #>=1
#define ID_FILEDESCRIPTION      0x467E      // utf8, #<=1
#define ID_FILENAME             0x466E      // utf8, #=1
#define ID_FILEMIMETYPE         0x4660      // string, #=1
#define ID_FILEDATA             0x465C      // binary, #=1
#define ID_FILEUID              0x46AE      // uint, #=1

enum eTrackType {
    kTrackTypeVideo     = 0x1,
    kTrackTypeAudio     = 0x2,
    kTrackTypeComplex   = 0x3,
    kTrackTypeLogo      = 0x10,
    kTrackTypeSubtitle  = 0x11,
    kTrackTypeButton    = 0x12,
    kTrackTypeControl   = 0x20
};

enum eTrackFlags {
    kTrackFlagEnabled   = 0x1,
    kTrackFlagDefault   = 0x2,
    kTrackFlagForced    = 0x4,
    kTrackFlagLacing    = 0x8,
};

enum eEBMLElementType {
    kEBMLElementMaster,
    kEBMLElementInteger,
    kEBMLElementSignedInteger,
    kEBMLElementString,
    kEBMLElementUTF8,
    kEBMLElementBinary,
    kEBMLElementFloat,
    kEBMLElementDate,
    // custom type
    kEBMLElementSkip,
    kEBMLElementBlock,
};

struct EBMLInteger {
    EBMLInteger() : u64(0), length(0) { }
    EBMLInteger(UInt64 x);

    union {
        UInt8           u8;
        UInt16          u16;
        UInt32          u32;
        UInt64          u64;
    };
    UInt32              length;
};

struct EBMLSignedInteger {
    EBMLSignedInteger() : i64(0), length(0) { }
    EBMLSignedInteger(Int64 x);

    union {
        Int8            i8;
        Int16           i16;
        Int32           i32;
        Int64           i64;
    };
    UInt32              length;
};

/**
 * @note always use id.u64 & size.u64 when compare
 */
struct EBMLElement : public SharedObject {
    const Char *            name;
    const EBMLInteger       id;
    EBMLInteger             size;
    const eEBMLElementType  type;

    FORCE_INLINE EBMLElement(const Char *_name, const EBMLInteger& _id, EBMLInteger& _size, const eEBMLElementType& _type) :
        name(_name), id(_id), size(_size), type(_type) { }
    FORCE_INLINE virtual ~EBMLElement() { }
    virtual MediaError parse(const sp<ABuffer>&, UInt32) = 0;
    //virtual MediaError compose(sp<ABuffer>&, UInt32) = 0;
    virtual String string() const = 0;
};

struct EBMLIntegerElement : public EBMLElement {
    EBMLInteger         vint;

    FORCE_INLINE EBMLIntegerElement(const Char *name, const EBMLInteger& id, EBMLInteger& size) :
        EBMLElement(name, id, size, kEBMLElementInteger) { }
    virtual MediaError parse(const sp<ABuffer>&, UInt32);
    virtual String string() const;
};

struct EBMLSignedIntegerElement : public EBMLElement {
    EBMLSignedInteger   svint;

    FORCE_INLINE EBMLSignedIntegerElement(const Char *name, const EBMLInteger& id, EBMLInteger& size) :
        EBMLElement(name, id, size, kEBMLElementSignedInteger) { }
    virtual MediaError parse(const sp<ABuffer>&, UInt32);
    virtual String string() const;
};

struct EBMLStringElement : public EBMLElement {
    String              str;

    FORCE_INLINE EBMLStringElement(const Char *name, const EBMLInteger& id, EBMLInteger& size) :
        EBMLElement(name, id, size, kEBMLElementString) { }
    virtual MediaError parse(const sp<ABuffer>&, UInt32);
    virtual String string() const;
};

struct EBMLUTF8Element : public EBMLElement {
    String              utf8;

    FORCE_INLINE EBMLUTF8Element(const Char *name, const EBMLInteger& id, EBMLInteger& size) :
        EBMLElement(name, id, size, kEBMLElementUTF8) { }
    virtual MediaError parse(const sp<ABuffer>&, UInt32);
    virtual String string() const;
};

struct EBMLBinaryElement : public EBMLElement {
    sp<Buffer>          data;

    FORCE_INLINE EBMLBinaryElement(const Char *name, const EBMLInteger& id, EBMLInteger& size) :
        EBMLElement(name, id, size, kEBMLElementBinary) { }
    virtual MediaError parse(const sp<ABuffer>&, UInt32);
    virtual String string() const;
};

struct EBMLFloatElement : public EBMLElement {
    Float64              flt;

    FORCE_INLINE EBMLFloatElement(const Char *name, const EBMLInteger& id, EBMLInteger& size) :
        EBMLElement(name, id, size, kEBMLElementFloat) { }
    virtual MediaError parse(const sp<ABuffer>&, UInt32);
    virtual String string() const;
};

struct EBMLMasterElement : public EBMLElement {
    struct Entry {
        Int64         position;
        sp<EBMLElement> element;
        Entry(Int64 pos, const sp<EBMLElement>& e) : position(pos), element(e) { }
    };
    List<Entry>         children;

    FORCE_INLINE EBMLMasterElement(const Char *name, const EBMLInteger& id, EBMLInteger& size) :
        EBMLElement(name, id, size, kEBMLElementMaster) { }
    virtual MediaError parse(const sp<ABuffer>&, UInt32);
    virtual String string() const;
};

struct EBMLSkipElement : public EBMLElement {
    FORCE_INLINE EBMLSkipElement(const Char *name, const EBMLInteger& id, EBMLInteger& size) :
        EBMLElement(name, id, size, kEBMLElementSkip) { }
    virtual MediaError parse(const sp<ABuffer>&, UInt32);
    virtual String string() const;
};

enum eBlockFlags {
    kBlockFlagKey           = 0x80,     ///< key frame @see kFrameFlagSync
    kBlockFlagLace          = 0x6,      ///< frame lacing mask
    kBlockFlagXiph          = 0x2,
    kBlockFlagEBML          = 0x6,
    kBlockFlagFixed         = 0x4,
    kBlockFlagInvisible     = 0x8,      ///< duration == 0 @see kFrameFlagReference
    kBlockFlagDiscardable   = 0x1,      ///< discardable frame when decoder is slow @see kFrameFlagDisposal
};

struct EBMLSimpleBlockElement : public EBMLElement {
    EBMLInteger         TrackNumber;
    Int16               TimeCode;
    UInt8               Flags;
    List<sp<Buffer> >   data;

    FORCE_INLINE EBMLSimpleBlockElement(const Char *name, const EBMLInteger& id, EBMLInteger& size) :
        EBMLElement(name, id, size, kEBMLElementBlock) { }
    virtual MediaError parse(const sp<ABuffer>&, UInt32);
    virtual String string() const;
};

sp<EBMLElement> MakeEBMLElement(const EBMLInteger& id, EBMLInteger& size);

sp<EBMLElement> FindEBMLElement(const sp<EBMLMasterElement>&, UInt64 id);

sp<EBMLElement> FindEBMLElementInside(const sp<EBMLMasterElement>& master, UInt64 inside, UInt64 target);

API_EXPORT void PrintEBMLElements(const sp<EBMLElement>&);

enum {
    kEnumStopCluster    = 0x1,  // enum stop at cluster
    kEnumSkipCluster    = 0x2,  // skip cluster content
    kEnumSkipBlocks     = 0x4,  // skip block content
};

//API_EXPORT sp<EBMLMasterElement> EnumEBMLElements(sp<ABuffer>& pipe, UInt32 flags = kEnumSkipCluster);

//sp<EBMLElement> ParseMatroska(sp<ABuffer>& pipe, Int64 *segment_offset, Int64 *clusters_offset);

API_EXPORT sp<EBMLElement> ReadEBMLElement(const sp<ABuffer>&, UInt32 flags = 0);

Int IsMatroskaFile(const sp<ABuffer>&);

__END_NAMESPACE(EBML)
__END_NAMESPACE_MFWK

#endif // MFWK_EBML_H

