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
#include "MediaDevice.h"

#include "mpeg4/Audio.h"
#include "mpeg4/Video.h"
#include "mpeg4/Systems.h"
#include "microsoft/Microsoft.h"

#include "EBML.h"

// reference:
// https://matroska.org/technical/specs/index.html
// https://matroska.org/files/matroska.pdf

__BEGIN_NAMESPACE_MFWK;
__USING_NAMESPACE(EBML);

#define IS_ZERO(x)  ((x) < 0.0000001 || -(x) < 0.0000001)

// http://haali.su/mkv/codecs.pdf
// https://matroska.org/technical/specs/codecid/index.html
static struct {
    const Char *        codec;
    const UInt32      format;
} kCodecMap[] = {
    // video
    { "V_MPEG4/ISO/AVC",        kVideoCodecH264             },
    { "V_MPEG4/ISO/ASP",        kVideoCodecMPEG4            },
    { "V_MPEGH/ISO/HEVC",       kVideoCodecHEVC             },
    { "V_MPEG4/ISO/",           kVideoCodecMPEG4            },
    { "V_VP8",                  kVideoCodecVP8              },
    { "V_VP9",                  kVideoCodecVP9              },
    { "V_MPEG4/MS/",            kVideoCodecMicrosoftMPEG4   },
    // audio
    { "A_AAC",                  kAudioCodecAAC              },
    { "A_AC3",                  kAudioCodecAC3              },
    { "A_DTS",                  kAudioCodecDTS              },
    { "A_MPEG/L1",              kAudioCodecMP3              },
    { "A_MPEG/L2",              kAudioCodecMP3              },
    { "A_MPEG/L3",              kAudioCodecMP3              },
    // END OF LIST
    { "",                       kAudioCodecUnknown          },
};
#define NELEM(x)    sizeof(x)/sizeof(x[0])

static FORCE_INLINE UInt32 GetCodecFormat(const String& codec) {
    for (UInt32 i = 0; i < NELEM(kCodecMap); ++i) {
        if (codec.startsWith(kCodecMap[i].codec)) {
            return kCodecMap[i].format;
        }
    }
    return kAudioCodecUnknown;
}

struct TOCEntry {
    TOCEntry() : time(0), pos(0) { }
    UInt64    time;
    Int64     pos;    // cluster position related to Segment Element
};

struct MatroskaTrack {
    MatroskaTrack() : index(0), format(0),
    frametime(0), timescale(1.0), compAlgo(4),
    decodeTimeCode(0) { }
    UInt32                  index;
    
    eCodecType              type;
    union {
        UInt32            format;     // ID_CODECID
        struct {
            eAudioCodec     format;
            UInt32        sampleRate;
            UInt32        channels;
        } a;
        struct {
            eVideoCodec     format;
            UInt32        width;
            UInt32        height;
        } v;
    };
    Int64                 frametime;  // ID_DEFAULTDURATION, not scaled
    Float64                  timescale;  // ID_TRACKTIMECODESCALE, DEPRECATED
    sp<Buffer>              csd;
    List<TOCEntry>          toc;
    
    UInt8                 compAlgo;       // ID_CONTENTCOMPALGO, 0 - zlib, 1 - bzlib, 2 - lzo1x, 3 - header strip
    sp<Buffer>              compSettings;   //

    sp<MediaDevice>         packetizer;
    
    // for block parse
    Int64                 decodeTimeCode;
};

// Frames using references should be stored in "coding order".
static sp<MediaFrame> CreatePacket(MatroskaTrack& trak,
                                    sp<Buffer>& block,
                                    Int64 timecode,
                                    Int64 timescale,
                                    eFrameType flag) {
    sp<MediaFrame> packet;
    if (trak.compAlgo == 3) { // header strip
        const UInt32 size = trak.compSettings->size() + block->size();
        packet = MediaFrame::Create(size);
        trak.compSettings->cloneBytes()->readBytes((Char *)packet->planes.buffers[0].data,
                                                   trak.compSettings->size());
        block->readBytes((Char *)packet->planes.buffers[0].data +
                         trak.compSettings->size(), block->size());
        packet->planes.buffers[0].size = size;
    } else {
        packet = MediaFrame::Create(block);
    }
    packet->id          = trak.index;
    
    packet->timecode         = MediaTime(timecode / trak.timescale, 1000000000LL / timescale);
#if 0
    if (trak.frametime) {
        packet->dts         = MediaTime(trak.decodeTimeCode / trak.timescale, 1000000000LL / timescale);
        packet->duration    = MediaTime(trak.frametime, 1000000000LL / timescale);
        trak.decodeTimeCode += trak.frametime;
    } else {
        packet->dts         = MediaTime(timecode / trak.timescale, 1000000000LL / timescale);
        packet->duration    = kMediaTimeInvalid;
    }
#endif
    packet->flags           = flag;
    return packet;
}

sp<EBMLMasterElement> ReadSEGMENT(const sp<ABuffer>& buffer, Int64 * clusters) {
    Int64 offset = buffer->offset();
    sp<EBMLMasterElement> SEGMENT = ReadEBMLElement(buffer, kEnumStopCluster);
    if (SEGMENT.isNil()) return Nil;
    
//#if LOG_NDEBUG == 0
    PrintEBMLElements(SEGMENT);
//#endif
    
    // children inside reference to the segment element position excluding id & size
    offset += SEGMENT->id.length + SEGMENT->size.length;
    
    *clusters = buffer->offset() - offset;
    
    // FIXME: multi SEEKHEAD exists
    sp<EBMLMasterElement> SEEKHEAD = FindEBMLElement(SEGMENT, ID_SEEKHEAD);
    if (SEEKHEAD.isNil()) {
        ERROR("SEEKHEAD is missing");
        return SEGMENT;
    }
    
    // go through each element(ID_SEEK)
    List<EBMLMasterElement::Entry>::const_iterator it = SEEKHEAD->children.cbegin();
    for (; it != SEEKHEAD->children.cend(); ++it) {
        sp<EBMLElement> element = (*it).element;
        if (element->id.u64 != ID_SEEK) continue;
        
        sp<EBMLIntegerElement> SEEKID = FindEBMLElement(element, ID_SEEKID);
        sp<EBMLIntegerElement> SEEKPOSITION = FindEBMLElement(element, ID_SEEKPOSITION);
        CHECK_FALSE(SEEKID.isNil());
        CHECK_FALSE(SEEKPOSITION.isNil());
        
        if (SEEKID->vint.u64 == ID_CLUSTER) {
            continue;
        }
        
        const Int64 pos = offset + SEEKPOSITION->vint.u64;
        if (pos <= *clusters) {
            // elements before clusters, skip reading it
            continue;
        }
        
        if (buffer->skipBytes(pos - buffer->offset()) != pos) {
            ERROR("seek failed");
            break;
        }
        
        DEBUG("SEEKID %#x @ %" PRIx64, SEEKID->vint.u64, SEEKPOSITION->vint.u64);
        sp<EBMLElement> ELEMENT = ReadEBMLElement(buffer);
        INFO("prefetch element %s", ELEMENT->name);
        if (ELEMENT.isNil() || ELEMENT->id.u64 != SEEKID->vint.u64) {
            ERROR("read element @ 0x%" PRIx64 " failed", SEEKPOSITION->vint.u64);
            break;
        }
        
        SEGMENT->children.push(EBMLMasterElement::Entry(pos, ELEMENT));
    }
    return SEGMENT;
}

#define TIMESCALE_DEF 1000000UL
Bool decodeMPEGAudioFrameHeader(const Buffer& frame, UInt32 *sampleRate, UInt32 *numChannels);
struct MatroskaFile : public MediaDevice {
    Int64                 mSegment;   // offset of SEGMENT
    Int64                 mClusters;  // offset of CLUSTERs
    MediaTime               mDuration;
    UInt64                mTimeScale;
    sp<ABuffer>             mContent;
    HashTable<UInt32, MatroskaTrack> mTracks;
    sp<EBMLMasterElement>   mCluster;
    List<sp<MediaFrame> >  mPackets;

    MatroskaFile() : MediaDevice(), mDuration(0), mTimeScale(TIMESCALE_DEF), mContent(Nil) { }

    MediaError init(const sp<ABuffer>& buffer) {
        // check ebml header
        sp<EBMLMasterElement> EBMLHEADER = ReadEBMLElement(buffer);
        if (EBMLHEADER == Nil) {
            ERROR("missing EBMLHEADER");
            return kMediaErrorBadFormat;
        }
        sp<EBMLStringElement> DOCTYPE = FindEBMLElement(EBMLHEADER, ID_DOCTYPE);
        if (DOCTYPE->str != "matroska") {
            ERROR("unknown DOCTYPE %s", DOCTYPE->str.c_str());
            return kMediaErrorNotSupported;
        }

        // check SEGMENT
        Int64 offset = buffer->offset();
        sp<EBMLMasterElement> SEGMENT = ReadSEGMENT(buffer, &mClusters);
        if (SEGMENT == Nil) {
            ERROR("missing SEGMENT");
            return kMediaErrorBadFormat;
        }
        // children inside reference to the segment element position excluding id & size
        mSegment = offset + SEGMENT->id.length + SEGMENT->size.length;

        // handle SEGMENTINFO
        sp<EBMLMasterElement> SEGMENTINFO = FindEBMLElement(SEGMENT, ID_SEGMENTINFO);
        if (SEGMENTINFO == Nil) {
            ERROR("missing SEGMENTINFO");
            return kMediaErrorBadFormat;
        }

        sp<EBMLIntegerElement> TIMECODESCALE = FindEBMLElement(SEGMENTINFO, ID_TIMECODESCALE);
        if (TIMECODESCALE != Nil) {
            DEBUG("timecodescale %" PRId64, TIMECODESCALE->vint.u64);
            mTimeScale = TIMECODESCALE->vint.u64;
        }
        if (!mTimeScale) mTimeScale = TIMESCALE_DEF;

        sp<EBMLFloatElement> DURATION = FindEBMLElement(SEGMENTINFO, ID_DURATION);
        if (DURATION != Nil) {
            mDuration = MediaTime( DURATION->flt * mTimeScale, 1000000000LL);
        }
        DEBUG("duration %.3f(s)", mDuration.seconds());

        // handle SEGMENT
        sp<EBMLMasterElement> TRACKS = FindEBMLElement(SEGMENT, ID_TRACKS);
        if (TRACKS == Nil) {
            ERROR("missing TRACKS");
            return kMediaErrorBadFormat;
        }
        
        PrintEBMLElements(TRACKS);

        Bool stage2 = False;
        List<EBMLMasterElement::Entry>::const_iterator it = TRACKS->children.cbegin();
        for (; it != TRACKS->children.cend(); ++it) {
            const EBMLMasterElement::Entry& e = *it;
            if (e.element->id.u64 != ID_TRACKENTRY) continue;

            MatroskaTrack trak;
            const sp<EBMLMasterElement> TRACKENTRY = e.element;

            sp<EBMLIntegerElement> TRACKNUMBER = FindEBMLElement(TRACKENTRY, ID_TRACKNUMBER);

            sp<EBMLStringElement> CODECID = FindEBMLElement(TRACKENTRY, ID_CODECID);
            sp<EBMLBinaryElement> CODECPRIVATE = FindEBMLElement(TRACKENTRY, ID_CODECPRIVATE);

            INFO("track codec %s", CODECID->str.c_str());
            if (CODECID->str == "V_MS/VFW/FOURCC") {
                Microsoft::BITMAPINFOHEADER biHead;
                if (biHead.parse(CODECPRIVATE->data->cloneBytes()) != kMediaNoError) {
                    ERROR("parse BITMAPINFOHEADER failed");
                    continue;
                }
                trak.format = kVideoCodecMicrosoftMPEG4;
            } else {
                trak.format = GetCodecFormat(CODECID->str);
            }
            
            if (trak.format == kAudioCodecUnknown) {
                ERROR("unknown codec %s", CODECID->str.c_str());
                continue;
            }

            sp<EBMLIntegerElement> DEFAULTDURATION = FindEBMLElement(TRACKENTRY, ID_DEFAULTDURATION);
            if (DEFAULTDURATION != Nil) {
                DEBUG("frame time %" PRIu64, DEFAULTDURATION->vint.u64);
                // DEFAULTDURATION is not scaled, scale here
                trak.frametime = DEFAULTDURATION->vint.u64 / mTimeScale;
            }

            sp<EBMLFloatElement> TRACKTIMECODESCALE = FindEBMLElement(TRACKENTRY, ID_TRACKTIMECODESCALE);
            if (TRACKTIMECODESCALE != Nil) {
                DEBUG("track timecodescale %f", TRACKTIMECODESCALE->flt);
                trak.timescale = TRACKTIMECODESCALE->flt;
                if (IS_ZERO(trak.timescale)) {
                    trak.timescale = 1.0f;
                }
            }
            
            if (CODECPRIVATE != Nil) {
                trak.csd = CODECPRIVATE->data;  // handle csd in format()
                DEBUG("track csd %s", trak.csd->string(True).c_str());
            }

            sp<EBMLIntegerElement> TRACKTYPE = FindEBMLElement(TRACKENTRY, ID_TRACKTYPE);
            if (TRACKTYPE->vint.u32 & kTrackTypeAudio) trak.type = kCodecTypeAudio;
            else if (TRACKTYPE->vint.u32 & kTrackTypeVideo) trak.type = kCodecTypeVideo;
            else if (TRACKTYPE->vint.u32 & kTrackTypeSubtitle) trak.type = kCodecTypeSubtitle;

            if (trak.type == kCodecTypeAudio) {
                sp<EBMLFloatElement> SAMPLINGFREQUENCY = FindEBMLElementInside(TRACKENTRY, ID_AUDIO, ID_SAMPLINGFREQUENCY);
                sp<EBMLIntegerElement> CHANNELS = FindEBMLElementInside(TRACKENTRY, ID_AUDIO, ID_CHANNELS);

                if (!SAMPLINGFREQUENCY.isNil())
                    trak.a.sampleRate = SAMPLINGFREQUENCY->flt;
                
                if (!CHANNELS.isNil())
                    trak.a.channels = CHANNELS->vint.u32;

                // I hate this: for some format, the mandatory properties always missing in header,
                // we have to decode blocks to get these properties
                if (trak.format == kAudioCodecAAC && trak.csd != Nil) {
                    // FIXME: strip audio properties from ADTS headers if csd is not exists
                    // AudioSpecificConfig
                    MPEG4::AudioSpecificConfig asc(trak.csd->cloneBytes());
                    if (asc.valid) {
                        trak.a.channels     = asc.channels;
                        trak.a.sampleRate   = asc.samplingFrequency;
                    } else
                        ERROR("bad AudioSpecificConfig");
                }

                if (trak.a.sampleRate == 0 || trak.a.channels == 0) {
                    WARN("%s: track miss mandatory properties", CODECID->str.c_str());
                    stage2 = True;
                }
            } else if (trak.type == kCodecTypeVideo) {
                // XXX: pixel width vs display width
                sp<EBMLIntegerElement> WIDTH = FindEBMLElementInside(TRACKENTRY, ID_VIDEO, ID_DISPLAYWIDTH);
                if (WIDTH == Nil) WIDTH = FindEBMLElementInside(TRACKENTRY, ID_VIDEO, ID_PIXELWIDTH);
                sp<EBMLIntegerElement> HEIGHT = FindEBMLElementInside(TRACKENTRY, ID_VIDEO, ID_DISPLAYHEIGHT);
                if (HEIGHT == Nil) HEIGHT = FindEBMLElementInside(TRACKENTRY, ID_VIDEO, ID_PIXELHEIGHT);

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
            if (CONTENTENCODING != Nil) {
                sp<EBMLMasterElement> CONTENTCOMPRESSION = FindEBMLElement(CONTENTENCODING, ID_CONTENTCOMPRESSION);
                if (CONTENTCOMPRESSION != Nil) {
                    sp<EBMLIntegerElement> CONTENTCOMPALGO = FindEBMLElement(CONTENTCOMPRESSION, ID_CONTENTCOMPALGO);
                    sp<EBMLBinaryElement> CONTENTCOMPSETTINGS = FindEBMLElement(CONTENTCOMPRESSION, ID_CONTENTCOMPSETTINGS);
                    
                    trak.compAlgo = CONTENTCOMPALGO->vint.u8;
                    if (trak.compAlgo == 3) {
                        CHECK_FALSE(CONTENTCOMPSETTINGS.isNil());
                        trak.compSettings = CONTENTCOMPSETTINGS->data;
                    }
                }
            }

            trak.index  = mTracks.size();
            mTracks.insert(TRACKNUMBER->vint.u32, trak);
        }

        // CUES
        sp<EBMLMasterElement> CUES = FindEBMLElement(SEGMENT, ID_CUES);
        if (!CUES.isNil()) {
            it = CUES->children.cbegin();
            for (; it != CUES->children.cend(); ++it) {     // CUES can be very large, use iterator
                const EBMLMasterElement::Entry& e = *it;
                if (e.element->id.u64 != ID_CUEPOINT) continue;
                
                sp<EBMLMasterElement> CUEPOINT = e.element;
                TOCEntry entry;
                
                List<EBMLMasterElement::Entry>::const_iterator it0 = CUEPOINT->children.cbegin();
                for (; it0 != CUEPOINT->children.cend(); ++it0) {
                    sp<EBMLIntegerElement> e = (*it0).element;
                    
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
        } else {
            ERROR("CUES is missing");
        }

        DEBUG("cluster start @ %" PRId64, mSegment + mClusters);
        buffer->skipBytes(mSegment + mClusters - buffer->offset());
        mContent    = buffer;

#if 0
        // stage 2: workarounds for some codec
        // get extra properties using packetizer
        if (stage2) {
            preparePackets();
            for (UInt32 i = 0; i < mTracks.size(); ++i) {
                MatroskaTrack& trak = mTracks[i];
                if (trak.packetizer == Nil) continue;

                sp<MediaFrame> packet = Nil;
                for (;;) {
                    List<sp<MediaFrame> >::const_iterator it = mPackets.cbegin();
                    for (; it != mPackets.cend(); ++it) {
                        if ((*it)->index == i) {
                            packet = *it;
                            break;
                        }
                    }

                    if (packet != Nil) break;

                    preparePackets();
                }

                trak.a.channels = packet->properties->findInt32(kKeyChannels);
                trak.a.sampleRate = packet->properties->findInt32(kKeySampleRate);
                INFO("real properties: %" PRIu32 " %" PRIu32, trak.a.channels, trak.a.sampleRate);
            }
        }
#endif
        
        return kMediaNoError;
    }

    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyFormat, kFileFormatMkv);
        info->setInt32(kKeyCount, mTracks.size());
        info->setInt64(kKeyDuration, mDuration.useconds());
        HashTable<UInt32, MatroskaTrack>::const_iterator it = mTracks.cbegin();
        for (; it != mTracks.cend(); ++it) {
            sp<Message> trakInfo = new Message;
            const MatroskaTrack& trak = it.value();
            trakInfo->setInt32(kKeyType, trak.type);
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
            if (trak.csd != Nil) {
                DEBUG("csd: %s", trak.csd->string(True).c_str());
                if (trak.format == kAudioCodecAAC) {
                    // AudioSpecificConfig -> ESDS
                    sp<Buffer> esds = MPEG4::MakeAudioESDS(trak.csd->cloneBytes());
                    if (esds.isNil()) {
                        MPEG4::AudioSpecificConfig asc (MPEG4::AOT_AAC_MAIN, trak.a.sampleRate, trak.a.channels);
                        esds = MPEG4::MakeAudioESDS(asc);
                    }
                    trakInfo->setObject(kKeyESDS, esds);
                } else if (trak.format == kVideoCodecH264) {
                    MPEG4::AVCDecoderConfigurationRecord avcC;
                    if (avcC.parse(trak.csd->cloneBytes()) == kMediaNoError) {
                        trakInfo->setObject(kKeyavcC, trak.csd);
                    } else {
                        ERROR("bad avcC");
                    }
                } else if (trak.format == kVideoCodecHEVC) {
                    trakInfo->setObject(kKeyhvcC, trak.csd);
                } else if (trak.format == kVideoCodecMPEG4) {
                    trakInfo->setObject(kKeyESDS, trak.csd);
                } else if (trak.format == kVideoCodecMicrosoftMPEG4) {
                    trakInfo->setObject(kKeyMicrosoftVCM, trak.csd);
                }
            }

            INFO("trak %zu: %s", trak.index, trakInfo->string().c_str());
            info->setObject(kKeyTrack + trak.index, trakInfo);
        }
        INFO("format %s", info->string().c_str());
        return info;
    }
    
    virtual MediaError configure(const sp<Message>& options) {
        if (options->contains(kKeySeek)) {
            seek(options->findInt64(kKeySeek));
            return kMediaNoError;
        }
        
        return kMediaErrorNotSupported;
    }
    
    void seek(Int64 us) {
        DEBUG("seek @ %.3fs", time.seconds());
        mCluster.clear();
        mPackets.clear();
        
        // seek with the first track who has toc
        HashTable<UInt32, MatroskaTrack>::const_iterator it = mTracks.cbegin();
        for (; it != mTracks.cend(); ++it) {
            const MatroskaTrack& trak = it.value();
            if (trak.toc.empty()) continue;
            
            //UInt64 timecode = MediaTime(time).rescale(1000000000LL / mTimeScale).value * trak.timescale;
            UInt64 timecode = (us * trak.timescale * 1000LL) / mTimeScale;
            
            List<TOCEntry>::const_iterator it0 = trak.toc.crbegin();
            for (; it0 != trak.toc.crend(); --it0) {
                const TOCEntry& entry = *it0;
                // find the entry with time < timecode
                if (entry.time <= timecode) {
                    DEBUG("seek hit @ %" PRIu64 ", cluster %" PRId64,
                          entry.time, entry.pos);
                    mContent->skipBytes(mSegment + entry.pos - mContent->offset());
                    break;
                }
            }
        }
    }

    MediaError preparePackets() {
        // prepare blocks
        if (mCluster == Nil) {
            mCluster = ReadEBMLElement(mContent);
            if (mCluster == Nil || mCluster->id.u64 != ID_CLUSTER) {
                INFO("no more cluster");
                return kMediaErrorBadContent;
            }

#if LOG_NDEBUG == 0
            PrintEBMLElements(mCluster);
#endif
        }

        sp<EBMLIntegerElement> TIMECODE = FindEBMLElement(mCluster, ID_TIMECODE);
        List<EBMLMasterElement::Entry>::iterator it = mCluster->children.begin();
        for (; it != mCluster->children.end();) {
            sp<EBMLElement> Element = (*it).element;
            if (Element->id.u64 != ID_BLOCKGROUP && Element->id.u64 != ID_SIMPLEBLOCK) {
                ++it;
                continue;
            }
            
            it = mCluster->children.erase(it);
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
            
            if (mTracks.find(block->TrackNumber.u32) == Nil) continue;
            MatroskaTrack& trak = mTracks[block->TrackNumber.u32];
            UInt64 timecode = TIMECODE->vint.u64 + block->TimeCode;
            List<sp<Buffer> >::iterator it0 = block->data.begin();
            for (; it0 != block->data.end(); ++it0) {

                sp<MediaFrame> packet = CreatePacket(trak,
                                                      *it0,
                                                      timecode,
                                                      mTimeScale,
                                                      type);

                if (trak.packetizer != Nil) {
                    if (trak.packetizer->push(packet) != kMediaNoError) {
                        DEBUG("[%zu] packetizer enqueue failed", packet->index);
                    }
                    packet = trak.packetizer->pull();
                }

                if (packet != Nil) {
                    packet->id = trak.index;    // fix trak index
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
    
    virtual MediaError push(const sp<MediaFrame>&) {
        return kMediaErrorInvalidOperation;
    }

    // https://matroska.org/technical/specs/notes.html#TimecodeScale
    // https://matroska.org/technical/specs/notes.html
    virtual sp<MediaFrame> pull() {
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

            sp<MediaFrame> packet = mPackets.front();
            mPackets.pop();

            DEBUG("pull %s", packet->string().c_str());
            return packet;
        }

        return Nil;
    }
    
    virtual MediaError reset() {
        return kMediaNoError;
    }
};

sp<MediaDevice> CreateMatroskaFile(const sp<ABuffer>& buffer) {
    sp<MatroskaFile> file = new MatroskaFile;
    if (file->init(buffer) == kMediaNoError) return file;
    return Nil;
}

__END_NAMESPACE_MFWK
