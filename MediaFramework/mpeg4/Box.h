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


// File:    Box.h
// Author:  mtdcy.chen
// Changes:
//          1. 20181120     initial version
//

#ifndef _MEDIA_MODULE_MP4_BOX_H
#define _MEDIA_MODULE_MP4_BOX_H

#include "MediaTypes.h"
#include "mpeg4/Systems.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(MPEG4)

// mp4 read & write in big endian, so no FOURCC macro
enum eBoxType {
    kBoxTypeFTYP    = 'ftyp',
    kBoxTypePDIN    = 'pdin',
    kBoxTypeMOOV    = 'moov',
    kBoxTypeMVHD    = 'mvhd',
    kBoxTypeMETA    = 'meta',
    kBoxTypeTRAK    = 'trak',
    kBoxTypeTKHD    = 'tkhd',
    kBoxTypeTREF    = 'tref',
    kBoxTypeTRGR    = 'trgr',
    kBoxTypeEDTS    = 'edts',
    kBoxTypeELST    = 'elst',
    kBoxTypeMDIA    = 'mdia',
    kBoxTypeMDHD    = 'mdhd',
    kBoxTypeHDLR    = 'hdlr',
    kBoxTypeELNG    = 'elng',
    kBoxTypeMINF    = 'minf',
    kBoxTypeVMHD    = 'vmhd',
    kBoxTypeSMHD    = 'smhd',
    kBoxTypeHMHD    = 'hmhd',
    kBoxTypeSTHD    = 'sthd',
    kBoxTypeNMHD    = 'nmhd',
    kBoxTypeDINF    = 'dinf',
    kBoxTypeDREF    = 'dref',
    kBoxTypeSTBL    = 'stbl',
    kBoxTypeSTSD    = 'stsd',
    kBoxTypeSTTS    = 'stts',
    kBoxTypeCTTS    = 'ctts',
    kBoxTypeCSLG    = 'cslg',
    kBoxTypeSTSC    = 'stsc',
    kBoxTypeSTSZ    = 'stsz',
    kBoxTypeSTZ2    = 'stz2',
    kBoxTypeSTCO    = 'stco',
    kBoxTypeCO64    = 'co64',
    kBoxTypeSTSS    = 'stss',
    kBoxTypeSTSH    = 'stsh',
    kBoxTypePADB    = 'padb',
    kBoxTypeSTDP    = 'stdp',
    kBoxTypeSDTP    = 'sdtp',
    kBoxTypeSBGP    = 'sbgp',
    kBoxTypeSGPD    = 'sgpd',
    kBoxTypeSUBS    = 'subs',
    kBoxTypeSAIZ    = 'saiz',
    kBoxTypeSAIO    = 'saio',
    kBoxTypeUDTA    = 'udta',
    kBoxTypeMVEX    = 'mvex',
    kBoxTypeMEHD    = 'mehd',
    kBoxTypeTREX    = 'trex',
    kBoxTypeLEVA    = 'leva',
    kBoxTypeMOOF    = 'moof',
    kBoxTypeMFHD    = 'mfhd',
    kBoxTypeTRAF    = 'traf',
    kBoxTypeTFHD    = 'tfhd',
    kBoxTypeTRUN    = 'trun',
    kBoxTypeTFDT    = 'tfdt',
    kBoxTypeMFRA    = 'mfra',
    kBoxTypeTFRA    = 'tfra',
    kBoxTypeMFRO    = 'mfro',
    kBoxTypeMDAT    = 'mdat',
    kBoxTypeFREE    = 'free',
    kBoxTypeSKIP    = 'skip',
    kBoxTypeCPRT    = 'cprt',
    kBoxTypeTSEL    = 'tsel',
    kBoxTypeSTRK    = 'strk',
    kBoxTypeSTRI    = 'stri',
    kBoxTypeSTRD    = 'strd',
    kBoxTypeILOC    = 'iloc',
    kBoxTypeIPRO    = 'ipro',
    kBoxTypeSINF    = 'sinf',
    kBoxTypeFRMA    = 'frma',
    kBoxTypeSCHM    = 'schm',
    kBoxTypeSCHI    = 'schi',
    kBoxTypeIINF    = 'iinf',
    kBoxTypeXML     = 'xml ',
    kBoxTypeBXML    = 'bxml',
    kBoxTypePITM    = 'pitm',
    kBoxTypeFILN    = 'filn',
    kBoxTypePAEN    = 'paen',
    kBoxTypeFIRE    = 'fire',
    kBoxTypeFPAR    = 'fpar',
    kBoxTypeFECR    = 'fecr',
    kBoxTypeSEGR    = 'segr',
    kBoxTypeGITN    = 'gitn',
    kBoxTypeIDAT    = 'idat',
    kBoxTypeIREF    = 'iref',
    kBoxTypeMECO    = 'meco',
    kBoxTypeMERE    = 'mere',
    kBoxTypeSTYP    = 'styp',
    kBoxTypeSIDX    = 'sidx',
    kBoxTypeSSIX    = 'ssix',
    kBoxTypePRFT    = 'prft',
    kBoxTypeNMHB    = 'nmhb',
    
    // ...
    kBoxTypeURL     = 'url ',
    kBoxTypeURN     = 'urn ',
    kBoxTypeCTRY    = 'ctry',
    kBoxTypeLANG    = 'lang',
    
    // ...
    kBoxTypeHINT    = 'hint',
    kBoxTypeCDSC    = 'cdsc',
    kBoxTypeDPND    = 'dpnd',
    kBoxTypeIPIR    = 'ipir',
    kBoxTypeMPOD    = 'mpod',
    kBoxTypeSYNC    = 'sync',
    kBoxTypeCHAP    = 'chap',
    
    // ...
    kBoxTypeIODS    = 'iods',
    kBoxTypeID32    = 'ID32',
    
    //
    kBoxTerminator  = '    ',   // with box size == 0
};

enum {
    kBrandTypeISOM      = 'isom',
    kBrandTypeMP41      = 'mp41',
    kBrandTypeMP42      = 'mp42',
    kBrandTypeQuickTime = 'qt  ',
};

const Char * BoxName(UInt32);

enum {
    kBoxFull        = 0x1,
    kBoxContainer   = 0x2,
};

// ISO/IEC 14496-12: Section 4.2 Object Structure, Page 11
struct FileTypeBox;
struct Box : public SharedObject {
    const String    Name;   // for debugging
    const UInt32  Type;
    const UInt8   Class;
    UInt8         Version;
    UInt32        Flags;
    
    Box(UInt32 type, UInt8 cls = 0);
    FORCE_INLINE virtual ~Box() { }
    virtual MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    virtual void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
    UInt32 size() const { return Class & kBoxFull ? 4 : 0; }    // box size excluding header
};

// ISO/IEC 14496-12 ISO base media file format
// http://www.ftyps.com
// ftyp *
struct FileTypeBox : public Box {
    UInt32            major_brand;
    UInt32            minor_version;
    Vector<UInt32>    compatibles;
    
    // default value.
    FORCE_INLINE FileTypeBox() : Box(kBoxTypeFTYP),
    major_brand(kBrandTypeMP41), minor_version(0) { }
    virtual MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
};

struct FullBox : public Box {
    FORCE_INLINE FullBox(UInt32 type, UInt8 cls = 0) : Box(type, cls | kBoxFull) { }
    FORCE_INLINE virtual ~FullBox() { }
    UInt32 size() const { return Box::size(); }
};

struct ContainerBox : public Box {
    const Bool          counted;
    Vector<sp<Box> >    child;
    
    FORCE_INLINE ContainerBox(UInt32 type, UInt8 cls = 0, Bool cnt = False) :
    Box(type, kBoxContainer | cls), counted(cnt) { }
    FORCE_INLINE virtual ~ContainerBox() { }
    virtual MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    virtual MediaError _parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    virtual void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct FullContainerBox : public ContainerBox {
    FORCE_INLINE FullContainerBox(UInt32 type) : ContainerBox(type, kBoxFull) { }
    FORCE_INLINE virtual ~FullContainerBox() { }
};

struct CountedFullContainerBox : public ContainerBox {
    FORCE_INLINE CountedFullContainerBox(UInt32 type) : ContainerBox(type, kBoxFull, True) { }
    FORCE_INLINE virtual ~CountedFullContainerBox() { }
};

// moov *
//  |- mvhd *
//  |- trak *  <audio|video|hint>
//  |   |- tkhd *
//  |   |- mdia *
//  |   |   |- mdhd *
//  |   |   |- hdlr *
//  |   |   |- minf *
//  |   |   |   |- dinf *
//  |   |   |   |   |- dref *
//  |   |   |   |- stbl *
//  |   |   |   |   |- stsd *
//  |   |   |   |   |   |- mp4a! - esds
//  |   |   |   |   |   |- mp4v - esds
//  |   |   |   |   |   |- mp4s - esds
//  |   |   |   |   |   |- avc1 - avcC
//  |   |   |   |   |- stts *
//  |   |   |   |   |- stsc *
//  |   |   |   |   |- stco *
//  |   |   |   |   |- co64
//  |   |   |   |   |- stsz *
//  |   |   |   |   |- stz2
//  |   |   |   |   |- ctts
//  |   |   |   |   |- stss
//  |   |   |   |   |- sdtp
//  |   |   |   |   |- stsh
//  |   |   |   |   |- padb
//  |   |   |   |   |- stdp
//  |   |   |   |   |- sdtp
//  |   |   |   |   |- sbgp
//  |   |   |   |   |- sgpd
//  |   |   |   |   |- subs
//  |   |   |   |- vmhd
//  |   |   |   |- smhd
//  |   |   |   |- hmhd
//  |   |   |   |- nmhd
//  |   |- tref
//  |   |- edts
//  |   |   |- elst
//  |   |  meta!
//  |- mvex
//  |   |- mehd
//  |   |- trex *
//  |- ipmc
//  |- udta
//  |   |- cprt
//  |   |- titl
//  |   |- auth
//  |   |- perf
//  |   |- gnre
//  |   |- yrrc
//  |   |- loci
//  |   |- desc
//  |   |- albm
//  |- iods
//  |- meta!
#define BOX_TYPE(NAME, BOX, BASE)  struct BOX : public BASE { FORCE_INLINE BOX() : BASE(NAME) { } };

BOX_TYPE(kBoxTypeMOOV,  MovieBox,               ContainerBox);
BOX_TYPE(kBoxTypeTRAK,  TrackBox,               ContainerBox);
BOX_TYPE(kBoxTypeMDIA,  MediaBox,               ContainerBox);
BOX_TYPE(kBoxTypeMINF,  MediaInformationBox,    ContainerBox);
BOX_TYPE(kBoxTypeDINF,  DataInformationBox,     ContainerBox);
BOX_TYPE(kBoxTypeSTBL,  SampleTableBox,         ContainerBox);

BOX_TYPE(kBoxTypeEDTS,  EditBox,                ContainerBox);
BOX_TYPE(kBoxTypeUDTA,  UserDataBox,            ContainerBox);
BOX_TYPE(kBoxTypeMVEX,  MovieExtendsBox,        ContainerBox);
BOX_TYPE(kBoxTypeMOOF,  MovieFragmentBox,       ContainerBox);
BOX_TYPE(kBoxTypeTRAF,  TrackFragmentBox,       ContainerBox);
BOX_TYPE(kBoxTypeDREF,  DataReferenceBox,       CountedFullContainerBox);
BOX_TYPE(kBoxTypeSTSD,  SampleDescriptionBox,   CountedFullContainerBox);

// 'meta'
// isom and quicktime using different semantics for MetaBox
struct MetaBox : public ContainerBox {
    FORCE_INLINE MetaBox() : ContainerBox(kBoxTypeMETA) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct MovieHeaderBox : public FullBox {
    UInt64    creation_time;
    UInt64    modification_time;
    UInt32    timescale;
    UInt64    duration;
    UInt32    rate;
    UInt16    volume;
    UInt32    next_track_ID;
    
    FORCE_INLINE MovieHeaderBox() : FullBox(kBoxTypeMVHD) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct TrackHeaderBox : public FullBox {
    /**
     * flag value of track
     * @see Box::flags
     */
    enum {
        Track_enabled       = 0x000001,
        Track_in_movie      = 0x000002,
        Track_in_preview    = 0x000004,
        Track_size_is_aspect_ratio  = 0x000008,
    };
    
    UInt64    creation_time;      ///< create time in seconds, UTC
    UInt64    modification_time;  ///< modification time, seconds, UTC
    UInt32    track_ID;           ///< unique, 1-based index.
    UInt64    duration;
    UInt16    layer;
    UInt16    alternate_group;
    UInt16    volume;
    UInt32    width;
    UInt32    height;
    
    FORCE_INLINE TrackHeaderBox() : FullBox(kBoxTypeTKHD) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);

};

BOX_TYPE(kBoxTypeTREF, TrackReferenceBox,     ContainerBox);
struct TrackReferenceTypeBox : public Box {
    Vector<UInt32>    track_IDs;

    FORCE_INLINE TrackReferenceTypeBox(UInt32 type) : Box(type) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

// ISO/IEC 14496-12: Section 8.6 Track Reference Box, Page 26
BOX_TYPE(kBoxTypeHINT, TrackReferenceHintBox, TrackReferenceTypeBox);
BOX_TYPE(kBoxTypeCDSC, TrackReferenceCdscBox, TrackReferenceTypeBox);
// ISO/IEC 14496-14: Section 5.2 Track Reference Type, Page 13
BOX_TYPE(kBoxTypeDPND, TrackReferenceDpndBox, TrackReferenceTypeBox);
BOX_TYPE(kBoxTypeIPIR, TrackReferenceIpirBox, TrackReferenceTypeBox);
BOX_TYPE(kBoxTypeMPOD, TrackReferenceMpodBox, TrackReferenceTypeBox);
BOX_TYPE(kBoxTypeSYNC, TrackReferenceSyncBox, TrackReferenceTypeBox);
BOX_TYPE(kBoxTypeCHAP, TrackReferenceChapBox, TrackReferenceTypeBox);

struct MediaHeaderBox : public FullBox {
    UInt64            creation_time;
    UInt64            modification_time;
    UInt32            timescale;
    UInt64            duration;
    String              language;
    
    FORCE_INLINE MediaHeaderBox() : FullBox(kBoxTypeMDHD) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct HandlerBox : public FullBox {
    UInt32            handler_type;
    String              handler_name;
    
    FORCE_INLINE HandlerBox() : FullBox(kBoxTypeHDLR) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct VideoMediaHeaderBox : public FullBox {
    UInt16            graphicsmode;
    Vector<UInt16>    opcolor;
    
    FORCE_INLINE VideoMediaHeaderBox() : FullBox(kBoxTypeVMHD) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct SoundMediaHeaderBox : public FullBox {
    UInt16            balance;

    FORCE_INLINE SoundMediaHeaderBox() : FullBox(kBoxTypeSMHD) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct HintMediaHeaderBox : public FullBox {
    UInt16            maxPDUsize;
    UInt16            avgPDUsize;
    UInt32            maxbitrate;
    UInt32            avgbitrate;
    
    FORCE_INLINE HintMediaHeaderBox() : FullBox(kBoxTypeHMHD) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

BOX_TYPE(kBoxTypeNMHB, NullMediaHeaderBox, FullBox);

struct DataEntryUrlBox : public FullBox {
    String              location;

    FORCE_INLINE DataEntryUrlBox() : FullBox(kBoxTypeURL) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct DataEntryUrnBox : public FullBox {
    String              urntype;
    String              location;
    
    FORCE_INLINE DataEntryUrnBox() : FullBox(kBoxTypeURN) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct TimeToSampleBox : public FullBox {
    struct Entry {
        UInt32        sample_count;
        UInt32        sample_delta;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE TimeToSampleBox() : FullBox(kBoxTypeSTTS) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct CompositionOffsetBox : public FullBox {
    struct Entry {
        UInt32        sample_count;
        Int32         sample_offset;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE CompositionOffsetBox() : FullBox(kBoxTypeCTTS) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct CompositionToDecodeBox : public FullBox {
    Int64             compositionToDTSShift;
    Int64             leastDecodeToDisplayDelta;
    Int64             greatestDecodeToDisplayDelta;
    Int64             compositionStartTime;
    Int64             compositionEndTime;
    
    FORCE_INLINE CompositionToDecodeBox() : FullBox(kBoxTypeCSLG) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct SampleDependencyTypeBox : public FullBox {
    Vector<UInt8>     dependency;

    FORCE_INLINE SampleDependencyTypeBox() : FullBox(kBoxTypeSDTP) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

enum {
    kMediaTypeVideo     = 'vide',
    kMediaTypeSound     = 'soun',
    kMediaTypeHint      = 'hint',
};

struct SampleEntry : public ContainerBox {
    // TODO: parse different sample entry in sub class
    UInt32            media_type;
    UInt16            data_reference_index;
    union {
        struct {
            UInt16    width;
            UInt16    height;
            UInt32    horizresolution;
            UInt32    vertresolution;
            UInt16    frame_count;
            UInt16    depth;
        } visual;
        struct {
            Bool        mov;            // is mov or isom
            UInt16    version;        // mov 0 & 1
            UInt16    channelcount;
            UInt16    samplesize;
            UInt32    samplerate;
            Int16     compressionID;  // mov 1
            UInt16    packetsize;     // mov 1
            UInt32    samplesPerPacket;   // mov 1
            UInt32    bytesPerPacket;     // mov 1
            UInt32    bytesPerFrame;      // mov 1
            UInt32    bytesPerSample;     // mov 1
        } sound;
    };
    // non-trivial
    String              compressorname;     // visual only
    
    FORCE_INLINE SampleEntry(UInt32 type, UInt32 st) : ContainerBox(type), media_type(st) { }
    FORCE_INLINE virtual ~SampleEntry() { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    MediaError _parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct VisualSampleEntry : public SampleEntry {
    FORCE_INLINE VisualSampleEntry(UInt32 type) : SampleEntry(type, kMediaTypeVideo) { }
};
struct AudioSampleEntry : public SampleEntry {
    FORCE_INLINE AudioSampleEntry(UInt32 type) : SampleEntry(type, kMediaTypeSound) { }
};
struct HintSampleEntry : public SampleEntry {
    FORCE_INLINE HintSampleEntry(UInt32 type) : SampleEntry(type, kMediaTypeHint) { }
};

enum {
    kBoxTypeMP4S    = 'mp4s',
    kBoxTypeALAC    = 'alac',
    kBoxTypeESDS    = 'esds',
    kBoxTypeMP4V    = 'mp4v',
    kBoxTypeAVC1    = 'avc1',
    kBoxTypeAVC2    = 'avc2',
    kBoxTypeHVC1    = 'hvc1',
    kBoxTypeHEV1    = 'hev1',
    kBoxTypeRAW     = 'raw ',
    kBoxTypeTWOS    = 'twos',
    kBoxTypeMP4A    = 'mp4a',
    kBoxTypeAVCC    = 'avcC',
    kBoxTypeHVCC    = 'hvcC',
    kBoxTypeM4DS    = 'm4ds',
    kBoxTypeS263    = 's263',
    kBoxTypeSAMR    = 'samr',
    kBoxTypeD263    = 'd263',
    kBoxTypeDAMR    = 'damr',
    kBoxTypeWAVE    = 'wave',
};

struct MpegSampleEntry : public SampleEntry {
    FORCE_INLINE MpegSampleEntry() : SampleEntry(kBoxTypeMP4S, '****') { }
};

struct SampleGroupEntry : public SharedObject {
    UInt32        grouping_type;
    FORCE_INLINE SampleGroupEntry(UInt32 type) : grouping_type(type) { }
    FORCE_INLINE virtual ~SampleGroupEntry() { }
    virtual MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&) = 0;
};

struct CommonBox : public Box {
    sp<Buffer>  data;

    FORCE_INLINE CommonBox(UInt32 type, Bool full = False) : Box(type, full) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct FullCommonBox : public CommonBox {
    FORCE_INLINE FullCommonBox(UInt32 type) : CommonBox(type, True) { }
};

#if 0
// do we really need detail of esds here?
// extractor
struct ESDBox : public FullBox {
    sp<ESDescriptor> ES;
    
    FORCE_INLINE ESDBox() : FullBox(kBoxTypeESDS) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};
#else
BOX_TYPE(kBoxTypeESDS, ESDBox,                  FullCommonBox);
#endif

BOX_TYPE(kBoxTypeMP4V, MP4VisualSampleEntry,          VisualSampleEntry);
BOX_TYPE(kBoxTypeAVC1, AVC1SampleEntry,               VisualSampleEntry);
BOX_TYPE(kBoxTypeAVC2, AVC2SampleEntry,               VisualSampleEntry);
BOX_TYPE(kBoxTypeHVC1, HVC1SampleEntry,               VisualSampleEntry);
BOX_TYPE(kBoxTypeHEV1, HEV1SampleEntry,               VisualSampleEntry);
BOX_TYPE(kBoxTypeRAW,  RawAudioSampleEntry,           AudioSampleEntry);
BOX_TYPE(kBoxTypeTWOS, TwosAudioSampleEntry,          AudioSampleEntry);
BOX_TYPE(kBoxTypeMP4A, MP4AudioSampleEntry,           AudioSampleEntry);
BOX_TYPE(kBoxTypeAVCC, AVCConfigurationBox,           CommonBox);
BOX_TYPE(kBoxTypeHVCC, HVCConfigurationBox,           CommonBox);
BOX_TYPE(kBoxTypeM4DS, MPEG4ExtensionDescriptorsBox,  CommonBox);
// 3gpp TS 26.244
BOX_TYPE(kBoxTypeS263, H263SampleEntry,               VisualSampleEntry);
BOX_TYPE(kBoxTypeSAMR, AMRSampleEntry,                AudioSampleEntry);
BOX_TYPE(kBoxTypeD263, H263SpecificBox,               CommonBox);
BOX_TYPE(kBoxTypeDAMR, AMRSpecificBox,                CommonBox);

// 'roll' - VisualRollRecoveryEntry
// 'roll' - AudioRollRecoveryEntry
// 'prol' - AudioPreRollEntry
struct RollRecoveryEntry : public SampleGroupEntry {
    UInt16        roll_distance;
    FORCE_INLINE RollRecoveryEntry(UInt32 type) : SampleGroupEntry(type) { }
    virtual MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
};

struct SampleGroupDescriptionBox : public FullBox {
    UInt32    grouping_type;
    UInt32    default_length;     // version 1
    UInt32    default_sample_description_index; // version 2
    Vector<sp<SampleGroupEntry> > entries;
    
    FORCE_INLINE SampleGroupDescriptionBox() : FullBox(kBoxTypeSGPD) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

// https://github.com/macosforge/alac
// 'alac' box inside 'alac', so custom AudioSampleEntry implementation
#if 1
struct ALACAudioSampleEntry : public AudioSampleEntry {
    sp<Buffer> extra;

    FORCE_INLINE ALACAudioSampleEntry() : AudioSampleEntry(kBoxTypeALAC) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};
#else
BOX_TYPE(kBoxTypeALAC, ALACAudioSampleEntry,          AudioSampleEntry);
#endif

// In QuickTime
// mp4a
//  |- wave
//  |   |- frma
//  |   |- mp4a     --> which have different semantics
//  |   |   |- esds
//  |   |- terminator box
//
//
// A 'wave' chunk for 'mp4a' typically contains (in order) at least
// a 'frma' atom, an 'mp4a' atom, an 'esds' atom, and a Terminator Atom
// *BUT* the 'mp4a' atom inside 'wave' seems have different semantics
struct siDecompressionParam : public ContainerBox {
    FORCE_INLINE siDecompressionParam() : ContainerBox(kBoxTypeWAVE) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

enum {
    kBoxTypeSRAT    = 'srat',
    kBoxTypeCOLR    = 'colr',
    kBoxTypeBTRT    = 'btrt',
};

struct SamplingRateBox : public FullBox {
    UInt32    sampling_rate;

    FORCE_INLINE SamplingRateBox() : FullBox(kBoxTypeSRAT) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

enum {
    kColourTypeNCLX     = 'nclx',   // on-screen colours
    kColourTypeRICC     = 'rICC',   // restricted ICC profile
    kColourTypePROF     = 'prof',   // unrestricted ICC profile
    kColourTypeNCLC     = 'nclc',   // mov color type for video
};

struct ColourInformationBox : public Box {
    UInt32    colour_type;
    UInt16    colour_primaries;
    UInt16    transfer_characteristics;
    UInt16    matrix_coefficients;
    Bool        full_range_flag;
    
    FORCE_INLINE ColourInformationBox() : Box(kBoxTypeCOLR) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct BitRateBox : public Box {
    UInt32        bufferSizeDB;
    UInt32        maxBitrate;
    UInt32        avgBitrate;
    
    FORCE_INLINE BitRateBox() : Box(kBoxTypeBTRT) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

// unified SampleSizeBox & CompactSampleSizeBox
struct SampleSizeBox : public FullBox {
    UInt32            sample_size;
    Vector<UInt64>    entries;
    
    FORCE_INLINE SampleSizeBox(UInt32 type) : FullBox(type) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};
BOX_TYPE(kBoxTypeSTSZ, PreferredSampleSizeBox, SampleSizeBox);
BOX_TYPE(kBoxTypeSTZ2, CompactSampleSizeBox, SampleSizeBox);

struct SampleToChunkBox : public FullBox {
    struct Entry {
        UInt32        first_chunk;
        UInt32        samples_per_chunk;
        UInt32        sample_description_index;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE SampleToChunkBox() : FullBox(kBoxTypeSTSC) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct ChunkOffsetBox : public FullBox {
    Vector<UInt64>    entries;

    FORCE_INLINE ChunkOffsetBox(UInt32 type) : FullBox(type) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};
BOX_TYPE(kBoxTypeSTCO, PreferredChunkOffsetBox, ChunkOffsetBox);
BOX_TYPE(kBoxTypeCO64, LargeChunkOffsetBox, ChunkOffsetBox);

struct SyncSampleBox : public FullBox {
    Vector<UInt32>    entries;

    FORCE_INLINE SyncSampleBox() : FullBox(kBoxTypeSTSS) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct ShadowSyncSampleBox : public FullBox {
    struct Entry {
        UInt32        shadowed_sample_number;
        UInt32        sync_sample_number;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE ShadowSyncSampleBox() : FullBox(kBoxTypeSTSH) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct DegradationPriorityBox : public FullBox {
    Vector<UInt16>    entries;

    FORCE_INLINE DegradationPriorityBox() : FullBox(kBoxTypeSTDP) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct PaddingBitsBox : public FullBox {
    struct Entry {
        UInt8     pad1;
        UInt8     pad2;
    };
    Vector<Entry>   entries;
    
    FORCE_INLINE PaddingBitsBox() : FullBox(kBoxTypePADB) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct FreeSpaceBox : public Box {
    FORCE_INLINE FreeSpaceBox(UInt32 type) : Box(type) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};
BOX_TYPE(kBoxTypeFREE, FreeBox, FreeSpaceBox);
BOX_TYPE(kBoxTypeSKIP, SkipBox, FreeSpaceBox);

struct EditListBox : public FullBox {
    struct Entry {
        UInt64        segment_duration;
        Int64         media_time;
        UInt16        media_rate_integer;
        UInt16        media_rate_fraction;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE EditListBox() : FullBox(kBoxTypeELST) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct NoticeBox : public FullBox {
    String              language;
    String              value;
    
    FORCE_INLINE NoticeBox(UInt32 type) : FullBox(type) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

enum {
    //kBoxTypeCPRT    = 'cprt',
    kBoxTypeTITL    = 'titl',
    kBoxTypeDSCP    = 'dscp',
    kBoxTypePERF    = 'perf',
    kBoxTypeGNRE    = 'gnre',
    kBoxTypeALBM    = 'albm',
    kBoxTypeYRRC    = 'yrrc',
    kBoxTypeLOCI    = 'loci',
    kBoxTypeAUTH    = 'auth',
};
BOX_TYPE(kBoxTypeCPRT, CopyrightBox, NoticeBox);
BOX_TYPE(kBoxTypeTITL, TitleBox, NoticeBox);
BOX_TYPE(kBoxTypeDSCP, DescriptionBox, NoticeBox);
BOX_TYPE(kBoxTypePERF, PerformerBox, NoticeBox);
BOX_TYPE(kBoxTypeGNRE, GenreBox, NoticeBox);
BOX_TYPE(kBoxTypeALBM, AlbumBox, NoticeBox);
BOX_TYPE(kBoxTypeYRRC, YearBox, NoticeBox);
BOX_TYPE(kBoxTypeLOCI, LocationBox, NoticeBox);
BOX_TYPE(kBoxTypeAUTH, AuthorBox, NoticeBox);

struct MovieExtendsHeaderBox : public FullBox {
    UInt64    fragment_duration;

    FORCE_INLINE MovieExtendsHeaderBox() : FullBox(kBoxTypeMEHD) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct TrackExtendsBox : public FullBox {
    UInt32    track_ID;
    UInt32    default_sample_description_index;
    UInt32    default_sample_duration;
    UInt32    default_sample_size;
    UInt32    default_sample_flags;
    
    FORCE_INLINE TrackExtendsBox() : FullBox(kBoxTypeTREX) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct MovieFragmentHeaderBox : public FullBox {
    UInt32    sequence_number;

    FORCE_INLINE MovieFragmentHeaderBox() : FullBox(kBoxTypeMFHD) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct TrackFragmentHeaderBox : public FullBox {
    UInt32    track_ID;
    UInt64    base_data_offset;
    UInt32    sample_description_index;
    UInt32    default_sample_duration;
    UInt32    default_sample_size;
    UInt32    default_sample_flags;
    
    FORCE_INLINE TrackFragmentHeaderBox() : FullBox(kBoxTypeTFHD) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct PrimaryItemBox : public FullBox {
    UInt16        item_ID;

    FORCE_INLINE PrimaryItemBox() : FullBox(kBoxTypePITM) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

// iTunes MetaData
// https://developer.apple.com/library/content/documentation/QuickTime/QTFF/Metadata/Metadata.html
// http://atomicparsley.sourceforge.net/mpeg-4files.html
//
// meta
//  |- hdlr
//  |- mhdr
//  |- keys
//  |- ilst
//  |   |- '\xa9nam'
//  |   |       |- data
//  |   |       |- itif
//  |   |       |- name
//  |- ctry
//  |- lang

enum {
    // iTunes
    kiTunesBoxTypeMHDR  = 'mhdr',
    kiTunesBoxTypeKEYS  = 'keys',
    kiTunesBoxTypeMDTA  = 'mdta',
    kiTunesBoxTypeILST  = 'ilst',
    kiTunesBoxTypeDATA  = 'data',
    kiTunesBoxTypeCTRY  = 'ctry',
    kiTunesBoxTypeLANG  = 'lang',
    
    // 0xa9(©) - 0x20(' ') = 0x89
    kiTunesBoxTypeTitle         = ' nam' + 0x89000000,
    kiTunesBoxTypeEncoder       = ' too' + 0x89000000,
    kiTunesBoxTypeAlbum         = ' alb' + 0x89000000,
    kiTunesBoxTypeArtist        = ' ART' + 0x89000000,
    kiTunesBoxTypeComment       = ' cmt' + 0x89000000,
    kiTunesBoxTypeGenre         = ' gen' + 0x89000000,
    kiTunesBoxTypeComposer      = ' wrt' + 0x89000000,
    kiTunesBoxTypeYear          = ' day' + 0x89000000,
    kiTunesBoxTypeTrackNum      = 'trkn',
    kiTunesBoxTypeDiskNum       = 'disk',
    kiTunesBoxTypeCompilation   = 'cpil',
    kiTunesBoxTypeBPM           = 'tmpo',
    kiTunesBoxTypeGaplessPlayback   = 'pgap',
    kiTunesBoxTypeInfomation    = 'itif',
    kiTunesBoxTypeName          = 'name',
    kiTunesBoxTypeMean          = 'mean',
    kiTunesBoxTypeKeyDec        = 'kevd',
    kiTunesBoxTypeCustom        = '----',
};

// mhdr
struct iTunesHeaderBox : public FullBox {
    UInt32        nextItemID;

    FORCE_INLINE iTunesHeaderBox() : FullBox(kiTunesBoxTypeMHDR) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

// FIXME: keys box seems have multi semantics
#if 0
struct iTunesKeysBox : public FullBox {
    struct Entry {
        String      Keytypespace;
        sp<Buffer>  Key_value;
    };
    Vector<Entry>   table;
    
    FORCE_INLINE iTunesKeysBox() : FullBox(kiTunesBoxTypeKEYS) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};
#else
BOX_TYPE(kiTunesBoxTypeKEYS, iTunesItemKeysBox, CountedFullContainerBox);
#endif

// mdta
struct iTunesStringBox : public Box {
    String      value;

    FORCE_INLINE iTunesStringBox(UInt32 type) : Box(type) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};
BOX_TYPE(kiTunesBoxTypeMDTA, iTunesMediaDataBox, iTunesStringBox);

// ilst
#if 0
BOX_TYPE(kiTunesBoxTypeILST, iTunesItemListBox, ContainerBox);
#else
struct iTunesItemListBox : public ContainerBox {
    Vector<UInt32>        key_index;

    FORCE_INLINE iTunesItemListBox() : ContainerBox(kiTunesBoxTypeILST) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};
#endif

// 'data'
struct iTunesDataBox : public Box {
    UInt32        Type_indicator;
    UInt16        Country_indicator;      // ISO 3166
    UInt16        Language_indicator;     // index or ISO 639-2/T
    sp<Buffer>      Value;
    
    FORCE_INLINE iTunesDataBox() : Box(kiTunesBoxTypeDATA) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

// ctry
struct CountryListBox : public FullBox {
    struct Entry {
        Vector<UInt16>    Countries;
    };
    Vector<Entry>   entries;
    
    FORCE_INLINE CountryListBox() : FullBox(kiTunesBoxTypeCTRY) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

// lang
struct LanguageListBox : public FullBox {
    struct Entry {
        Vector<UInt16>    Languages;
    };
    Vector<Entry>           entries;
    
    FORCE_INLINE LanguageListBox() : FullBox(kiTunesBoxTypeLANG) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

BOX_TYPE(kiTunesBoxTypeTitle, iTunesTitleItemBox, ContainerBox);
BOX_TYPE(kiTunesBoxTypeEncoder, iTunesEncoderItemBox, ContainerBox);
BOX_TYPE(kiTunesBoxTypeAlbum, iTunesAlbumItemBox, ContainerBox);
BOX_TYPE(kiTunesBoxTypeArtist, iTunesArtistItemBox, ContainerBox);
BOX_TYPE(kiTunesBoxTypeComment, iTunesCommentItemBox, ContainerBox);
BOX_TYPE(kiTunesBoxTypeGenre, iTunesGenreItemBox, ContainerBox);
BOX_TYPE(kiTunesBoxTypeComposer, iTunesComposerItemBox, ContainerBox);
BOX_TYPE(kiTunesBoxTypeYear, iTunesYearItemBox, ContainerBox);
BOX_TYPE(kiTunesBoxTypeTrackNum, iTunesTrackNumItemBox, ContainerBox);
BOX_TYPE(kiTunesBoxTypeDiskNum, iTunesDiskNumItemBox, ContainerBox);
BOX_TYPE(kiTunesBoxTypeCompilation, iTunesCompilationItemBox, ContainerBox);
BOX_TYPE(kiTunesBoxTypeBPM, iTunesBPMItemBox, ContainerBox);
BOX_TYPE(kiTunesBoxTypeGaplessPlayback, iTunesGaplessPlaybackBox, ContainerBox);

struct iTunesInfomationBox : public FullBox {
    UInt32        Item_ID;

    FORCE_INLINE iTunesInfomationBox() : FullBox(kiTunesBoxTypeInfomation) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct iTunesNameBox : public Box {
    String          Name;

    FORCE_INLINE iTunesNameBox() : Box(kiTunesBoxTypeName) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct iTunesMeanBox : public Box {
    String          Mean;

    FORCE_INLINE iTunesMeanBox() : Box(kiTunesBoxTypeMean) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct iTunesKeyDecBox : public Box {
    UInt32        Keytypespace;
    sp<Buffer>      Key_value;
    
    FORCE_INLINE iTunesKeyDecBox() : Box(kiTunesBoxTypeKeyDec) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

BOX_TYPE(kiTunesBoxTypeCustom, iTunesCustomBox, ContainerBox);

//BOX_TYPE("mebx", TimedMetadataSampleDescriptionBox, CountedFullContainerBox);

struct ObjectDescriptorBox : public FullBox {
    sp<Buffer> iods;

    FORCE_INLINE ObjectDescriptorBox() : FullBox(kBoxTypeIODS) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

struct ID3v2Box : public FullBox {
    String      language;
    sp<Buffer>  ID3v2data;
    
    FORCE_INLINE ID3v2Box() : FullBox(kBoxTypeID32) { }
    MediaError parse(const sp<ABuffer>& buffer, const sp<FileTypeBox>&);
    void compose(sp<ABuffer>&, const sp<FileTypeBox>&);
};

sp<Box> MakeBoxByType(UInt32);

// 'mdat' box is very big, NEVER parse/compose directly
struct MediaDataBox : public Box {
    // remember 'mdat' box offset & length
    Int64     offset;     // the offset of first child box
    Int64     length;     // total bytes in 'mdat' box, except box head
    MediaDataBox() : Box(kBoxTypeMDAT) { }
};
sp<Box> ReadBox(const sp<ABuffer>&, const sp<FileTypeBox>& = Nil);

Bool CheckTrackBox(const sp<TrackBox>& trak);

sp<Box> FindBox(const sp<ContainerBox>& root, UInt32 boxType, UInt32 index = 0);
sp<Box> FindBox2(const sp<ContainerBox>& root, UInt32 first, UInt32 second);
sp<Box> FindBoxInside(const sp<ContainerBox>& root, UInt32 sub, UInt32 target);
void PrintBox(const sp<Box>& root);

__END_NAMESPACE(MPEG4)
__END_NAMESPACE_MPX

#endif


