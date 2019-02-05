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


// File:    MatroskaFile.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG   "Matroska"
#define LOG_NDEBUG 0
#include <MediaToolkit/Toolkit.h>

#include <MediaFramework/MediaTime.h>

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
using namespace mtdcy;
using namespace EBML;

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

static sp<EBMLElement> getSegmentElement(sp<Content>& pipe,
                                         sp<EBMLMasterElement>& top,
                                         int64_t segment,
                                         uint64_t _id) {
    sp<EBMLMasterElement> SEEKHEAD = FindEBMLElementInside(top, ID_SEGMENT, ID_SEEKHEAD);
    if (SEEKHEAD == NULL) return NULL;
    
    EBMLInteger id = _id;
    
    List<sp<EBMLElement> >::iterator it = SEEKHEAD->children.begin();
    for (; it != SEEKHEAD->children.end(); ++it) {
        if ((*it)->id.u64 != ID_SEEK) continue;
        //CHECK_TRUE(ID_SEEK == (*it)->id.u64);
        sp<EBMLIntegerElement> SEEKID = FindEBMLElement(*it, ID_SEEKID);
        if (SEEKID->vint.u64 == _id) {
            sp<EBMLIntegerElement> SEEKPOSITION = FindEBMLElement(*it, ID_SEEKPOSITION);
            CHECK_LT(segment + SEEKPOSITION->vint.u64, pipe->size());
            pipe->seek(segment + SEEKPOSITION->vint.u64);
            return ReadEBMLElement(pipe);
        }
    }
    return NULL;
}

#include <MediaFramework/MediaDefs.h>
// http://haali.su/mkv/codecs.pdf
// https://matroska.org/technical/specs/codecid/index.html
static struct {
    const char *        codec;
    const eCodecFormat  format;
} kCodecMap[] = {
    // video
    { "V_MPEG4/ISO/AVC",        kVideoCodecFormatH264   },
    { "V_MPEG4/ISO/ASP",        kVideoCodecFormatMPEG4  },
    { "V_MPEGH/ISO/HEVC",       kVideoCodecFormatHEVC   },
    // audio
    { "A_AAC",                  kAudioCodecFormatAAC    },
    { "A_DTS",                  kAudioCodecFormatDTS    },
    // END OF LIST
    { "",                       kCodecFormatUnknown     },
};
#define NELEM(x)    sizeof(x)/sizeof(x[0])

static eCodecFormat GetCodecFormat(const String& codec) {
    for (size_t i = 0; i < NELEM(kCodecMap); ++i) {
        if (codec == kCodecMap[i].codec) {
            return kCodecMap[i].format;
        }
    }
    return kCodecFormatUnknown;
}

struct MatroskaTrack {
    MatroskaTrack() : id(0), format(kCodecFormatUnknown),
    frametime(0), timescale(1.0), next_dts(kTimeBegin) { }
    
    size_t              id;         // ID_TRACKNUMBER
    eCodecFormat        format;     // ID_CODECID
    int64_t             frametime;  // ID_DEFAULTDURATION
    double              timescale;  // ID_TRACKTIMECODESCALE, DEPRECATED
    union {
        struct {
            uint32_t    sampleRate;
            uint32_t    channels;
        } a;
        struct {
            uint32_t    width;
            uint32_t    height;
        } v;
    };
    sp<Buffer>          csd;
    List<sp<EBMLBlockElement> >     cache;
    MediaTime           next_dts;
};

struct CUEPoint {
    MediaTime   time;
    size_t      track;
    int64_t     cluster;
    size_t      block;
};

struct Cluster {
};

#if 0
struct WAVEFORMATEX {
    uint16_t    wFormatTag;
    uint16_t    nChannels;
    uint32_t    nSamplesPerSec;
    uint32_t    nAvgBytesPerSec;
    uint16_t    nBlockAlign;
    uint16_t    wBitsPerSample;
    sp<Buffer>  extra;
    WAVEFORMATEX(BitReader& br) {
        wFormatTag      = br.rb16();
        nChannels       = br.rb16();
        nSamplesPerSec  = br.rb32();
        nAvgBytesPerSec = br.rb32();
        nBlockAlign     = br.rb16();
        wBitsPerSample  = br.rb16();
    }
};
#endif

#include <MediaFramework/MediaPacket.h>
struct MatroskaPacket : public MediaPacket {
    sp<Buffer>  buffer;
    MatroskaPacket(sp<Buffer>& _buffer) : MediaPacket(), buffer(_buffer) {
        data    = (uint8_t*)buffer->data();
        size    = buffer->size();
    }
};

#include <MediaFramework/MediaExtractor.h>
#include "mpeg4/Audio.h"
struct MatroskaFile : public MediaExtractor {
    int64_t                 mSegment;
    double                  mDuration;
    uint64_t                mTimeScale;
    sp<Content>             mContent;
    List<MatroskaTrack>             mTracks;
    List<CUEPoint>          mTOC;
    sp<EBMLMasterElement>   mCluster;
    size_t                  mBlock;
    
    MatroskaFile() : mDuration(0), mTimeScale(1.0), mContent(NULL),
    mCluster(NULL), mBlock(0) { }
    
    virtual MediaError init(sp<Content>& pipe, const Message& options) {
        pipe->seek(0);
        sp<EBMLMasterElement> top = ParseMatroska(pipe, &mSegment);
        
        PrintEBMLElements(top);
        
        // check ebml header
        sp<EBMLMasterElement> EBMLHEADER = FindEBMLElement(top, ID_EBMLHEADER);
        if (EBMLHEADER == NULL) {
            ERROR("missing EBMLHEADER");
            return kMediaErrorBadFormat;
        }
        sp<EBMLStringElement> DOCTYPE = FindEBMLElement(EBMLHEADER, ID_DOCTYPE);
        if (DOCTYPE->str != "matroska") {
            ERROR("unknown DOCTYPE %s", DOCTYPE->str.c_str());
            return kMediaErrorNotSupported;
        }
        
        // handle SEGMENTINFO
        sp<EBMLMasterElement> SEGMENTINFO = getSegmentElement(pipe, top, mSegment, ID_SEGMENTINFO);
        if (SEGMENTINFO == NULL) {
            ERROR("missing SEGMENTINFO");
            return kMediaErrorBadFormat;
        }
        PrintEBMLElements(SEGMENTINFO);
        
        sp<EBMLIntegerElement> TIMECODESCALE = FindEBMLElement(SEGMENTINFO, ID_TIMECODESCALE);
        if (TIMECODESCALE != NULL) {
            DEBUG("timecodescale %" PRId64, TIMECODESCALE->vint.u64);
            mTimeScale = TIMECODESCALE->vint.u64;
        }
        
        sp<EBMLFloatElement> DURATION = FindEBMLElement(SEGMENTINFO, ID_DURATION);
        if (DURATION != NULL) {
            mDuration = DURATION->vint.flt * mTimeScale;
        }
        DEBUG("duration %.3f(s)", mDuration / 1E9);
        
        // handle SEGMENT
        sp<EBMLMasterElement> TRACKS = getSegmentElement(pipe, top, mSegment, ID_TRACKS);
        if (TRACKS == NULL) {
            ERROR("missing TRACKS");
            return kMediaErrorBadFormat;
        }
        PrintEBMLElements(TRACKS);
        
        List<sp<EBMLElement> >::const_iterator it = TRACKS->children.cbegin();
        for (; it != TRACKS->children.cend(); ++it) {
            if ((*it)->id.u64 != ID_TRACKENTRY) continue;
            
            MatroskaTrack trak;
            const sp<EBMLMasterElement> TRACKENTRY = *it;
            
            sp<EBMLIntegerElement> TRACKNUMBER = FindEBMLElement(TRACKENTRY, ID_TRACKNUMBER);
            trak.id = TRACKNUMBER->vint.u32;
            
            sp<EBMLStringElement> CODECID = FindEBMLElement(TRACKENTRY, ID_CODECID);
            
            INFO("track codec %s", CODECID->str.c_str());
            trak.format = GetCodecFormat(CODECID->str);
            
            sp<EBMLIntegerElement> DEFAULTDURATION = FindEBMLElement(TRACKENTRY, ID_DEFAULTDURATION);
            if (DEFAULTDURATION != NULL) {
                DEBUG("frame time %" PRIu64, DEFAULTDURATION->vint.u64);
                trak.frametime = DEFAULTDURATION->vint.u64;
            }
            
            sp<EBMLFloatElement> TRACKTIMECODESCALE = FindEBMLElement(TRACKENTRY, ID_TRACKTIMECODESCALE);
            if (TRACKTIMECODESCALE != NULL) {
                DEBUG("track timecodescale %.f", TRACKTIMECODESCALE->vint.flt);
                trak.timescale = TRACKTIMECODESCALE->vint.flt;
            }
             
            sp<EBMLIntegerElement> TRACKTYPE = FindEBMLElement(TRACKENTRY, ID_TRACKTYPE);
            
            if (TRACKTYPE->vint.u32 & kTrackTypeAudio) {
                sp<EBMLIntegerElement> SAMPLINGFREQUENCY = FindEBMLElementInside(TRACKENTRY, ID_AUDIO, ID_SAMPLINGFREQUENCY);
                sp<EBMLIntegerElement> CHANNELS = FindEBMLElementInside(TRACKENTRY, ID_AUDIO, ID_CHANNELS);
                
                trak.a.sampleRate = SAMPLINGFREQUENCY->vint.u32;
                trak.a.channels = CHANNELS->vint.u32;
                
            } else if (TRACKTYPE->vint.u32 & kTrackTypeVideo) {
                sp<EBMLIntegerElement> PIXELWIDTH = FindEBMLElementInside(TRACKENTRY, ID_VIDEO, ID_PIXELWIDTH);
                sp<EBMLIntegerElement> PIXELHEIGHT = FindEBMLElementInside(TRACKENTRY, ID_VIDEO, ID_PIXELHEIGHT);
                
                trak.v.width   = PIXELWIDTH->vint.u32;
                trak.v.height  = PIXELHEIGHT->vint.u32;

            } else {
                ERROR("TODO: track type %#x", TRACKTYPE->vint.u32);
            }
            
            sp<EBMLBinaryElement> CODECPRIVATE = FindEBMLElement(TRACKENTRY, ID_CODECPRIVATE);
            if (CODECPRIVATE != NULL) {
                trak.csd = CODECPRIVATE->data;
            }
            
            mTracks.push(trak);
        }
        
        // CUES
        sp<EBMLMasterElement> CUES = getSegmentElement(pipe, top, mSegment, ID_CUES);
        PrintEBMLElements(CUES);
        
        it = CUES->children.cbegin();
        for (; it != CUES->children.cend(); ++it) {     // CUES can be very large, use iterator
            if ((*it)->id.u64 != ID_CUEPOINT) continue;
            
            CUEPoint point;
            // CUEPOINT
            sp<EBMLIntegerElement> CUETIME = FindEBMLElement(*it, ID_CUETIME);
            point.time = CUETIME->vint.i64;
            sp<EBMLIntegerElement> CUETRACK = FindEBMLElementInside(*it, ID_CUETRACKPOSITIONS, ID_CUETRACK);
            point.track = CUETIME->vint.u32;
            sp<EBMLIntegerElement> CUECLUSTERPOSITION = FindEBMLElementInside(*it, ID_CUETRACKPOSITIONS, ID_CUECLUSTERPOSITION);
            point.cluster = CUECLUSTERPOSITION->vint.u64;
            sp<EBMLIntegerElement> CUERELATIVEPOSITION = FindEBMLElementInside(*it, ID_CUETRACKPOSITIONS, ID_CUERELATIVEPOSITION);
            point.block = CUERELATIVEPOSITION->vint.u32;
            mTOC.push(point);
        }
        
        // CLUSTER: read the first cluster
        const CUEPoint& first = *mTOC.cbegin();
        pipe->seek(first.cluster + mSegment);
        mCluster = ReadEBMLElement(pipe);
        mBlock = 0;
        
        mContent    = pipe;
        
        return kMediaNoError;
    }
    
    virtual Message formats() const {
        Message info;
        info.setInt32(kKeyFormat, kFileFormatMKV);
        info.setInt32(kKeyCount, mTracks.size());
        info.set<MediaTime>(kKeyDuration, mDuration);
        for (size_t i = 0; i < mTracks.size(); ++i) {
            Message trakInfo;
            const MatroskaTrack& trak = mTracks[i];
            trakInfo.setInt32(kKeyFormat, trak.format);
            eCodecType type = GetCodecType(trak.format);
            if (type == kCodecTypeAudio) {
                trakInfo.setInt32(kKeySampleRate, trak.a.sampleRate);
                trakInfo.setInt32(kKeyChannels, trak.a.channels);
            } else if (type == kCodecTypeVideo) {
                trakInfo.setInt32(kKeyWidth, trak.v.width);
                trakInfo.setInt32(kKeyHeight, trak.v.height);
            } else {
                ERROR("FIXME");
            }
            
            // https://haali.su/mkv/codecs.pdf
            // https://tools.ietf.org/id/draft-lhomme-cellar-codec-00.html
            if (trak.csd != NULL) {
                DEBUG("csd: %s", trak.csd->string(true).c_str());
                if (trak.format == kAudioCodecFormatAAC) {
                    // AudioSpecificConfig
                    BitReader br(*trak.csd);
                    MPEG4::AudioSpecificConfig asc(br);
                    if (asc.valid)
                        trakInfo.set<Buffer>(kKeyCodecSpecificData, *trak.csd);
                    else
                        ERROR("bad AudioSpecificConfig");
                } else if (trak.format == kVideoCodecFormatH264) {
                    trakInfo.set<Buffer>(kKeyavcC, *trak.csd);
                } else if (trak.format == kVideoCodecFormatHEVC) {
                    trakInfo.set<Buffer>(kKeyhvcC, *trak.csd);
                }
            }
            
            INFO("trak %zu: %s", i, trakInfo.string().c_str());
            String name = String::format("track-%zu", i);
            info.set<Message>(name, trakInfo);
        }
        INFO("format %s", info.string().c_str());
        return info;
    }
    
    // https://matroska.org/technical/specs/notes.html#TimecodeScale
    // https://matroska.org/technical/specs/notes.html
    virtual sp<MediaPacket> read(size_t index,
                                 eModeReadType mode,
                                 const MediaTime& ts = kTimeInvalid) {
        if (mCluster == NULL) {
            INFO("eos...");
            return NULL;
        }
        
        MatroskaTrack& trak = mTracks[index];
        sp<EBMLBlockElement> block;
        if (trak.cache.size()) {
            block = *trak.cache.begin();
            trak.cache.pop();
        } else {
            for (;;) {
                // get next SIMPLEBLOCK
                block = mCluster->children[mBlock++];
                if (block->id.u64 != ID_SIMPLEBLOCK) continue;
                
                if (block->TrackNumber.u32 != trak.id) {
                    // cache block data
                    MatroskaTrack *other = NULL;
                    for (size_t i = 0; i < mTracks.size(); ++i) {
                        if (mTracks[i].id == block->TrackNumber.u32) {
                            other = &mTracks[i];
                        }
                    }
                    CHECK_NULL(other);
                    other->cache.push(block);
                } else {
                    break;
                }
            }
        }
        
        sp<EBMLIntegerElement> TIMECODE = FindEBMLElement(mCluster, ID_TIMECODE);
        
        sp<MediaPacket> packet = new MatroskaPacket(block->data);
        packet->pts     = MediaTime(((TIMECODE->vint.u64 + block->TimeCode) * mTimeScale) / trak.timescale, 1000000000LL);
        if (trak.frametime > 0) {
            // frametime is not scaled
            packet->dts = trak.next_dts;
            trak.next_dts = packet->dts + MediaTime((trak.frametime) / trak.timescale, 1000000000LL);
        } else {
            packet->dts = packet->pts;
        }
        packet->format  = trak.format;
        packet->flags   = 0;
        if (block->Flags & kBlockFlagKey)           packet->flags |= kFrameFlagSync;
        if (block->Flags & kBlockFlagDiscardable)   packet->flags |= kFrameFlagDisposal;
        
        if (mBlock >= mCluster->children.size()) {
            mCluster = ReadEBMLElement(mContent);
            PrintEBMLElements(mCluster);
            mBlock = 0;
        }
        
        DEBUG("[%zu] packet %zu bytes, pts %.3f, dts %.3f, flags %#x", index,
              packet->size,
              packet->pts.seconds(),
              packet->dts.seconds(),
              packet->flags);
        return packet;
    }
};

namespace mtdcy {
    sp<MediaExtractor> CreateMatroskaFile() {
        return new MatroskaFile;
    }
}

