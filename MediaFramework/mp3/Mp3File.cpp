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


// File:    Mp3File.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "Mp3File"
//#define LOG_NDEBUG 0
#include "MediaDefs.h"

#define WITH_ID3
#ifdef WITH_ID3
#include "tags/id3/ID3.h"
#endif

#include <stdio.h> // FIXME: sscanf

#include "MediaPacketizer.h"
#include "MediaExtractor.h"


__BEGIN_NAMESPACE_MPX

using namespace ID3;
const static int kScanLength = 32 * 1024;


// MPEG Audio Frame Header:
// refer to http://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header
//
// AAAAAAAA AAABBCCD EEEEFFGH IIJJLMNN
// A    - <11>  sync bits
// B    - <2>   version bits
// C    - <2>   layer bits
// D    - <1>   protection bit
// E    - <4>   bitrate index
// F    - <2>   samplerate index
// G    - <1>   padding bit
// H    - <1>   private bit
// I    - <2>   channel mode
// J    - <2>   mode extension
// L    - <1>   copyright bit
// M    - <1>   original bit
// N    - <2>   emphasis

/*version 1 */
static uint32_t kBitrateTablev1[4][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* invalid */
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}, /* layer 3 */
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},  /*layer 2 */
    {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0}, /* layer 1*/
};

/*version 2 & 2.5 */
static uint32_t kBitrateTablev2[4][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,}, /* invalid */
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}, /* layer 3 */
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},  /* layer 2 */
    {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0}, /* layer 1 */
};

static int kSampleRateTable[4][4] = {
    {11050, 12000, 8000, 0}, /* version 2.5 */
    {0, 0, 0, 0},           /* invalid */
    {22050, 24000, 16000, 0}, /* version 2 */
    {44100, 48000, 32000, 0}, /* version 1 */
};

static int kSamplesPerFrameTable[4][4/*layer*/] = {
    {0, 576, 1152, 384}, /* version 2.5 */
    {0, 0, 0, 0},           /* invalid */
    {0, 576, 1152, 384}, /* version 2 */
    {0, 1152, 1152, 384}, /* version 1 */
};

// parse side infomation
// offset after frame header
static size_t kSideInfoOffset[][2] = {
    {17,    32},    /* version 1: mono, stereo */
    {9,     17}     /* version 2 & 2.5: mono, stereo */
};

enum {
    MPEG_VERSION_2_5,
    MPEG_VERSION_INVALID,
    MPEG_VERSION_2,
    MPEG_VERSION_1
};

enum {
    MPEG_LAYER_INVALID,
    MPEG_LAYER_III,
    MPEG_LAYER_II,
    MPEG_LAYER_I
};

enum {
    MPEG_CHANNEL_MODE_STEREO,
    MPEG_CHANNEL_MODE_JOINT_STEREO,
    MPEG_CHANNEL_MODE_DUAL,
    MPEG_CHANNEL_MODE_SINGLE
};

struct MPEGAudioFrameHeader {
    uint32_t    Version;
    uint32_t    Layer;
    uint32_t    bitRate;
    uint32_t    sampleRate;
    uint32_t    numChannels;
    uint32_t    samplesPerFrame;
    size_t      frameLengthInBytes;
};

// return frameLength in bytes
static ssize_t decodeFrameHeader(uint32_t head, MPEGAudioFrameHeader *frameHeader) {
    if ((head & 0xffe00000) != 0xffe00000) return -1;

    uint32_t version    = (head >> 19) & 0x3;
    uint32_t layer      = (head >> 17) & 0x3;
    uint32_t brIndex    = (head >> 12) & 0xf;
    uint32_t srIndex    = (head >> 10) & 0x3;
    uint32_t padding    = (head >> 9) & 0x1;
    uint32_t chnIndex   = (head >> 6) & 0x3;

    if (version == 0x1 || layer == 0x0 || brIndex == 0xf ||
            srIndex == 0x3) {
        DEBUG("invalid head.");
        return BAD_VALUE;
    }

    uint32_t samplesPerFrame    = kSamplesPerFrameTable[version][layer];
    uint32_t bitrate            = 1000 * (version == MPEG_VERSION_1 ?
            kBitrateTablev1[layer][brIndex] :
            kBitrateTablev2[layer][brIndex]);
    uint32_t sampleRate         = kSampleRateTable[version][srIndex];

    size_t  frameLengthInBytes  = (samplesPerFrame * bitrate) / (sampleRate * 8) + padding;

    if (layer == MPEG_LAYER_I)  frameLengthInBytes  *= 4;

    if (frameHeader) {
        frameHeader->Version    = version;
        frameHeader->Layer      = layer;
        frameHeader->bitRate    = bitrate;
        frameHeader->sampleRate = sampleRate;
        frameHeader->numChannels            = chnIndex == MPEG_CHANNEL_MODE_SINGLE ? 1 : 2;
        frameHeader->samplesPerFrame        = samplesPerFrame;
        frameHeader->frameLengthInBytes     = frameLengthInBytes;
    }

    return frameLengthInBytes;
}

// locate the first MPEG audio frame in data, and return frame length
// and offset in bytes. NO guarantee
const static uint32_t kHeaderMask = (0xffe00000 | (3 << 17) | (3 << 10) | (3 << 19) | (3 << 6));
static int64_t locateFirstFrame(const Buffer& data,
        struct MPEGAudioFrameHeader *frameHeader,
        size_t *possible/* possible head position */,
        uint32_t *_head/* found head */) {
    // locate first frame:
    //  current and next frame header is valid
    BitReader br(data.data(), data.size());
    uint32_t head = 0;
    for (size_t i = 0; i < br.length() / 8; i++) {
        head = (head << 8) | br.r8();

        // test current frame
        ssize_t frameLength = decodeFrameHeader(head, frameHeader);
        if (frameLength <= 4) continue;

        if (i + frameLength > br.length() / 8) {
            DEBUG("scan to the end.");
            if (possible) *possible = i - 3;
            break;
        }

        // test next head.
        BitReader _br(data.data(), data.size());
        _br.skipBytes(frameLength + i - 3);
        uint32_t next = _br.rb32();
        if ((next & kHeaderMask) == (head & kHeaderMask)
                && decodeFrameHeader(next, NULL) > 4) {
            if (_head) *_head = head;
            return i - 3;
        }
    }

    return NAME_NOT_FOUND;
}

static int64_t locateFrame(const Buffer& data, uint32_t common,
        struct MPEGAudioFrameHeader *frameHeader,
        size_t *possible) {
    BitReader br(data.data(), data.size());
    uint32_t head = 0;
    for (size_t i = 0; i < br.length() / 8; i++) {
        head = (head << 8) | br.r8();
        if ((head & kHeaderMask) != common) continue;

        ssize_t frameLength = decodeFrameHeader(head, frameHeader);
        if (frameLength <= 4) continue;
        if (i - 3 + frameLength > br.length() / 8) {
            if (possible) *possible = i - 3;
            break;
        }

        return i - 3;
    }
    return NAME_NOT_FOUND;
}

bool decodeMPEGAudioFrameHeader(const Buffer& frame, uint32_t *sampleRate, uint32_t *numChannels) {
    MPEGAudioFrameHeader mpa;
    ssize_t offset = locateFirstFrame(frame, &mpa, NULL, NULL);
    if (offset < 0) return false;
    if (sampleRate) *sampleRate = mpa.sampleRate;
    if (numChannels) *numChannels = mpa.numChannels;
    return true;
}

struct XingHeader {
    String          ID;
    uint32_t        numFrames;
    uint32_t        numBytes;
    List<uint8_t>   toc;
    uint32_t        quality;
    // LAME extension
    String          encoder;
    uint8_t         lpf;
    uint32_t        delays;
    uint32_t        paddings;
};

// http://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header#XINGHeader
// http://gabriel.mp3-tech.org/mp3infotag.html#versionstring
static bool parseXingHeader(const Buffer& firstFrame, XingHeader *head) {
    CHECK_NULL(head);
    BitReader br(firstFrame.data(), firstFrame.size());
    head->ID    = br.readS(4);  // XING or Info;
    if (head->ID != "XING" && head->ID != "Info") {
        return false;
    }

    const uint32_t flags = br.rb32();
    DEBUG("XING flags: %#x", flags);

    if (flags & 0x0001) {
        head->numFrames     = br.rb32();
        DEBUG("Xing: number frames %d", head->numFrames);
    } else {
        head->numFrames     = 0;
        DEBUG("Xing: no number frames.");
    }

    // including the first frame.
    if (flags & 0x0002) {
        head->numBytes      = br.rb32();
        DEBUG("Xing: number bytes %d", (int32_t)head->numBytes);
    } else {
        head->numBytes      = 0;
        DEBUG("Xing: no number bytes.");
    }

    if (flags & 0x0004) {
        for (int i = 0; i < 100; i++) {
            //mTOC.push((mNumBytes * pos) / 256 + mFirstFrameOffset);
            head->toc.push(br.r8());
        }
        DEBUG("TOC: %d %d ... %d %d", head->toc[0], head->toc[1], head->toc[98], head->toc[99]);
    } else {
        DEBUG("Xing: no toc");
    }

    if (flags & 0x0008) {
        head->quality   = br.rb32();
    }

    // LAME extension
    head->encoder       = br.readS(9);
    br.skipBytes(1);                    // Info Tag revision & VBR method

    head->lpf           = br.r8();      // low pass filter value.
    uint8_t lpf         = br.r8();     // low pass filter value. Hz = lpf * 100;
    DEBUG("lpf: %u Hz.", lpf * 100);

    br.skipBytes(8);               // replay gain
    br.skipBytes(1);               // encode flags & ATH Type
    br.skipBytes(1);               // specified or minimal bitrate

    // refer to ffmpeg/libavformat/mp3dec.c:mp3_parse_info_tag
    if (head->encoder.startsWith("LAME") ||
            head->encoder.startsWith("Lavf") ||
            head->encoder.startsWith("Lavc")) {
        head->delays    = br.read(12);
        head->paddings  = br.read(12);
    } else {
        head->delays    = 0;
        head->paddings  = 0;
    }

    // 12 bytes remains.

    return true;
}

struct VBRIHeader {
    String          ID;
    uint16_t        version;
    uint16_t        delay;
    uint16_t        quality;
    uint32_t        numBytes;
    uint32_t        numFrames;
    List<uint32_t>  toc;
};
// https://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header#VBRIHeader
// https://www.crifan.com/files/doc/docbook/mpeg_vbr/release/webhelp/vbri_header.html
static bool parseVBRIHeader(const Buffer& firstFrame, VBRIHeader *head) {
    CHECK_NULL(head);

    BitReader br(firstFrame.data(), firstFrame.size());
    head->ID                = br.readS(4);  // VBRI
    if (head->ID != "VBRI") return false;

    head->version           = br.rb16();
    head->delay             = br.rb16();
    head->quality           = br.rb16();

    head->numBytes          = br.rb32();    // total size
    head->numFrames         = br.rb32();    // total frames

    uint16_t numEntries     = br.rb16();
    uint16_t scaleFactor    = br.rb16();
    uint16_t entrySize      = br.rb16();
    uint16_t entryFrames    = br.rb16();

    DEBUG("numEntries %d, entrySize %d, scaleFactor %d, entryFrames %d",
            numEntries, entrySize, scaleFactor, entryFrames);

    if (numEntries > 0) {
        for (int i = 0; i < numEntries; i++) {
            int length = 0;
            switch (entrySize) {
                case 1:
                    length = br.r8();
                    break;
                case 2:
                    length = br.rb16();
                    break;
                case 3:
                    length = br.rb24();
                    break;
                default:
                    CHECK_EQ(entrySize, 4);
                    length = br.rb32();
                    break;
            }

            head->toc.push(length * scaleFactor);
        }
        // end

        DEBUG("TOC: %d %d ... %d %d",
                head->toc[0], head->toc[1],
                head->toc[numEntries-2],
                head->toc[numEntries-1]);
    }

    return true;
}

struct Mp3Packetizer : public MediaPacketizer {
    Buffer      mBuffer;
    uint32_t    mCommonHead;
    bool        mNeedMoreData;
    bool        mFlushing;
    MediaTime   mAnchorTime;
    MediaTime   mFrameTime;
    sp<Message> mProperties;

    Mp3Packetizer() : MediaPacketizer(), mBuffer(4096, kBufferTypeRing),
    mCommonHead(0), mNeedMoreData(true), mFlushing(false) { }

    virtual ~Mp3Packetizer() { }
    
    virtual String string() const { return "Mp3Packetizer"; }

    virtual MediaError enqueue(const sp<MediaPacket>& in) {
        if (in == NULL) {
            DEBUG("flushing");
            mFlushing = true;
            return kMediaNoError;
        }

        if (__builtin_expect(mCommonHead == 0, false)) {
            mAnchorTime     = in->pts;
            if (mAnchorTime == kTimeInvalid) {
                mAnchorTime = kTimeBegin;
            }
        }

        while (mBuffer.empty() < in->size) {
            if (mNeedMoreData) {
                CHECK_EQ(mBuffer.resize(mBuffer.capacity() * 2), OK);
                DEBUG("resize internal buffer => %zu", mBuffer.capacity());
            } else {
                return kMediaErrorResourceBusy;
            }
        }

        mBuffer.write((const char *)in->data, in->size);
        mNeedMoreData = false;
        return kMediaNoError;
    }

    virtual sp<MediaPacket> dequeue() {
        DEBUG("internal buffer ready bytes %zu", mBuffer.ready());

        if (mBuffer.ready() <= 4) {
            if (!mFlushing) mNeedMoreData = true;
            return NULL;
        }

        // only at the very beginning, find the common header
        if (__builtin_expect(mCommonHead == 0, false)) {
            DEBUG("find common header");

            MPEGAudioFrameHeader mpa;
            size_t possible = 0;
            uint32_t head = 0;
            ssize_t offset = locateFirstFrame(mBuffer, &mpa, &possible, &head);

            if (offset < 0) {
                size_t junk = possible ? possible : mBuffer.ready() - 3;
                DEBUG("missing head, skip %zu junk", offset);
                mBuffer.skip(junk);
                return NULL;
            } else if (offset > 0) {
                DEBUG("skip %zu junk", offset);
                mBuffer.skip(offset);
            }
            mCommonHead = head & kHeaderMask;
            DEBUG("common header %#x", mCommonHead);

            mFrameTime = MediaTime(mpa.samplesPerFrame, mpa.sampleRate);
            mProperties = new Message;
            mProperties->setInt32(kKeyChannels, mpa.numChannels);
            mProperties->setInt32(kKeySampleRate, mpa.sampleRate);
        }

        size_t possible = 0;
        MPEGAudioFrameHeader mpa;
        ssize_t offset = locateFrame(mBuffer, mCommonHead, &mpa, &possible);
        if (offset < 0) {
            if (mFlushing) {
                DEBUG("%zu drop tailing bytes %s",
                        mpa.frameLengthInBytes,
                        mBuffer.string().c_str());
            } else {
                size_t junk = possible ? possible : mBuffer.ready() - 3;
                DEBUG("skip junk bytes %zu", junk);
                mNeedMoreData = true;
                mBuffer.skip(junk);
            }
            return NULL;
        } else if (offset > 0) {
            DEBUG("skip junk bytes %zu before frame", offset);
            mBuffer.skip(offset);
        }

        DEBUG("current frame length %zu (%zu)",
                mpa.frameLengthInBytes, mBuffer.ready());

        sp<MediaPacket> packet = MediaPacketCreate(mpa.frameLengthInBytes);
        mBuffer.read((char*)packet->data, mpa.frameLengthInBytes);
        CHECK_EQ(mpa.frameLengthInBytes, packet->size);

        mAnchorTime         += mFrameTime;
        packet->pts         = mAnchorTime;
        packet->dts         = mAnchorTime;
        packet->flags       = kFrameFlagSync;
        packet->format      = kAudioCodecFormatMP3;
        packet->properties  = mProperties;

        return packet;
    }
    
    virtual void flush() {
        mBuffer.reset();
        mCommonHead = 0;
        mNeedMoreData = true;
    }
};

struct Mp3File : public MediaExtractor {
    sp<Content>             mContent;
    int64_t                 mFirstFrameOffset;
    MPEGAudioFrameHeader    mHeader;

    bool                    mVBR;

    int32_t                 mNumFrames;
    int64_t                 mNumBytes;
    MediaTime               mDuration;

    Vector<int64_t>         mTOC;

    MediaTime               mAnchorTime;

    sp<MediaPacket>         mRawPacket;
    sp<MediaPacketizer>     mPacketizer;

    Mp3File() :
        mContent(NULL),
        mFirstFrameOffset(0),
        mVBR(false),
        mNumFrames(0),
        mNumBytes(0),
        mDuration(kTimeInvalid),
        mAnchorTime(kTimeBegin),
        mRawPacket(NULL),
        mPacketizer(new Mp3Packetizer) { }
    
    virtual String string() const { return "Mp3File"; }

    // refer to:
    // 1. http://gabriel.mp3-tech.org/mp3infotag.html#versionstring
    // 2. http://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header
    // 3. http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
    virtual MediaError init(sp<Content>& pipe, const sp<Message>& options) {
        CHECK_TRUE(pipe != 0);

        sp<Message> outputFormat    = new Message;
        sp<Message> ast             = new Message;

        pipe->seek(0);
        sp<Buffer> head     = pipe->read(ID3v2::kHeaderLength);
        if (head != NULL && head->size() == ID3v2::kHeaderLength) {
            ssize_t length  = ID3v2::isID3v2(*head);
            if (length > 0) {
                sp<Buffer> data = pipe->read(length);
                if (data != NULL) {
                    head->resize(ID3v2::kHeaderLength + length);
                    head->write(*data);
                    ID3v2 id3;
                    if (id3.parse(*head) == OK) {
                        const sp<Message>& values = id3.values();
                        DEBUG("id3v2 found: %s", values.string().c_str());
#if 1
                        // information for gapless playback
                        // http://yabb.jriver.com/interact/index.php?topic=65076.msg436101#msg436101
                        const char *iTunSMPB = values->findString("iTunSMPB");
                        if (iTunSMPB != 0) {
                            int32_t encodeDelay, encodePadding;
                            int32_t originalSampleCount;
                            if (sscanf(iTunSMPB, " %*x %x %x %x %*x",
                                        &encodeDelay, &encodePadding, &originalSampleCount) == 3) {
                                INFO("iTunSMPB: mEncodePadding %d mEncodePadding %d originalSampleCount %d",
                                        encodeDelay, encodePadding, originalSampleCount);
                                ast->setInt32("encoder-delay", encodeDelay);
                                ast->setInt32("encoder-padding", encodePadding);
                            }
                        }
#endif
                        outputFormat->setObject(Media::ID3v2, values);
                        mFirstFrameOffset = pipe->tell();
                    }
                }
            }
        }

        size_t totalLength = pipe->length();
        DEBUG("totalLength %zu", totalLength);
        pipe->seek(totalLength - ID3v1::kLength);
        sp<Buffer> tail = pipe->read(ID3v1::kLength);
        if (tail != NULL && tail->size() == ID3v1::kLength) {
            ID3v1 id3;
            if (id3.parse(*tail) == OK) {
                outputFormat->setObject(Media::ID3v1, id3.values());
                totalLength -= ID3v1::kLength;
            }
        }

        pipe->seek(mFirstFrameOffset);

        sp<Buffer> scanData = pipe->read(kScanLength);
        if (scanData == 0) {
            ERROR("file is corupt?");
            return kMediaErrorBadFormat;
        }

        // skip junk before first frame.
        ssize_t result = locateFirstFrame(*scanData, &mHeader, NULL, NULL);
        if (result < 0) {
            ERROR("failed to locate the first frame.");
            return kMediaErrorBadFormat;
        } else if (result > 0) {
            DEBUG("%zu bytes junk data before first frame", (size_t)result);
        }
        mFirstFrameOffset   += result;
        DEBUG("mFirstFrameOffset = %" PRId64, mFirstFrameOffset);

        pipe->seek(mFirstFrameOffset);
        sp<Buffer> firstFrame = pipe->read(mHeader.frameLengthInBytes);
        if (firstFrame->size() < mHeader.frameLengthInBytes) {
            ERROR("content is too small.");
            return kMediaErrorBadFormat;
        }
        DEBUG("first frame size %zu", mHeader.frameLengthInBytes);

        // decode first frame
        const size_t offset = 4 + kSideInfoOffset[mHeader.Version == MPEG_VERSION_1 ? 0 : 1]
            [mHeader.numChannels - 1];
        firstFrame->skip(offset);

        DEBUG("side info: %s", firstFrame->string().c_str());

        mNumBytes = totalLength - mFirstFrameOffset;    // default value
        XingHeader xing;
        VBRIHeader vbri;
        if (parseXingHeader(*firstFrame, &xing)) {
            INFO("Xing header present");
            mVBR        = xing.ID == "Xing";
            mNumBytes   = xing.numBytes;
            mNumFrames  = xing.numFrames;
            List<uint8_t>::iterator it = xing.toc.begin();
            for (; it != xing.toc.end(); ++it) {
                mTOC.push((xing.numBytes * (*it)) / 256 + mFirstFrameOffset);
            }
        } else if (parseVBRIHeader(*firstFrame, &vbri)) {
            INFO("VBRI header present");
            mVBR        = true;
            mNumBytes   = vbri.numBytes;
            mNumFrames  = vbri.numFrames;
            int64_t offset = mFirstFrameOffset;
            mTOC.push(offset);
            List<uint32_t>::iterator it = vbri.toc.begin();
            for (; it != vbri.toc.end(); ++it) {
                offset += (*it);
                mTOC.push(offset);
            }
        }

#if 1
        // hack for truncated/incomplete files.
        if (mNumBytes > totalLength - mFirstFrameOffset) {
            WARN("Fix number of bytes, expected %" PRId64 " bytes, but we got max bytes %" PRId64,
                    mNumBytes, totalLength - mFirstFrameOffset);
            mNumBytes   = totalLength - mFirstFrameOffset;
            mNumFrames  = 0;
        }
#endif

        DEBUG("number bytes of data %" PRId64 " pipe length %" PRId64,
                mNumBytes, pipe->size());

        if (mNumFrames != 0) {
            DEBUG("calc duration based on frame count.");
            mDuration   = MediaTime(mNumFrames * mHeader.samplesPerFrame, mHeader.sampleRate);
            if (mVBR) {
                mHeader.bitRate     = 8 * mNumBytes / mDuration.seconds();
            }
        } else {
            DEBUG("calc duration based on bitrate.");
            mDuration   = MediaTime((8 * 1E6 * mNumBytes) / mHeader.bitRate, 1000000LL).scale(mHeader.sampleRate);
        }
        DEBUG("mBitRate %d, mDuration %.3f(s)", mHeader.bitRate, mDuration.seconds());


        if (mTOC.size()) {
            // add last entry to TOC
            mTOC.push(mFirstFrameOffset + mNumBytes);
            pipe->seek(mTOC[0]);
        } else {
            pipe->seek(mFirstFrameOffset);
        }

        mContent = pipe;

        DEBUG("firstFrameOffset %" PRId64, mFirstFrameOffset);
        return kMediaNoError;
    }

    virtual ~Mp3File() { }

    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyFormat, kFileFormatMP3);
        info->setInt64(kKeyDuration, mDuration.useconds());

        sp<Message> trak = new Message;
        trak->setInt32(kKeyFormat, kAudioCodecFormatMP3);
        // FIXME:
        //ast->setInt32(Media::Bitrate, mBitRate);
        trak->setInt32(kKeyChannels, mHeader.numChannels);
        trak->setInt32(kKeySampleRate, mHeader.sampleRate);

        info->setObject("track-0", trak);
        return info;
    }

    virtual sp<MediaPacket> read(size_t index,
            eModeReadType mode,
            const MediaTime& ts = kTimeInvalid) {
        CHECK_EQ(index, 0);
        CHECK_NE(mode, kModeReadPeek); // don't support this
        bool sawInputEOS = false;

        if (ts != kTimeInvalid) {
            int64_t pos = 0;
            double percent   = ts.seconds() / mDuration.seconds();
            if (percent < 0) percent = 0;
            else if (percent > 1) percent = 1;

            if (mTOC.size()) {
                float a = percent * (mTOC.size() - 1);
                int index = (int)a;

                int64_t fa  = mTOC[index];
                int64_t fb  = mTOC[index + 1];

                pos = fa + (fb - fa) * (a - index);
            } else {
                if (mVBR) {
                    WARN("seek vbr without toc");
                }

                pos = mNumBytes * percent + mFirstFrameOffset;
            }

            mContent->seek(pos);
            // TODO: calc anchor time by index.
            mAnchorTime     = ts;

            mPacketizer->flush();
        }

        sp<MediaPacket> packet;
        for (;;) {
            if (mRawPacket == 0 && !sawInputEOS) {
                DEBUG("read content at %" PRId64 "/%" PRId64,
                        mContent->tell(),
                        mContent->size());
                // mInternalBuffer must be twice of this
                sp<Buffer> data = mContent->read(2048);
                if (data == NULL) {
                    DEBUG("saw content eos..");
                    sawInputEOS = true;
                } else {
                    mRawPacket = MediaPacketCreate(data);
                }
            }

            if (sawInputEOS) {
                DEBUG("flushing...");
                mPacketizer->enqueue(NULL);
            } else if (mPacketizer->enqueue(mRawPacket) == kMediaNoError) {
                DEBUG("enqueue buffer done");
                mRawPacket.clear();
            }

            packet = mPacketizer->dequeue();
            if (packet == NULL) {
                if (sawInputEOS) {
                    break;
                }
                continue;
            } else {
                break;
            }
        }

        if (packet == NULL) {
            INFO("eos...");
            return NULL;
        }

        packet->pts += mAnchorTime;
        packet->dts += mAnchorTime;

        return packet;
    }
};

sp<MediaExtractor> CreateMp3File() {
    return new Mp3File;
}

sp<MediaPacketizer> CreateMp3Packetizer() {
    return new Mp3Packetizer;
}

ssize_t locateFirstFrame(const Buffer& data, size_t *frameLength) {
    MPEGAudioFrameHeader mpa;
    ssize_t offset = locateFirstFrame(data, &mpa, NULL, NULL);

    if (offset < 0) return NAME_NOT_FOUND;

    *frameLength    = mpa.frameLengthInBytes;
    return offset;
}

ssize_t decodeMPEG4AudioHeader(uint32_t head, uint32_t * sampleRate, uint32_t * numChannels) {
    MPEGAudioFrameHeader frameHeader;
    ssize_t frameLengthInBytes = decodeFrameHeader(head, &frameHeader);
    if (sampleRate)     *sampleRate = frameHeader.sampleRate;
    if (numChannels)    *numChannels = frameHeader.numChannels;
    return frameLengthInBytes;
}

__END_NAMESPACE_MPX

