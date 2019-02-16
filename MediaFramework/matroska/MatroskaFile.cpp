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
                                         sp<EBMLMasterElement>& segment,
                                         int64_t segment_offset,
                                         uint64_t id) {
    sp<EBMLElement> ebml = FindEBMLElement(segment, id);
    if (ebml != NULL) return ebml;  // already parsed
    
    // else, go through SEEKHEAD
    sp<EBMLMasterElement> SEEKHEAD = FindEBMLElement(segment, ID_SEEKHEAD);
    if (SEEKHEAD == NULL) return NULL;
    
    List<sp<EBMLElement> >::iterator it = SEEKHEAD->children.begin();
    for (; it != SEEKHEAD->children.end(); ++it) {
        if ((*it)->id.u64 != ID_SEEK) continue;
        //CHECK_TRUE(ID_SEEK == (*it)->id.u64);
        sp<EBMLIntegerElement> SEEKID = FindEBMLElement(*it, ID_SEEKID);
        if (SEEKID->vint.u64 == id) {
            sp<EBMLIntegerElement> SEEKPOSITION = FindEBMLElement(*it, ID_SEEKPOSITION);
            CHECK_LT(segment_offset + SEEKPOSITION->vint.u64, pipe->size());
            pipe->seek(segment_offset + SEEKPOSITION->vint.u64);
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
    { "A_MPEG/L2",              kAudioCodecFormatMP3    },
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

#include <MediaFramework/MediaPacket.h>
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
    List<sp<MediaPacket> >  blocks;
    MediaTime               next_dts;
};

struct TOCEntry {
    TOCEntry() : time(0), track(0), cluster(0), block(0) { }
    uint64_t    time;
    size_t      track;
    int64_t     cluster;
    size_t      block;
};

// Frames using references should be stored in "coding order".
struct MatroskaPacket : public MediaPacket {
    sp<Buffer>  buffer;
    
    MatroskaPacket(MatroskaTrack& trak,
                   const sp<EBMLBlockElement>& block,
                   uint64_t timecode) {
        buffer  = block->data;
        data    = (uint8_t*)buffer->data();
        size    = buffer->size();
        
        pts     = MediaTime(timecode / trak.timescale, 1000000000LL);
#if 0
        dts     = kTimeInvalid;
#else
        if (trak.frametime > 0) {
            dts = trak.next_dts;
            trak.next_dts = dts + MediaTime((trak.frametime) / trak.timescale, 1000000000LL);
        } else {
            dts = pts;
        }
#endif
        
        format  = trak.format;
        flags   = 0;
        if (block->Flags & kBlockFlagKey)           flags |= kFrameFlagSync;
        if (block->Flags & kBlockFlagDiscardable)   flags |= kFrameFlagDisposal;
    }
};

#include <MediaFramework/MediaExtractor.h>
#include "mpeg4/Audio.h"
#include "mpeg4/Video.h"
#define TIMESCALE_DEF 1000000UL
struct MatroskaFile : public MediaExtractor {
    int64_t                 mSegment;   // offset of SEGMENT
    int64_t                 mClusters;  // offset of CLUSTERs
    double                  mDuration;
    uint64_t                mTimeScale;
    sp<Content>             mContent;
    List<MatroskaTrack>     mTracks;
    List<TOCEntry>          mTOC;
    
    MatroskaFile() : mDuration(0), mTimeScale(TIMESCALE_DEF), mContent(NULL)
    { }
    
    virtual MediaError init(sp<Content>& pipe, const Message& options) {
        pipe->seek(0);
        sp<EBMLMasterElement> top = ParseMatroska(pipe, &mSegment, &mClusters);
#if LOG_NDEBUG == 0
        PrintEBMLElements(top);
#endif
        
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
        
        // check SEGMENT
        sp<EBMLMasterElement> SEGMENT = FindEBMLElement(top, ID_SEGMENT);
        if (SEGMENT == NULL) {
            ERROR("missing SEGMENT");
            return kMediaErrorBadFormat;
        }
        
        // handle SEGMENTINFO
        sp<EBMLMasterElement> SEGMENTINFO = getSegmentElement(pipe, SEGMENT, mSegment, ID_SEGMENTINFO);
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
        if (!mTimeScale) mTimeScale = TIMESCALE_DEF;
        
        sp<EBMLFloatElement> DURATION = FindEBMLElement(SEGMENTINFO, ID_DURATION);
        if (DURATION != NULL) {
            mDuration = DURATION->vint.flt * mTimeScale;
        }
        DEBUG("duration %.3f(s)", mDuration / 1E9);
        
        // handle SEGMENT
        sp<EBMLMasterElement> TRACKS = getSegmentElement(pipe, SEGMENT, mSegment, ID_TRACKS);
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
                trak.frametime = DEFAULTDURATION->vint.u64;     // ns, not scaled
            }
            
            sp<EBMLFloatElement> TRACKTIMECODESCALE = FindEBMLElement(TRACKENTRY, ID_TRACKTIMECODESCALE);
            if (TRACKTIMECODESCALE != NULL) {
                DEBUG("track timecodescale %.f", TRACKTIMECODESCALE->vint.flt);
                trak.timescale = TRACKTIMECODESCALE->vint.flt;
            }
             
            sp<EBMLIntegerElement> TRACKTYPE = FindEBMLElement(TRACKENTRY, ID_TRACKTYPE);
            
            if (TRACKTYPE->vint.u32 & kTrackTypeAudio) {
                sp<EBMLFloatElement> SAMPLINGFREQUENCY = FindEBMLElementInside(TRACKENTRY, ID_AUDIO, ID_SAMPLINGFREQUENCY);
                sp<EBMLIntegerElement> CHANNELS = FindEBMLElementInside(TRACKENTRY, ID_AUDIO, ID_CHANNELS);
                
                trak.a.sampleRate = SAMPLINGFREQUENCY->vint.flt;
                trak.a.channels = CHANNELS->vint.u32;
                
                if (trak.a.sampleRate == 0 && trak.format == kAudioCodecFormatAAC) {
                    // XXX: do we have to fix the sample rate here?
                }
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
                trak.csd = CODECPRIVATE->data;  // handle csd in format()
            }
            
            mTracks.push(trak);
        }
        
        // CUES
        sp<EBMLMasterElement> CUES = getSegmentElement(pipe, SEGMENT, mSegment, ID_CUES);
        PrintEBMLElements(CUES);
        
        it = CUES->children.cbegin();
        for (; it != CUES->children.cend(); ++it) {     // CUES can be very large, use iterator
            if ((*it)->id.u64 != ID_CUEPOINT) continue;
            
            TOCEntry entry;
            // CUEPOINT
            sp<EBMLIntegerElement> CUETIME = FindEBMLElement(*it, ID_CUETIME);
            entry.time = CUETIME->vint.u64;
            sp<EBMLIntegerElement> CUETRACK = FindEBMLElementInside(*it, ID_CUETRACKPOSITIONS, ID_CUETRACK);
            entry.track = CUETRACK->vint.u32;
            sp<EBMLIntegerElement> CUECLUSTERPOSITION = FindEBMLElementInside(*it, ID_CUETRACKPOSITIONS, ID_CUECLUSTERPOSITION);
            entry.cluster = CUECLUSTERPOSITION->vint.u64;
            sp<EBMLIntegerElement> CUERELATIVEPOSITION = FindEBMLElementInside(*it, ID_CUETRACKPOSITIONS, ID_CUERELATIVEPOSITION);
            if (CUERELATIVEPOSITION != NULL) {
                entry.block = CUERELATIVEPOSITION->vint.u32;
            }
#if LOG_NDEBUG == 0
            DEBUG("[%zu]: [%zu] %" PRIu64 ", %" PRId64 ", %zu",
                  mTOC.size(), entry.track,
                  entry.time, entry.cluster, entry.block);
#endif
            mTOC.push(entry);
        }
        
        // CLUSTER: read the first cluster
        // XXX: first entry in TOC may not be the first cluster
        const TOCEntry& first = *mTOC.cbegin();
        INFO("search to first cluster %" PRId64 " vs %" PRId64,
             mClusters,
             first.cluster + mSegment);
        pipe->seek(mClusters);
        
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
                continue;
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
                    BitReader br(*trak.csd);
                    MPEG4::AVCDecoderConfigurationRecord avcC(br);
                    if (avcC.valid) {
                        trakInfo.set<Buffer>(kKeyavcC, *trak.csd);
                    } else {
                        ERROR("bad avcC");
                    }
                } else if (trak.format == kVideoCodecFormatHEVC) {
                    trakInfo.set<Buffer>(kKeyhvcC, *trak.csd);
                } else if (trak.format == kVideoCodecFormatMPEG4) {
                    trakInfo.set<Buffer>(kKeyESDS, *trak.csd);
                }
            }
            
            INFO("trak %zu: %s", i, trakInfo.string().c_str());
            String name = String::format("track-%zu", i);
            info.set<Message>(name, trakInfo);
        }
        INFO("format %s", info.string().c_str());
        return info;
    }
    
    MediaError prepareBlocks(MatroskaTrack& trak) {
        while (trak.blocks.empty()) {
            // prepare packets
            sp<EBMLMasterElement> cluster = ReadEBMLElement(mContent);
            if (cluster == NULL || cluster->id.u64 != ID_CLUSTER) {
                INFO("no more cluster");
                break;
            }
            
#if LOG_NDEBUG == 0
            PrintEBMLElements(cluster);
#endif
            sp<EBMLIntegerElement> TIMECODE = FindEBMLElement(cluster, ID_TIMECODE);
            
            List<sp<EBMLElement> >::const_iterator it = cluster->children.cbegin();
            for (; it != cluster->children.cend(); ++it) {
                if ((*it)->id.u64 != ID_SIMPLEBLOCK) continue;
                // TODO: handle BLOCKGROUP
                sp<EBMLBlockElement> block = *it;
                MatroskaTrack* tmp = NULL;
                for (size_t i = 0; i < mTracks.size(); ++i) {
                    if (mTracks[i].id == block->TrackNumber.u32)
                        tmp = &mTracks[i];
                }
                CHECK_NULL(tmp);
                
                uint64_t timecode = (TIMECODE->vint.u64 + block->TimeCode) * mTimeScale;
                tmp->blocks.push(new MatroskaPacket(*tmp, block, timecode));
            }
        }
        return trak.blocks.empty() ? kMediaErrorNoMoreData: kMediaNoError;
    }
    
    // https://matroska.org/technical/specs/notes.html#TimecodeScale
    // https://matroska.org/technical/specs/notes.html
    virtual sp<MediaPacket> read(size_t index,
                                 eModeReadType mode,
                                 const MediaTime& ts = kTimeInvalid) {
        
        MatroskaTrack& trak = mTracks[index];
        
        // TODO: handle seek here
        prepareBlocks(trak);
        
        if (trak.blocks.empty()) {
            INFO("track %zu eos...", index);
            return NULL;
        }
        
        sp<MediaPacket> packet = *trak.blocks.begin();
        trak.blocks.pop();
        
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

