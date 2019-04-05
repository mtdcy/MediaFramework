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

#include <MediaFramework/MediaDefs.h>

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(MPEG4)

// ISO/IEC 14496-12 ISO base media file format
// http://www.ftyps.com
// ftyp *
struct FileTypeBox {
    String          major_brand;
    uint32_t        minor_version;
    String          compatibles;
    
    // default value.
    FORCE_INLINE FileTypeBox() : major_brand("mp41"), minor_version(0), compatibles("mp41") { }
    FileTypeBox(const BitReader& br, size_t size);
};

// ISO/IEC 14496-12: Section 4.2 Object Structure, Page 11
struct Box : public SharedObject {
    const String    name;
    bool            full;
    bool            container;
    uint8_t         version;
    uint32_t        flags;
    
    FORCE_INLINE Box(const String& _name, bool _full = false, bool _container = false) : name(_name), full(_full), container(_container) { }
    FORCE_INLINE virtual ~Box() { }
    virtual status_t parse(const BitReader&, size_t, const FileTypeBox&);
    virtual void compose(BitWriter&, const FileTypeBox&);
    size_t size() const { return full ? 4 : 0; }
};

struct FullBox : public Box {
    FORCE_INLINE FullBox(const String& _name, bool container = false) : Box(_name, true, container) { }
    FORCE_INLINE virtual ~FullBox() { }
    size_t size() const { return Box::size(); }
};

struct ContainerBox : public Box {
    bool                counted;
    Vector<sp<Box> >    child;
    
    FORCE_INLINE ContainerBox(const String& _name, bool _full = false, bool _counted = false) : Box(_name, _full, true), counted(_counted) { }
    FORCE_INLINE virtual ~ContainerBox() { }
    virtual status_t parse(const BitReader&, size_t, const FileTypeBox&);
    virtual status_t _parse(const BitReader&, size_t, const FileTypeBox&);
    virtual void compose(BitWriter&, const FileTypeBox&);
};

struct FullContainerBox : public ContainerBox {
    FORCE_INLINE FullContainerBox(const String& _name) : ContainerBox(_name, true, false) { }
    FORCE_INLINE virtual ~FullContainerBox() { }
};

struct CountedFullContainerBox : public ContainerBox {
    FORCE_INLINE CountedFullContainerBox(const String& _name) : ContainerBox(_name, true, true) { }
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

BOX_TYPE("moov", MovieBox,              ContainerBox);
BOX_TYPE("trak", TrackBox,              ContainerBox);
BOX_TYPE("mdia", MediaBox,              ContainerBox);
BOX_TYPE("minf", MediaInformationBox,   ContainerBox);
BOX_TYPE("dinf", DataInformationBox,    ContainerBox);
BOX_TYPE("stbl", SampleTableBox,        ContainerBox);

BOX_TYPE("edts", EditBox,               ContainerBox);
BOX_TYPE("udta", UserDataBox,           ContainerBox);
BOX_TYPE("mvex", MovieExtendsBox,       ContainerBox);
BOX_TYPE("moof", MovieFragmentBox,      ContainerBox);
BOX_TYPE("traf", TrackFragmentBox,      ContainerBox);
BOX_TYPE("dref", DataReferenceBox,      CountedFullContainerBox);
BOX_TYPE("stsd", SampleDescriptionBox,  CountedFullContainerBox);

// 'meta'
// isom and quicktime using different semantics for MetaBox
struct MetaBox : public ContainerBox {
    FORCE_INLINE MetaBox() : ContainerBox("meta") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
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
    
    FORCE_INLINE MovieHeaderBox() : FullBox("mvhd") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
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
    
    FORCE_INLINE TrackHeaderBox() : FullBox("tkhd") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);

};

BOX_TYPE("tref", TrackReferenceBox,     ContainerBox);
struct TrackReferenceTypeBox : public Box {
    Vector<uint32_t>    track_IDs;

    FORCE_INLINE TrackReferenceTypeBox(const String& _name) : Box(_name) { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
// ISO/IEC 14496-12: Section 8.6 Track Reference Box, Page 26
BOX_TYPE("hint", TrackReferenceHintBox, TrackReferenceTypeBox);
BOX_TYPE("cdsc", TrackReferenceCdscBox, TrackReferenceTypeBox);
// ISO/IEC 14496-14: Section 5.2 Track Reference Type, Page 13
BOX_TYPE("dpnd", TrackReferenceDpndBox, TrackReferenceTypeBox);
BOX_TYPE("ipir", TrackReferenceIpirBox, TrackReferenceTypeBox);
BOX_TYPE("mpod", TrackReferenceMpodBox, TrackReferenceTypeBox);
BOX_TYPE("sync", TrackReferenceSyncBox, TrackReferenceTypeBox);
BOX_TYPE("chap", TrackReferenceChapBox, TrackReferenceTypeBox);

struct MediaHeaderBox : public FullBox {
    uint64_t            creation_time;
    uint64_t            modification_time;
    uint32_t            timescale;
    uint64_t            duration;
    String              language;
    
    FORCE_INLINE MediaHeaderBox() : FullBox("mdhd") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct HandlerBox : public FullBox {
    String              handler_type;
    String              handler_name;
    
    FORCE_INLINE HandlerBox() : FullBox("hdlr") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct VideoMediaHeaderBox : public FullBox {
    uint16_t            graphicsmode;
    Vector<uint16_t>    opcolor;
    
    FORCE_INLINE VideoMediaHeaderBox() : FullBox("vmhd") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct SoundMediaHeaderBox : public FullBox {
    uint16_t            balance;

    FORCE_INLINE SoundMediaHeaderBox() : FullBox("smhd") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct HintMediaHeaderBox : public FullBox {
    uint16_t            maxPDUsize;
    uint16_t            avgPDUsize;
    uint32_t            maxbitrate;
    uint32_t            avgbitrate;
    
    FORCE_INLINE HintMediaHeaderBox() : FullBox("hmhd") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

BOX_TYPE("nmhb", NullMediaHeaderBox, FullBox);

struct DataEntryUrlBox : public FullBox {
    String              location;

    FORCE_INLINE DataEntryUrlBox() : FullBox("url ") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct DataEntryUrnBox : public FullBox {
    String              urn_name;
    String              location;
    
    FORCE_INLINE DataEntryUrnBox() : FullBox("urn ") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct TimeToSampleBox : public FullBox {
    struct Entry {
        uint32_t        sample_count;
        uint32_t        sample_delta;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE TimeToSampleBox() : FullBox("stts") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct CompositionOffsetBox : public FullBox {
    struct Entry {
        uint32_t        sample_count;
        uint32_t        sample_offset;  // int32_t in version 1
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE CompositionOffsetBox() : FullBox("ctts") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct SampleDependencyTypeBox : public FullBox {
    Vector<uint8_t>     dependency;

    FORCE_INLINE SampleDependencyTypeBox() : FullBox("sdtp") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct SampleEntry : public ContainerBox {
    String              type;
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
    
    FORCE_INLINE SampleEntry(const String& _name, const String& _type) : ContainerBox(_name), type(_type) { }
    FORCE_INLINE virtual ~SampleEntry() { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    status_t _parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct VisualSampleEntry : public SampleEntry {
    FORCE_INLINE VisualSampleEntry(const String& _name) : SampleEntry(_name, "vide") { }
};
struct AudioSampleEntry : public SampleEntry {
    FORCE_INLINE AudioSampleEntry(const String& _name) : SampleEntry(_name, "soun") { }
};
struct HintSampleEntry : public SampleEntry {
    FORCE_INLINE HintSampleEntry(const String& _name) : SampleEntry(_name, "hint") { }
};
struct MpegSampleEntry : public SampleEntry {
    FORCE_INLINE MpegSampleEntry() : SampleEntry("mp4s", "*") { }
};

// https://github.com/macosforge/alac
// 'alac' box inside 'alac', so custom AudioSampleEntry implementation
#if 1
struct ALACAudioSampleEntry : public AudioSampleEntry {
    sp<Buffer> extra;

    FORCE_INLINE ALACAudioSampleEntry() : AudioSampleEntry("alac") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
#else
BOX_TYPE("alac", ALACAudioSampleEntry,          AudioSampleEntry);
#endif

struct CommonBox : public Box {
    sp<Buffer>  data;

    FORCE_INLINE CommonBox(const String& _name, bool full = false) : Box(_name, full) { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct FullCommonBox : public CommonBox {
    FORCE_INLINE FullCommonBox(const String& _name) : CommonBox(_name, true) { }
};

BOX_TYPE("mp4v", MP4VisualSampleEntry,          VisualSampleEntry);
BOX_TYPE("avc1", AVC1SampleEntry,               VisualSampleEntry);
BOX_TYPE("avc2", AVC2SampleEntry,               VisualSampleEntry);
BOX_TYPE("hvc1", HVC1SampleEntry,               VisualSampleEntry);
BOX_TYPE("hev1", HEV1SampleEntry,               VisualSampleEntry);
BOX_TYPE("raw ", RawAudioSampleEntry,           AudioSampleEntry);
BOX_TYPE("twos", TwosAudioSampleEntry,          AudioSampleEntry);
BOX_TYPE("mp4a", MP4AudioSampleEntry,           AudioSampleEntry);
BOX_TYPE("esds", ESDBox,                        FullCommonBox);
BOX_TYPE("avcC", AVCConfigurationBox,           CommonBox);
BOX_TYPE("hvcC", HVCConfigurationBox,           CommonBox);
BOX_TYPE("m4ds", MPEG4ExtensionDescriptorsBox,  CommonBox);
// 3gpp TS 26.244
BOX_TYPE("s263", H263SampleEntry,               VisualSampleEntry);
BOX_TYPE("samr", AMRSampleEntry,                AudioSampleEntry);
BOX_TYPE("d263", H263SpecificBox,               CommonBox);
BOX_TYPE("damr", AMRSpecificBox,                CommonBox);

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
    FORCE_INLINE siDecompressionParam() : ContainerBox("wave") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct SamplingRateBox : public FullBox {
    uint32_t    sampling_rate;

    FORCE_INLINE SamplingRateBox() : FullBox("srat") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct ColourInformationBox : public Box {
    String      colour_type;
    uint16_t    colour_primaries;
    uint16_t    transfer_characteristics;
    uint16_t    matrix_coefficients;
    bool        full_range_flag;
    
    FORCE_INLINE ColourInformationBox() : Box("colr") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct BitRateBox : public Box {
    uint32_t        bufferSizeDB;
    uint32_t        maxBitrate;
    uint32_t        avgBitrate;
    
    FORCE_INLINE BitRateBox() : Box("btrt") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

// unified SampleSizeBox & CompactSampleSizeBox
struct SampleSizeBox : public FullBox {
    uint32_t            sample_size;
    Vector<uint64_t>    entries;
    
    FORCE_INLINE SampleSizeBox(const String& _name) : FullBox(_name) { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
BOX_TYPE("stsz", PreferredSampleSizeBox, SampleSizeBox);
BOX_TYPE("stz2", CompactSampleSizeBox, SampleSizeBox);

struct SampleToChunkBox : public FullBox {
    struct Entry {
        uint32_t        first_chunk;
        uint32_t        samples_per_chunk;
        uint32_t        sample_description_index;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE SampleToChunkBox() : FullBox("stsc") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct ChunkOffsetBox : public FullBox {
    Vector<uint64_t>    entries;

    FORCE_INLINE ChunkOffsetBox(const String& _name) : FullBox(_name) { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
BOX_TYPE("stco", PreferredChunkOffsetBox, ChunkOffsetBox);
BOX_TYPE("co64", LargeChunkOffsetBox, ChunkOffsetBox);

struct SyncSampleBox : public FullBox {
    Vector<uint32_t>    entries;

    FORCE_INLINE SyncSampleBox() : FullBox("stss") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct ShadowSyncSampleBox : public FullBox {
    struct Entry {
        uint32_t        shadowed_sample_number;
        uint32_t        sync_sample_number;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE ShadowSyncSampleBox() : FullBox("stsh") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct DegradationPriorityBox : public FullBox {
    Vector<uint16_t>    entries;

    FORCE_INLINE DegradationPriorityBox() : FullBox("stdp") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct PaddingBitsBox : public FullBox {
    struct Entry {
        uint8_t     pad1;
        uint8_t     pad2;
    };
    Vector<Entry>   entries;
    
    FORCE_INLINE PaddingBitsBox() : FullBox("padb") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct FreeSpaceBox : public Box {
    FORCE_INLINE FreeSpaceBox(const String& _name) : Box(_name) { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
BOX_TYPE("free", FreeBox, FreeSpaceBox);
BOX_TYPE("skip", SkipBox, FreeSpaceBox);

struct EditListBox : public FullBox {
    struct Entry {
        uint64_t        segment_duration;
        int64_t         media_time;
        uint16_t        media_rate_integer;
        uint16_t        media_rate_fraction;
    };
    Vector<Entry>       entries;
    
    FORCE_INLINE EditListBox() : FullBox("elst") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct NoticeBox : public FullBox {
    String              language;
    String              value;
    
    FORCE_INLINE NoticeBox(const String& _name) : FullBox(_name) { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
BOX_TYPE("cprt", CopyrightBox, NoticeBox);
BOX_TYPE("titl", TitleBox, NoticeBox);
BOX_TYPE("dscp", DescriptionBox, NoticeBox);
BOX_TYPE("perf", PerformerBox, NoticeBox);
BOX_TYPE("gnre", GenreBox, NoticeBox);
BOX_TYPE("albm", AlbumBox, NoticeBox);
BOX_TYPE("yrrc", YearBox, NoticeBox);
BOX_TYPE("loci", LocationBox, NoticeBox);
BOX_TYPE("auth", AuthorBox, NoticeBox);

struct MovieExtendsHeaderBox : public FullBox {
    uint64_t    fragment_duration;

    FORCE_INLINE MovieExtendsHeaderBox() : FullBox("mehd") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct TrackExtendsBox : public FullBox {
    uint32_t    track_ID;
    uint32_t    default_sample_description_index;
    uint32_t    default_sample_duration;
    uint32_t    default_sample_size;
    uint32_t    default_sample_flags;
    
    FORCE_INLINE TrackExtendsBox() : FullBox("trex") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct MovieFragmentHeaderBox : public FullBox {
    uint32_t    sequence_number;

    FORCE_INLINE MovieFragmentHeaderBox() : FullBox("mfhd") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct TrackFragmentHeaderBox : public FullBox {
    uint32_t    track_ID;
    uint64_t    base_data_offset;
    uint32_t    sample_description_index;
    uint32_t    default_sample_duration;
    uint32_t    default_sample_size;
    uint32_t    default_sample_flags;
    
    FORCE_INLINE TrackFragmentHeaderBox() : FullBox("tfhd") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct PrimaryItemBox : public FullBox {
    uint16_t        item_ID;

    FORCE_INLINE PrimaryItemBox() : FullBox("pitm") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
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

// mhdr
struct iTunesHeaderBox : public FullBox {
    uint32_t        nextItemID;

    FORCE_INLINE iTunesHeaderBox() : FullBox("mhdr") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

// FIXME: keys box seems have multi semantics
#if 0
struct iTunesKeysBox : public FullBox {
    struct Entry {
        String      Key_namespace;
        sp<Buffer>  Key_value;
    };
    Vector<Entry>   table;
    
    FORCE_INLINE iTunesKeysBox() : FullBox("keys") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
#else
BOX_TYPE("keys", iTunesItemKeysBox, CountedFullContainerBox);
#endif

// mdta
struct iTunesStringBox : public Box {
    String      value;

    FORCE_INLINE iTunesStringBox(const String& _name) : Box(_name) { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
BOX_TYPE("mdta", iTunesMediaDataBox, iTunesStringBox);

// ilst
#if 0
BOX_TYPE("ilst", iTunesItemListBox, ContainerBox);
#else
struct iTunesItemListBox : public ContainerBox {
    Vector<uint32_t>        key_index;

    FORCE_INLINE iTunesItemListBox() : ContainerBox("ilst") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};
#endif

// 'data'
struct iTunesDataBox : public Box {
    uint32_t        Type_indicator;
    uint16_t        Country_indicator;      // ISO 3166
    uint16_t        Language_indicator;     // index or ISO 639-2/T
    sp<Buffer>      Value;
    
    FORCE_INLINE iTunesDataBox() : Box("data") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

// ctry
struct CountryListBox : public FullBox {
    struct Entry {
        Vector<uint16_t>    Countries;
    };
    Vector<Entry>   entries;
    
    FORCE_INLINE CountryListBox() : FullBox("ctry") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

// lang
struct LanguageListBox : public FullBox {
    struct Entry {
        Vector<uint16_t>    Languages;
    };
    Vector<Entry>           entries;
    
    FORCE_INLINE LanguageListBox() : FullBox("lang") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

BOX_TYPE("\xa9nam", iTunesTitleItemBox, ContainerBox);
BOX_TYPE("\xa9too", iTunesEncoderItemBox, ContainerBox);
BOX_TYPE("\xa9""alb", iTunesAlbumItemBox, ContainerBox);
BOX_TYPE("\xa9""ART", iTunesArtistItemBox, ContainerBox);
BOX_TYPE("\xa9""cmt", iTunesCommentItemBox, ContainerBox);
BOX_TYPE("\xa9gen", iTunesGenreItemBox, ContainerBox);
BOX_TYPE("\xa9wrt", iTunesComposerItemBox, ContainerBox);
BOX_TYPE("\xa9""day", iTunesYearItemBox, ContainerBox);
BOX_TYPE("trkn", iTunesTrackNumItemBox, ContainerBox);
BOX_TYPE("disk", iTunesDiskNumItemBox, ContainerBox);
BOX_TYPE("cpil", iTunesCompilationItemBox, ContainerBox);
BOX_TYPE("tmpo", iTunesBPMItemBox, ContainerBox);
BOX_TYPE("pgap", iTunesGaplessPlaybackBox, ContainerBox);

struct iTunesInfomationBox : public FullBox {
    uint32_t        Item_ID;

    FORCE_INLINE iTunesInfomationBox() : FullBox("itif") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct iTunesNameBox : public Box {
    String          Name;

    FORCE_INLINE iTunesNameBox() : Box("name") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct iTunesMeanBox : public Box {
    String          Mean;

    FORCE_INLINE iTunesMeanBox() : Box("mean") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct iTunesKeyDecBox : public Box {
    String          Key_namespace;
    sp<Buffer>      Key_value;
    
    FORCE_INLINE iTunesKeyDecBox() : Box("keyd") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

BOX_TYPE("----", iTunesCustomBox, ContainerBox);

//BOX_TYPE("mebx", TimedMetadataSampleDescriptionBox, CountedFullContainerBox);

struct ObjectDescriptorBox : public FullBox {
    sp<Buffer> iods;

    FORCE_INLINE ObjectDescriptorBox() : FullBox("iods") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

struct ID3v2Box : public FullBox {
    String      language;
    sp<Buffer>  ID3v2data;
    
    FORCE_INLINE ID3v2Box() : FullBox("ID32") { }
    status_t parse(const BitReader&, size_t, const FileTypeBox&);
    void compose(BitWriter&, const FileTypeBox&);
};

sp<Box> MakeBoxByName(const String& name);

bool CheckTrackBox(const sp<TrackBox>& trak);

sp<Box> FindBox(const sp<ContainerBox>& root,
        const String& boxType, size_t index = 0);
sp<Box> FindBox(const sp<ContainerBox>& root,
        const String& first, const String& second);
sp<Box> FindBoxInside(const sp<ContainerBox>& root,
        const String& sub, const String& target);
void PrintBox(const sp<Box>& root);

__END_NAMESPACE(MPEG4)
__END_NAMESPACE_MPX

#endif


