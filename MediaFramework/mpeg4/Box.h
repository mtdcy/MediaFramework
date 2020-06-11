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

const char * BoxName(uint32_t);

// ISO/IEC 14496-12 ISO base media file format
// http://www.ftyps.com
// ftyp *
struct FileTypeBox {
    uint32_t            major_brand;
    uint32_t            minor_version;
    Vector<uint32_t>    compatibles;
    
    // default value.
    FORCE_INLINE FileTypeBox() : major_brand(kBrandTypeMP41), minor_version(0) { }
    FileTypeBox(const BitReader& br, size_t size);
};

// ISO/IEC 14496-12: Section 4.2 Object Structure, Page 11
struct Box : public SharedObject {
    uint32_t        type;
    bool            full;
    bool            container;
    uint8_t         version;
    uint32_t        flags;
    
    FORCE_INLINE Box(uint32_t _type, bool _full = false, bool _container = false) : type(_type), full(_full), container(_container) { }
    FORCE_INLINE virtual ~Box() { }
    virtual MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    virtual void compose(BitWriter&, const FileTypeBox&);
    size_t size() const { return full ? 4 : 0; }
};

struct FullBox : public Box {
    FORCE_INLINE FullBox(uint32_t type, bool container = false) : Box(type, true, container) { }
    FORCE_INLINE virtual ~FullBox() { }
    size_t size() const { return Box::size(); }
};

struct ContainerBox : public Box {
    bool                counted;
    Vector<sp<Box> >    child;
    
    FORCE_INLINE ContainerBox(uint32_t type, bool _full = false, bool _counted = false) : Box(type, _full, true), counted(_counted) { }
    FORCE_INLINE virtual ~ContainerBox() { }
    virtual MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    virtual MediaError _parse(const BitReader&, size_t, const FileTypeBox&);
    virtual void compose(BitWriter&, const FileTypeBox&);
};

struct FullContainerBox : public ContainerBox {
    FORCE_INLINE FullContainerBox(uint32_t type) : ContainerBox(type, true, false) { }
    FORCE_INLINE virtual ~FullContainerBox() { }
};

struct CountedFullContainerBox : public ContainerBox {
    FORCE_INLINE CountedFullContainerBox(uint32_t type) : ContainerBox(type, true, true) { }
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
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct MovieHeaderBox : public FullBox {
    uint64_t    creation_time;
    uint64_t    modification_time;
    uint32_t    timescale;
    uint64_t    duration;
    uint32_t    rate;
    uint16_t    volume;
    uint32_t    next_track_ID;
    
    FORCE_INLINE MovieHeaderBox() : FullBox(kBoxTypeMVHD) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
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
    
    uint64_t    creation_time;      ///< create time in seconds, UTC
    uint64_t    modification_time;  ///< modification time, seconds, UTC
    uint32_t    track_ID;           ///< unique, 1-based index.
    uint64_t    duration;
    uint16_t    layer;
    uint16_t    alternate_group;
    uint16_t    volume;
    uint32_t    width;
    uint32_t    height;
    
    FORCE_INLINE TrackHeaderBox() : FullBox(kBoxTypeTKHD) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);

};

BOX_TYPE(kBoxTypeTREF, TrackReferenceBox,     ContainerBox);
struct TrackReferenceTypeBox : public Box {
    Vector<uint32_t>    track_IDs;

    FORCE_INLINE TrackReferenceTypeBox(uint32_t type) : Box(type) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
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
    uint64_t            creation_time;
    uint64_t            modification_time;
    uint32_t            timescale;
    uint64_t            duration;
    String              language;
    
    FORCE_INLINE MediaHeaderBox() : FullBox(kBoxTypeMDHD) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct HandlerBox : public FullBox {
    uint32_t            handler_type;
    String              handler_name;
    
    FORCE_INLINE HandlerBox() : FullBox(kBoxTypeHDLR) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct VideoMediaHeaderBox : public FullBox {
    uint16_t            graphicsmode;
    Vector<uint16_t>    opcolor;
    
    FORCE_INLINE VideoMediaHeaderBox() : FullBox(kBoxTypeVMHD) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct SoundMediaHeaderBox : public FullBox {
    uint16_t            balance;

    FORCE_INLINE SoundMediaHeaderBox() : FullBox(kBoxTypeSMHD) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct HintMediaHeaderBox : public FullBox {
    uint16_t            maxPDUsize;
    uint16_t            avgPDUsize;
    uint32_t            maxbitrate;
    uint32_t            avgbitrate;
    
    FORCE_INLINE HintMediaHeaderBox() : FullBox(kBoxTypeHMHD) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

BOX_TYPE(kBoxTypeNMHB, NullMediaHeaderBox, FullBox);

struct DataEntryUrlBox : public FullBox {
    String              location;

    FORCE_INLINE DataEntryUrlBox() : FullBox(kBoxTypeURL) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct DataEntryUrnBox : public FullBox {
    String              urntype;
    String              location;
    
    FORCE_INLINE DataEntryUrnBox() : FullBox(kBoxTypeURN) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct TimeToSampleBox : public FullBox {
    struct Entry {
        uint32_t        sample_count;
        uint32_t        sample_delta;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE TimeToSampleBox() : FullBox(kBoxTypeSTTS) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct CompositionOffsetBox : public FullBox {
    struct Entry {
        uint32_t        sample_count;
        int32_t         sample_offset;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE CompositionOffsetBox() : FullBox(kBoxTypeCTTS) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct CompositionToDecodeBox : public FullBox {
    int64_t             compositionToDTSShift;
    int64_t             leastDecodeToDisplayDelta;
    int64_t             greatestDecodeToDisplayDelta;
    int64_t             compositionStartTime;
    int64_t             compositionEndTime;
    
    FORCE_INLINE CompositionToDecodeBox() : FullBox(kBoxTypeCSLG) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct SampleDependencyTypeBox : public FullBox {
    Vector<uint8_t>     dependency;

    FORCE_INLINE SampleDependencyTypeBox() : FullBox(kBoxTypeSDTP) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

enum {
    kMediaTypeVideo     = 'vide',
    kMediaTypeSound     = 'soun',
    kMediaTypeHint      = 'hint',
};

struct SampleEntry : public ContainerBox {
    // TODO: parse different sample entry in sub class
    uint32_t            media_type;
    uint16_t            data_reference_index;
    union {
        struct {
            uint16_t    width;
            uint16_t    height;
            uint32_t    horizresolution;
            uint32_t    vertresolution;
            uint16_t    frame_count;
            uint16_t    depth;
        } visual;
        struct {
            bool        mov;            // is mov or isom
            uint16_t    version;        // mov 0 & 1
            uint16_t    channelcount;
            uint16_t    samplesize;
            uint32_t    samplerate;
            int16_t     compressionID;  // mov 1
            uint16_t    packetsize;     // mov 1
            uint32_t    samplesPerPacket;   // mov 1
            uint32_t    bytesPerPacket;     // mov 1
            uint32_t    bytesPerFrame;      // mov 1
            uint32_t    bytesPerSample;     // mov 1
        } sound;
    };
    // non-trivial
    String              compressorname;     // visual only
    
    FORCE_INLINE SampleEntry(uint32_t type, uint32_t st) : ContainerBox(type), media_type(st) { }
    FORCE_INLINE virtual ~SampleEntry() { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    MediaError _parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct VisualSampleEntry : public SampleEntry {
    FORCE_INLINE VisualSampleEntry(uint32_t type) : SampleEntry(type, kMediaTypeVideo) { }
};
struct AudioSampleEntry : public SampleEntry {
    FORCE_INLINE AudioSampleEntry(uint32_t type) : SampleEntry(type, kMediaTypeSound) { }
};
struct HintSampleEntry : public SampleEntry {
    FORCE_INLINE HintSampleEntry(uint32_t type) : SampleEntry(type, kMediaTypeHint) { }
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
    uint32_t        grouping_type;
    FORCE_INLINE SampleGroupEntry(uint32_t type) : grouping_type(type) { }
    FORCE_INLINE virtual ~SampleGroupEntry() { }
    virtual MediaError parse(const BitReader&, size_t, const FileTypeBox&) = 0;
};

struct CommonBox : public Box {
    sp<Buffer>  data;

    FORCE_INLINE CommonBox(uint32_t type, bool full = false) : Box(type, full) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct FullCommonBox : public CommonBox {
    FORCE_INLINE FullCommonBox(uint32_t type) : CommonBox(type, true) { }
};

#if 0
// do we really need detail of esds here?
// extractor
struct ESDBox : public FullBox {
    sp<ESDescriptor> ES;
    
    FORCE_INLINE ESDBox() : FullBox(kBoxTypeESDS) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
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
    uint16_t        roll_distance;
    FORCE_INLINE RollRecoveryEntry(uint32_t type) : SampleGroupEntry(type) { }
    virtual MediaError parse(const BitReader&, size_t, const FileTypeBox&);
};

struct SampleGroupDescriptionBox : public FullBox {
    uint32_t    grouping_type;
    uint32_t    default_length;     // version 1
    uint32_t    default_sample_description_index; // version 2
    Vector<sp<SampleGroupEntry> > entries;
    
    FORCE_INLINE SampleGroupDescriptionBox() : FullBox(kBoxTypeSGPD) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

// https://github.com/macosforge/alac
// 'alac' box inside 'alac', so custom AudioSampleEntry implementation
#if 1
struct ALACAudioSampleEntry : public AudioSampleEntry {
    sp<Buffer> extra;

    FORCE_INLINE ALACAudioSampleEntry() : AudioSampleEntry(kBoxTypeALAC) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
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
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

enum {
    kBoxTypeSRAT    = 'srat',
    kBoxTypeCOLR    = 'colr',
    kBoxTypeBTRT    = 'btrt',
};

struct SamplingRateBox : public FullBox {
    uint32_t    sampling_rate;

    FORCE_INLINE SamplingRateBox() : FullBox(kBoxTypeSRAT) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

enum {
    kColourTypeNCLX     = 'nclx',   // on-screen colours
    kColourTypeRICC     = 'rICC',   // restricted ICC profile
    kColourTypePROF     = 'prof',   // unrestricted ICC profile
    kColourTypeNCLC     = 'nclc',   // mov color type for video
};

struct ColourInformationBox : public Box {
    uint32_t    colour_type;
    uint16_t    colour_primaries;
    uint16_t    transfer_characteristics;
    uint16_t    matrix_coefficients;
    bool        full_range_flag;
    
    FORCE_INLINE ColourInformationBox() : Box(kBoxTypeCOLR) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct BitRateBox : public Box {
    uint32_t        bufferSizeDB;
    uint32_t        maxBitrate;
    uint32_t        avgBitrate;
    
    FORCE_INLINE BitRateBox() : Box(kBoxTypeBTRT) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

// unified SampleSizeBox & CompactSampleSizeBox
struct SampleSizeBox : public FullBox {
    uint32_t            sample_size;
    Vector<uint64_t>    entries;
    
    FORCE_INLINE SampleSizeBox(uint32_t type) : FullBox(type) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
BOX_TYPE(kBoxTypeSTSZ, PreferredSampleSizeBox, SampleSizeBox);
BOX_TYPE(kBoxTypeSTZ2, CompactSampleSizeBox, SampleSizeBox);

struct SampleToChunkBox : public FullBox {
    struct Entry {
        uint32_t        first_chunk;
        uint32_t        samples_per_chunk;
        uint32_t        sample_description_index;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE SampleToChunkBox() : FullBox(kBoxTypeSTSC) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct ChunkOffsetBox : public FullBox {
    Vector<uint64_t>    entries;

    FORCE_INLINE ChunkOffsetBox(uint32_t type) : FullBox(type) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
BOX_TYPE(kBoxTypeSTCO, PreferredChunkOffsetBox, ChunkOffsetBox);
BOX_TYPE(kBoxTypeCO64, LargeChunkOffsetBox, ChunkOffsetBox);

struct SyncSampleBox : public FullBox {
    Vector<uint32_t>    entries;

    FORCE_INLINE SyncSampleBox() : FullBox(kBoxTypeSTSS) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct ShadowSyncSampleBox : public FullBox {
    struct Entry {
        uint32_t        shadowed_sample_number;
        uint32_t        sync_sample_number;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE ShadowSyncSampleBox() : FullBox(kBoxTypeSTSH) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct DegradationPriorityBox : public FullBox {
    Vector<uint16_t>    entries;

    FORCE_INLINE DegradationPriorityBox() : FullBox(kBoxTypeSTDP) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct PaddingBitsBox : public FullBox {
    struct Entry {
        uint8_t     pad1;
        uint8_t     pad2;
    };
    Vector<Entry>   entries;
    
    FORCE_INLINE PaddingBitsBox() : FullBox(kBoxTypePADB) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct FreeSpaceBox : public Box {
    FORCE_INLINE FreeSpaceBox(uint32_t type) : Box(type) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
BOX_TYPE(kBoxTypeFREE, FreeBox, FreeSpaceBox);
BOX_TYPE(kBoxTypeSKIP, SkipBox, FreeSpaceBox);

struct EditListBox : public FullBox {
    struct Entry {
        uint64_t        segment_duration;
        int64_t         media_time;
        uint16_t        media_rate_integer;
        uint16_t        media_rate_fraction;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE EditListBox() : FullBox(kBoxTypeELST) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct NoticeBox : public FullBox {
    String              language;
    String              value;
    
    FORCE_INLINE NoticeBox(uint32_t type) : FullBox(type) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
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
    uint64_t    fragment_duration;

    FORCE_INLINE MovieExtendsHeaderBox() : FullBox(kBoxTypeMEHD) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct TrackExtendsBox : public FullBox {
    uint32_t    track_ID;
    uint32_t    default_sample_description_index;
    uint32_t    default_sample_duration;
    uint32_t    default_sample_size;
    uint32_t    default_sample_flags;
    
    FORCE_INLINE TrackExtendsBox() : FullBox(kBoxTypeTREX) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct MovieFragmentHeaderBox : public FullBox {
    uint32_t    sequence_number;

    FORCE_INLINE MovieFragmentHeaderBox() : FullBox(kBoxTypeMFHD) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct TrackFragmentHeaderBox : public FullBox {
    uint32_t    track_ID;
    uint64_t    base_data_offset;
    uint32_t    sample_description_index;
    uint32_t    default_sample_duration;
    uint32_t    default_sample_size;
    uint32_t    default_sample_flags;
    
    FORCE_INLINE TrackFragmentHeaderBox() : FullBox(kBoxTypeTFHD) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct PrimaryItemBox : public FullBox {
    uint16_t        item_ID;

    FORCE_INLINE PrimaryItemBox() : FullBox(kBoxTypePITM) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
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
    uint32_t        nextItemID;

    FORCE_INLINE iTunesHeaderBox() : FullBox(kiTunesBoxTypeMHDR) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
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
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
#else
BOX_TYPE(kiTunesBoxTypeKEYS, iTunesItemKeysBox, CountedFullContainerBox);
#endif

// mdta
struct iTunesStringBox : public Box {
    String      value;

    FORCE_INLINE iTunesStringBox(uint32_t type) : Box(type) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
BOX_TYPE(kiTunesBoxTypeMDTA, iTunesMediaDataBox, iTunesStringBox);

// ilst
#if 0
BOX_TYPE(kiTunesBoxTypeILST, iTunesItemListBox, ContainerBox);
#else
struct iTunesItemListBox : public ContainerBox {
    Vector<uint32_t>        key_index;

    FORCE_INLINE iTunesItemListBox() : ContainerBox(kiTunesBoxTypeILST) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
#endif

// 'data'
struct iTunesDataBox : public Box {
    uint32_t        Type_indicator;
    uint16_t        Country_indicator;      // ISO 3166
    uint16_t        Language_indicator;     // index or ISO 639-2/T
    sp<Buffer>      Value;
    
    FORCE_INLINE iTunesDataBox() : Box(kiTunesBoxTypeDATA) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

// ctry
struct CountryListBox : public FullBox {
    struct Entry {
        Vector<uint16_t>    Countries;
    };
    Vector<Entry>   entries;
    
    FORCE_INLINE CountryListBox() : FullBox(kiTunesBoxTypeCTRY) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

// lang
struct LanguageListBox : public FullBox {
    struct Entry {
        Vector<uint16_t>    Languages;
    };
    Vector<Entry>           entries;
    
    FORCE_INLINE LanguageListBox() : FullBox(kiTunesBoxTypeLANG) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
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
    uint32_t        Item_ID;

    FORCE_INLINE iTunesInfomationBox() : FullBox(kiTunesBoxTypeInfomation) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct iTunesNameBox : public Box {
    String          Name;

    FORCE_INLINE iTunesNameBox() : Box(kiTunesBoxTypeName) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct iTunesMeanBox : public Box {
    String          Mean;

    FORCE_INLINE iTunesMeanBox() : Box(kiTunesBoxTypeMean) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct iTunesKeyDecBox : public Box {
    uint32_t        Keytypespace;
    sp<Buffer>      Key_value;
    
    FORCE_INLINE iTunesKeyDecBox() : Box(kiTunesBoxTypeKeyDec) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

BOX_TYPE(kiTunesBoxTypeCustom, iTunesCustomBox, ContainerBox);

//BOX_TYPE("mebx", TimedMetadataSampleDescriptionBox, CountedFullContainerBox);

struct ObjectDescriptorBox : public FullBox {
    sp<Buffer> iods;

    FORCE_INLINE ObjectDescriptorBox() : FullBox(kBoxTypeIODS) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct ID3v2Box : public FullBox {
    String      language;
    sp<Buffer>  ID3v2data;
    
    FORCE_INLINE ID3v2Box() : FullBox(kBoxTypeID32) { }
    MediaError parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

sp<Box> MakeBoxByType(uint32_t);

bool CheckTrackBox(const sp<TrackBox>& trak);

sp<Box> FindBox(const sp<ContainerBox>& root, uint32_t boxType, size_t index = 0);
sp<Box> FindBox2(const sp<ContainerBox>& root, uint32_t first, uint32_t second);
sp<Box> FindBoxInside(const sp<ContainerBox>& root, uint32_t sub, uint32_t target);
void PrintBox(const sp<Box>& root);

__END_NAMESPACE(MPEG4)
__END_NAMESPACE_MPX

#endif


