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
//#define LOG_NDEBUG 0
#include "MediaTypes.h"

#include <MediaFramework/MediaPacketizer.h>

#include <MediaFramework/MediaFile.h>
#include "mpeg4/Audio.h"
#include "mpeg4/Video.h"
#include "mpeg4/Systems.h"
#include "asf/Asf.h"

#include "EBML.h"

// reference:
// https://matroska.org/technical/specs/index.html
// https://matroska.org/files/matroska.pdf

__BEGIN_NAMESPACE_MPX;
__USING_NAMESPACE(EBML);

#define IS_ZERO(x)  ((x) < 0.0000001 || -(x) < 0.0000001)

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
            CHECK_LT(segment_offset + SEEKPOSITION->vint.u64, pipe->length());
            pipe->seek(segment_offset + SEEKPOSITION->vint.u64);
            return ReadEBMLElement(pipe);
        }
    }
    return NULL;
}

// http://haali.su/mkv/codecs.pdf
// https://matroska.org/technical/specs/codecid/index.html
static struct {
    const char *        codec;
    const uint32_t      format;
} kCodecMap[] = {
    // video
    { "V_MPEG4/ISO/AVC",        kVideoCodecH264    },
    { "V_MPEG4/ISO/ASP",        kVideoCodecMPEG4   },
    { "V_MPEGH/ISO/HEVC",       kVideoCodecHEVC    },
    { "V_MPEG4/ISO/",           kVideoCodecMPEG4   },
    { "V_MPEG4/MS/V3",          kVideoCodecMP42    },
    { "V_VP8",                  kVideoCodecVP8     },
    { "V_VP9",                  kVideoCodecVP9     },
    // audio
    { "A_AAC",                  kAudioCodecAAC     },
    { "A_AC3",                  kAudioCodecAC3     },
    { "A_DTS",                  kAudioCodecDTS     },
    { "A_MPEG/L1",              kAudioCodecMP3     },
    { "A_MPEG/L2",              kAudioCodecMP3     },
    { "A_MPEG/L3",              kAudioCodecMP3     },
    // END OF LIST
    { "",                       kAudioCodecUnknown },
};
#define NELEM(x)    sizeof(x)/sizeof(x[0])

static FORCE_INLINE uint32_t GetCodecFormat(const String& codec) {
    for (size_t i = 0; i < NELEM(kCodecMap); ++i) {
        if (codec.startsWith(kCodecMap[i].codec)) {
            return kCodecMap[i].format;
        }
    }
    return kAudioCodecUnknown;
}

struct TOCEntry {
    TOCEntry() : time(0), pos(0) { }
    uint64_t    time;
    int64_t     pos;    // cluster position related to Segment Element
};

struct MatroskaTrack {
    MatroskaTrack() : index(0), format(0),
    frametime(0), timescale(1.0), compAlgo(4),
    decodeTimeCode(0) { }
    size_t                  index;
    
    eCodecType              type;
    union {
        uint32_t            format;     // ID_CODECID
        struct {
            eAudioCodec     format;
            uint32_t        sampleRate;
            uint32_t        channels;
        } a;
        struct {
            eVideoCodec     format;
            uint32_t        width;
            uint32_t        height;
        } v;
    };
    int64_t                 frametime;  // ID_DEFAULTDURATION, not scaled
    double                  timescale;  // ID_TRACKTIMECODESCALE, DEPRECATED
    sp<Buffer>              csd;
    List<TOCEntry>          toc;
    
    uint8_t                 compAlgo;       // ID_CONTENTCOMPALGO, 0 - zlib, 1 - bzlib, 2 - lzo1x, 3 - header strip
    sp<Buffer>              compSettings;   //

    sp<MediaPacketizer>     packetizer;
    
    // for block parse
    int64_t                 decodeTimeCode;
};

// Frames using references should be stored in "coding order".
struct MatroskaPacket : public MediaPacket {
    sp<Buffer>  buffer;

    MatroskaPacket(MatroskaTrack& trak,
            const sp<Buffer>& block,
            int64_t timecode,
            int64_t timescale,
            eFrameType flag) {
        if (trak.compAlgo == 3) { // header strip
            buffer  = new Buffer(trak.compSettings->size() + block->size());
            buffer->write(*trak.compSettings);
            buffer->write(*block);
        } else {
            buffer  = block;
        }
        data        = (uint8_t*)buffer->data();
        size        = buffer->size();
        index       = trak.index;
        
        pts         = MediaTime(timecode / trak.timescale, 1000000000LL / timescale);
        if (trak.frametime) {
            dts     = MediaTime(trak.decodeTimeCode / trak.timescale, 1000000000LL / timescale);
            duration= MediaTime(trak.frametime, 1000000000LL / timescale);
            trak.decodeTimeCode += trak.frametime;
        } else {
            dts     = pts;
            duration= kMediaTimeInvalid;
        }
        type        = flag;
    }
};

#define TIMESCALE_DEF 1000000UL
bool decodeMPEGAudioFrameHeader(const Buffer& frame, uint32_t *sampleRate, uint32_t *numChannels);
struct MatroskaFile : public MediaFile {
    int64_t                 mSegment;   // offset of SEGMENT
    int64_t                 mClusters;  // offset of CLUSTERs
    MediaTime               mDuration;
    uint64_t                mTimeScale;
    sp<Content>             mContent;
    HashTable<size_t, MatroskaTrack> mTracks;
    sp<EBMLMasterElement>   mCluster;
    List<sp<MediaPacket> >  mPackets;

    MatroskaFile() : MediaFile(), mDuration(0), mTimeScale(TIMESCALE_DEF), mContent(NULL) { }

    virtual String string() const { return "MatroskaFile"; }

    MediaError init(sp<Content>& pipe) {
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
#if LOG_NDEBUG == 0
        PrintEBMLElements(SEGMENTINFO);
#endif

        sp<EBMLIntegerElement> TIMECODESCALE = FindEBMLElement(SEGMENTINFO, ID_TIMECODESCALE);
        if (TIMECODESCALE != NULL) {
            DEBUG("timecodescale %" PRId64, TIMECODESCALE->vint.u64);
            mTimeScale = TIMECODESCALE->vint.u64;
        }
        if (!mTimeScale) mTimeScale = TIMESCALE_DEF;

        sp<EBMLFloatElement> DURATION = FindEBMLElement(SEGMENTINFO, ID_DURATION);
        if (DURATION != NULL) {
            mDuration = MediaTime( DURATION->flt * mTimeScale, 1000000000LL);
        }
        DEBUG("duration %.3f(s)", mDuration.seconds());

        // handle SEGMENT
        sp<EBMLMasterElement> TRACKS = getSegmentElement(pipe, SEGMENT, mSegment, ID_TRACKS);
        if (TRACKS == NULL) {
            ERROR("missing TRACKS");
            return kMediaErrorBadFormat;
        }
#if 1//LOG_NDEBUG == 0
        PrintEBMLElements(TRACKS);
#endif

        bool stage2 = false;
        List<sp<EBMLElement> >::const_iterator it = TRACKS->children.cbegin();
        for (; it != TRACKS->children.cend(); ++it) {
            if ((*it)->id.u64 != ID_TRACKENTRY) continue;

            MatroskaTrack trak;
            const sp<EBMLMasterElement> TRACKENTRY = *it;

            sp<EBMLIntegerElement> TRACKNUMBER = FindEBMLElement(TRACKENTRY, ID_TRACKNUMBER);

            sp<EBMLStringElement> CODECID = FindEBMLElement(TRACKENTRY, ID_CODECID);
            sp<EBMLBinaryElement> CODECPRIVATE = FindEBMLElement(TRACKENTRY, ID_CODECPRIVATE);

            INFO("track codec %s", CODECID->str.c_str());
            if (CODECID->str == "V_MS/VFW/FOURCC") {
                BitReader br(CODECPRIVATE->data->data(), CODECPRIVATE->data->size());
                ASF::BITMAPINFOHEADER biHead;
                if (biHead.parse(br) != kMediaNoError) {
                    ERROR("parse BITMAPINFOHEADER failed");
                    continue;
                }
                trak.format = ASF::GetVideoCodec(biHead.biCompression);
            } else {
                trak.format = GetCodecFormat(CODECID->str);
            }
            
            if (trak.format == kAudioCodecUnknown) {
                ERROR("unknown codec %s", CODECID->str.c_str());
                continue;
            }

            sp<EBMLIntegerElement> DEFAULTDURATION = FindEBMLElement(TRACKENTRY, ID_DEFAULTDURATION);
            if (DEFAULTDURATION != NULL) {
                DEBUG("frame time %" PRIu64, DEFAULTDURATION->vint.u64);
                // DEFAULTDURATION is not scaled, scale here
                trak.frametime = DEFAULTDURATION->vint.u64 / mTimeScale;
            }

            sp<EBMLFloatElement> TRACKTIMECODESCALE = FindEBMLElement(TRACKENTRY, ID_TRACKTIMECODESCALE);
            if (TRACKTIMECODESCALE != NULL) {
                DEBUG("track timecodescale %f", TRACKTIMECODESCALE->flt);
                trak.timescale = TRACKTIMECODESCALE->flt;
                if (IS_ZERO(trak.timescale)) {
                    trak.timescale = 1.0f;
                }
            }
            
            if (CODECPRIVATE != NULL) {
                trak.csd = CODECPRIVATE->data;  // handle csd in format()
            }

            sp<EBMLIntegerElement> TRACKTYPE = FindEBMLElement(TRACKENTRY, ID_TRACKTYPE);
            if (TRACKTYPE->vint.u32 & kTrackTypeAudio) trak.type = kCodecTypeAudio;
            else if (TRACKTYPE->vint.u32 & kTrackTypeVideo) trak.type = kCodecTypeVideo;
            else if (TRACKTYPE->vint.u32 & kTrackTypeSubtitle) trak.type = kCodecTypeSubtitle;

            if (trak.type == kCodecTypeAudio) {
                sp<EBMLFloatElement> SAMPLINGFREQUENCY = FindEBMLElementInside(TRACKENTRY, ID_AUDIO, ID_SAMPLINGFREQUENCY);
                sp<EBMLIntegerElement> CHANNELS = FindEBMLElementInside(TRACKENTRY, ID_AUDIO, ID_CHANNELS);

                if (!SAMPLINGFREQUENCY.isNIL())
                    trak.a.sampleRate = SAMPLINGFREQUENCY->flt;
                
                if (!CHANNELS.isNIL())
                    trak.a.channels = CHANNELS->vint.u32;

                // I hate this: for some format, the mandatory properties always missing in header,
                // we have to decode blocks to get these properties
                if (trak.format == kAudioCodecAAC && trak.csd != NULL) {
                    // FIXME: strip audio properties from ADTS headers if csd is not exists
                    // AudioSpecificConfig
                    BitReader br(trak.csd->data(), trak.csd->size());
                    MPEG4::AudioSpecificConfig asc(br);
                    if (asc.valid) {
                        trak.a.channels     = asc.channels;
                        trak.a.sampleRate   = asc.samplingFrequency;
                    } else
                        ERROR("bad AudioSpecificConfig");
                }

                if (trak.a.sampleRate == 0 || trak.a.channels == 0) {
                    WARN("%s: track miss mandatory properties", CODECID->str.c_str());
                    stage2 = true;
                }
            } else if (trak.type == kCodecTypeVideo) {
                // XXX: pixel width vs display width
                sp<EBMLIntegerElement> WIDTH = FindEBMLElementInside(TRACKENTRY, ID_VIDEO, ID_DISPLAYWIDTH);
                if (WIDTH == NULL) WIDTH = FindEBMLElementInside(TRACKENTRY, ID_VIDEO, ID_PIXELWIDTH);
                sp<EBMLIntegerElement> HEIGHT = FindEBMLElementInside(TRACKENTRY, ID_VIDEO, ID_DISPLAYHEIGHT);
                if (HEIGHT == NULL) HEIGHT = FindEBMLElementInside(TRACKENTRY, ID_VIDEO, ID_PIXELHEIGHT);

                trak.v.width   = WIDTH->vint.u32;
                trak.v.height  = HEIGHT->vint.u32;
            } else {
                ERROR("TODO: track type %#x", TRACKTYPE->vint.u32);
            }

            if (trak.format == kAudioCodecMP3) {
                // FIXME: DO we need packetizer or not
                //trak.packetizer = MediaPacketizer::Create(trak.format);
            }
            
            sp<EBMLMasterElement> CONTENTENCODING = FindEBMLElementInside(TRACKENTRY, ID_CONTENTENCODINGS, ID_CONTENTENCODING);
            if (CONTENTENCODING != NULL) {
                sp<EBMLMasterElement> CONTENTCOMPRESSION = FindEBMLElement(CONTENTENCODING, ID_CONTENTCOMPRESSION);
                if (CONTENTCOMPRESSION != NULL) {
                    sp<EBMLIntegerElement> CONTENTCOMPALGO = FindEBMLElement(CONTENTCOMPRESSION, ID_CONTENTCOMPALGO);
                    sp<EBMLBinaryElement> CONTENTCOMPSETTINGS = FindEBMLElement(CONTENTCOMPRESSION, ID_CONTENTCOMPSETTINGS);
                    
                    trak.compAlgo = CONTENTCOMPALGO->vint.u8;
                    if (trak.compAlgo == 3) {
                        CHECK_FALSE(CONTENTCOMPSETTINGS.isNIL());
                        trak.compSettings = CONTENTCOMPSETTINGS->data;
                    }
                }
            }

            trak.index  = mTracks.size();
            mTracks.insert(TRACKNUMBER->vint.u32, trak);
        }

        // CUES
        sp<EBMLMasterElement> CUES = getSegmentElement(pipe, SEGMENT, mSegment, ID_CUES);
#if LOG_NDEBUG == 0
        PrintEBMLElements(CUES);
#endif

        it = CUES->children.cbegin();
        for (; it != CUES->children.cend(); ++it) {     // CUES can be very large, use iterator
            if ((*it)->id.u64 != ID_CUEPOINT) continue;
            
            sp<EBMLMasterElement> CUEPOINT = *it;
            TOCEntry entry;
            
            List<sp<EBMLElement> >::const_iterator it0 = CUEPOINT->children.cbegin();
            for (; it0 != CUEPOINT->children.cend(); ++it0) {
                sp<EBMLIntegerElement> e = *it0;
                
                if (e->id.u64 == ID_CUETIME) {
                    entry.time = e->vint.u64;
                } else if (e->id.u64 == ID_CUETRACKPOSITIONS) {     // may contains multi
                    sp<EBMLIntegerElement> CUETRACK = FindEBMLElement(e, ID_CUETRACK);
                    sp<EBMLIntegerElement> CUECLUSTERPOSITION = FindEBMLElement(e, ID_CUECLUSTERPOSITION);
                    
                    entry.pos = CUECLUSTERPOSITION->vint.u64;
                    mTracks[CUETRACK->vint.u32].toc.push(entry);
                }
            }
        }

        pipe->seek(mClusters);
        mContent    = pipe;

        // stage 2: workarounds for some codec
        // get extra properties using packetizer
        if (stage2) {
            preparePackets();
            for (size_t i = 0; i < mTracks.size(); ++i) {
                MatroskaTrack& trak = mTracks[i];
                if (trak.packetizer == NULL) continue;

                sp<MediaPacket> packet = NULL;
                for (;;) {
                    List<sp<MediaPacket> >::const_iterator it = mPackets.cbegin();
                    for (; it != mPackets.cend(); ++it) {
                        if ((*it)->index == i) {
                            packet = *it;
                            break;
                        }
                    }

                    if (packet != NULL) break;

                    preparePackets();
                }

                trak.a.channels = packet->properties->findInt32(kKeyChannels);
                trak.a.sampleRate = packet->properties->findInt32(kKeySampleRate);
                INFO("real properties: %" PRIu32 " %" PRIu32, trak.a.channels, trak.a.sampleRate);
            }
        }

        return kMediaNoError;
    }

    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyFormat, kFileFormatMkv);
        info->setInt32(kKeyCount, mTracks.size());
        info->setInt64(kKeyDuration, mDuration.useconds());
        HashTable<size_t, MatroskaTrack>::const_iterator it = mTracks.cbegin();
        for (; it != mTracks.cend(); ++it) {
            sp<Message> trakInfo = new Message;
            const MatroskaTrack& trak = it.value();
            trakInfo->setInt32(kKeyCodecType, trak.type);
            trakInfo->setInt32(kKeyFormat, trak.format);
            if (trak.type == kCodecTypeAudio) {
                trakInfo->setInt32(kKeySampleRate, trak.a.sampleRate);
                trakInfo->setInt32(kKeyChannels, trak.a.channels);
            } else if (trak.type == kCodecTypeVideo) {
                trakInfo->setInt32(kKeyWidth, trak.v.width);
                trakInfo->setInt32(kKeyHeight, trak.v.height);
            } else {
                ERROR("FIXME");
                continue;
            }

            // https://haali.su/mkv/codecs.pdf
            // https://tools.ietf.org/id/draft-lhomme-cellar-codec-00.html
            if (trak.csd != NULL) {
                DEBUG("csd: %s", trak.csd->string(true).c_str());
                if (trak.format == kAudioCodecAAC) {
                    // AudioSpecificConfig -> ESDS
                    sp<Buffer> esds = MPEG4::MakeAudioESDS(trak.csd->data(), trak.csd->size());
                    if (esds.isNIL()) {
                        MPEG4::AudioSpecificConfig asc (MPEG4::AOT_AAC_MAIN, trak.a.sampleRate, trak.a.channels);
                        esds = MPEG4::MakeAudioESDS(asc);
                    }
                    trakInfo->setObject(kKeyESDS, esds);
                } else if (trak.format == kVideoCodecH264) {
                    BitReader br(trak.csd->data(), trak.csd->size());
                    MPEG4::AVCDecoderConfigurationRecord avcC(br);
                    if (avcC.valid) {
                        trakInfo->setObject(kKeyavcC, trak.csd);
                    } else {
                        ERROR("bad avcC");
                    }
                } else if (trak.format == kVideoCodecHEVC) {
                    trakInfo->setObject(kKeyhvcC, trak.csd);
                } else if (trak.format == kVideoCodecMPEG4) {
                    trakInfo->setObject(kKeyESDS, trak.csd);
                } else if (trak.format == kVideoCodecMP42) {
                    trakInfo->setObject(kKeyVCM, trak.csd);
                }
            }

            INFO("trak %zu: %s", trak.index, trakInfo->string().c_str());
            String name = String::format("track-%zu", trak.index);
            info->setObject(name, trakInfo);
        }
        INFO("format %s", info->string().c_str());
        return info;
    }
    
    void seek(const MediaTime& time) {
        DEBUG("seek @ %.3fs", time.seconds());
        // seek with the first track who has toc
        HashTable<size_t, MatroskaTrack>::const_iterator it = mTracks.cbegin();
        for (; it != mTracks.cend(); ++it) {
            const MatroskaTrack& trak = it.value();
            if (trak.toc.empty()) continue;
            
            uint64_t timecode = MediaTime(time).scale(1000000000LL / mTimeScale).value * trak.timescale;
            
            List<TOCEntry>::const_iterator it0 = trak.toc.crbegin();
            for (; it0 != trak.toc.crend(); --it0) {
                const TOCEntry& entry = *it0;
                // find the entry with time < timecode
                if (entry.time <= timecode) {
                    DEBUG("seek hit @ %" PRIu64 ", cluster %" PRId64,
                          entry.time, entry.pos);
                    mContent->seek(entry.pos + mSegment);
                    break;
                }
            }
        }
    }

    MediaError preparePackets() {
        // prepare blocks
        if (mCluster == NULL) {
            mCluster = ReadEBMLElement(mContent);
            if (mCluster == NULL || mCluster->id.u64 != ID_CLUSTER) {
                INFO("no more cluster");
                return kMediaErrorNoMoreData;
            }

#if LOG_NDEBUG == 0
            PrintEBMLElements(mCluster);
#endif
        }

        sp<EBMLIntegerElement> TIMECODE = FindEBMLElement(mCluster, ID_TIMECODE);
        List<sp<EBMLElement> >::iterator it = mCluster->children.begin();
        for (; it != mCluster->children.end(); ++it) {
            sp<EBMLElement> Element = (*it);
            if (Element->id.u64 != ID_BLOCKGROUP && Element->id.u64 != ID_SIMPLEBLOCK) {
                continue;
            }
            
            mCluster->children.erase(it);
            sp<EBMLSimpleBlockElement> block;
            if (Element->id.u64 == ID_BLOCKGROUP) {
                sp<EBMLMasterElement> BLOCKGROUP = Element;
                block = FindEBMLElement(BLOCKGROUP, ID_BLOCK);
            } else {
                block = Element;
            }
            
            // handle each blocks
            eFrameType type = kFrameTypeUnknown;
            if (block->Flags & kBlockFlagKey)           type |= kFrameTypeSync;
            if (block->Flags & kBlockFlagDiscardable)   type |= kFrameTypeDisposal;
            if (block->Flags & kBlockFlagInvisible)     type |= kFrameTypeReference;
            
            if (mTracks.find(block->TrackNumber.u32) == NULL) continue;
            MatroskaTrack& trak = mTracks[block->TrackNumber.u32];
            uint64_t timecode = TIMECODE->vint.u64 + block->TimeCode;
            List<sp<Buffer> >::const_iterator it0 = block->data.cbegin();
            for (; it0 != block->data.cend(); ++it0) {

                sp<MatroskaPacket> packet = new MatroskaPacket(trak,
                                                               *it0,
                                                               timecode,
                                                               mTimeScale,
                                                               type);

                if (trak.packetizer != NULL) {
                    if (trak.packetizer->enqueue(packet) != kMediaNoError) {
                        DEBUG("[%zu] packetizer enqueue failed", packet->index);
                    }
                    packet = trak.packetizer->dequeue();
                }

                if (packet != NULL) {
                    packet->index = trak.index;    // fix trak index
                    DEBUG("[%zu] packet %zu bytes", packet->index, packet->size);
                    mPackets.push(packet);
                }

                timecode += trak.frametime;
            }
            break;  // process once block each time.
        }

        // we have finished this cluster
        if (it == mCluster->children.end()) {
            DEBUG("finish with this cluster");
            mCluster.clear();
        }

        return kMediaNoError;
    }

    // https://matroska.org/technical/specs/notes.html#TimecodeScale
    // https://matroska.org/technical/specs/notes.html
    virtual sp<MediaPacket> read(const eReadMode& mode,
            const MediaTime& ts = kMediaTimeInvalid) {
        if (ts != kMediaTimeInvalid) {
            mCluster.clear();
            mPackets.clear();
            seek(ts);
        }

        for (;;) {
            while (mPackets.empty()) {
                if (preparePackets() != kMediaNoError) {
                    break;
                }
            }

            if (mPackets.empty()) {
                INFO("EOS");
                break;
            }

            sp<MediaPacket> packet = mPackets.front();
            mPackets.pop();

            DEBUG("[%zu] packet %zu bytes, pts %.3f, dts %.3f, flags %#x",
                    packet->index,
                    packet->size,
                    packet->pts.seconds(),
                    packet->dts.seconds(),
                    packet->type);
            return packet;
        }

        return NULL;
    }
};

sp<MediaFile> CreateMatroskaFile(sp<Content>& pipe) {
    sp<MatroskaFile> file = new MatroskaFile;
    if (file->init(pipe) == kMediaNoError) return file;
    return NIL;
}

__END_NAMESPACE_MPX
