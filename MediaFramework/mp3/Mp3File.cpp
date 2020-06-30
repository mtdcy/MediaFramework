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
#include "MediaTypes.h"
#include "id3/ID3.h"

#include <stdio.h> // FIXME: sscanf

#include "MediaDevice.h"

__BEGIN_NAMESPACE_MPX

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

// same version/layer/sampleRate/channels
const static uint32_t kHeaderMask = (0xffe00000 | (3 << 17) | (3 << 10) | (3 << 19) | (3 << 6));
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
        return kMediaErrorBadValue;
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
static int64_t locateFirstFrame(const sp<ABuffer>& buffer,
        struct MPEGAudioFrameHeader *frameHeader,
        size_t *possible/* possible head position */,
        uint32_t *_head/* found head */) {
    // locate first frame:
    //  current and next frame header is valid
    uint32_t head = 0;
    while (buffer->size()) {
        head = (head << 8) | buffer->r8();
        const int64_t offset = buffer->offset() - 4;

        // test current frame
        ssize_t frameLength = decodeFrameHeader(head, frameHeader);
        if (frameLength <= 4) continue;

        if (frameLength > buffer->size()) {
            DEBUG("scan to the end.");
            if (possible) *possible = buffer->offset();
            break;
        }

        // test next head.
        buffer->skipBytes(frameLength - 4);
        uint32_t next = buffer->rb32();
        DEBUG("current head %#x @ %" PRId64 ", next head %#x @%" PRId64,
              head, offset, next, offset + frameLength);
        if ((next & kHeaderMask) == (head & kHeaderMask)
                && decodeFrameHeader(next, NULL) > 4) {
            if (_head) *_head = head;
            DEBUG("locate first frame with head %#x @ %" PRId64 ", with next head %#x @%" PRId64,
                  head, offset, next, offset + frameLength);
            // put buffer @ first frame start position
            buffer->skipBytes(-(buffer->offset() - offset));
            return offset;
        }
        buffer->skipBytes(-frameLength);
    }

    return -1;
}

static bool locateFrame(const sp<ABuffer>& buffer, uint32_t common,
        struct MPEGAudioFrameHeader *frameHeader,
        size_t *possible) {
    uint32_t head = 0;
    while (buffer->size()) {
        head = (head << 8) | buffer->r8();
        if ((head & kHeaderMask) != common) continue;

        ssize_t frameLength = decodeFrameHeader(head, frameHeader);
        if (frameLength <= 4) continue;
        if (frameLength - 4 > buffer->size()) {
            if (possible) *possible = buffer->offset() - 4;
            break;
        }

        return true;
    }
    
    return false;
}

bool decodeMPEGAudioFrameHeader(const sp<ABuffer>& frame, uint32_t *sampleRate, uint32_t *numChannels) {
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
static bool parseXingHeader(const sp<ABuffer>& firstFrame, XingHeader *head) {
    CHECK_NULL(head);
    head->ID    = firstFrame->rs(4);  // XING or Info;
    if (head->ID != "XING" && head->ID != "Info") {
        return false;
    }

    const uint32_t flags = firstFrame->rb32();
    DEBUG("XING flags: %#x", flags);

    if (flags & 0x0001) {
        head->numFrames     = firstFrame->rb32();
        DEBUG("Xing: number frames %d", head->numFrames);
    } else {
        head->numFrames     = 0;
        DEBUG("Xing: no number frames.");
    }

    // including the first frame.
    if (flags & 0x0002) {
        head->numBytes      = firstFrame->rb32();
        DEBUG("Xing: number bytes %d", (int32_t)head->numBytes);
    } else {
        head->numBytes      = 0;
        DEBUG("Xing: no number bytes.");
    }

    if (flags & 0x0004) {
        for (int i = 0; i < 100; i++) {
            //mTOC.push((mNumBytes * pos) / 256 + mFirstFrameOffset);
            head->toc.push(firstFrame->r8());
        }
        //DEBUG("TOC: %d %d ... %d %d", head->toc[0], head->toc[1], head->toc[98], head->toc[99]);
#if LOG_NDEBUG == 0
        String tmp;
        List<uint8_t>::iterator it = head->toc.begin();
        for (; it != head->toc.end(); ++it) {
            tmp += String::format("%u ", *it);
        }
        DEBUG("TOC: %s", tmp.c_str());
#endif
    } else {
        DEBUG("Xing: no toc");
    }

    if (flags & 0x0008) {
        head->quality   = firstFrame->rb32();
    }

    // LAME extension
    head->encoder       = firstFrame->rs(9);
    firstFrame->skipBytes(1);                    // Info Tag revision & VBR method

    head->lpf           = firstFrame->r8();      // low pass filter value.
    uint8_t lpf         = firstFrame->r8();     // low pass filter value. Hz = lpf * 100;
    DEBUG("lpf: %u Hz.", lpf * 100);

    firstFrame->skipBytes(8);               // replay gain
    firstFrame->skipBytes(1);               // encode flags & ATH Type
    firstFrame->skipBytes(1);               // specified or minimal bitrate

    // refer to ffmpeg/libavformat/mp3dec.c:mp3_parse_info_tag
    if (head->encoder.startsWith("LAME") ||
            head->encoder.startsWith("Lavf") ||
            head->encoder.startsWith("Lavc")) {
        head->delays    = firstFrame->read(12);
        head->paddings  = firstFrame->read(12);
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
static bool parseVBRIHeader(const sp<ABuffer>& firstFrame, VBRIHeader *head) {
    CHECK_NULL(head);

    head->ID                = firstFrame->rs(4);  // VBRI
    if (head->ID != "VBRI") return false;

    head->version           = firstFrame->rb16();
    head->delay             = firstFrame->rb16();
    head->quality           = firstFrame->rb16();

    head->numBytes          = firstFrame->rb32();    // total size
    head->numFrames         = firstFrame->rb32();    // total frames

    uint16_t numEntries     = firstFrame->rb16();
    uint16_t scaleFactor    = firstFrame->rb16();
    uint16_t entrySize      = firstFrame->rb16();
    uint16_t entryFrames    = firstFrame->rb16();

    DEBUG("numEntries %d, entrySize %d, scaleFactor %d, entryFrames %d",
            numEntries, entrySize, scaleFactor, entryFrames);

    if (numEntries > 0) {
        for (int i = 0; i < numEntries; i++) {
            int length = 0;
            switch (entrySize) {
                case 1:
                    length = firstFrame->r8();
                    break;
                case 2:
                    length = firstFrame->rb16();
                    break;
                case 3:
                    length = firstFrame->rb24();
                    break;
                default:
                    CHECK_EQ(entrySize, 4);
                    length = firstFrame->rb32();
                    break;
            }

            head->toc.push(length * scaleFactor);
        }
        // end
#if 0
        DEBUG("TOC: %d %d ... %d %d",
                head->toc[0], head->toc[1],
                head->toc[numEntries-2],
                head->toc[numEntries-1]);
#endif
#if LOG_NDEBUG == 0
        String tmp;
        List<uint32_t>::iterator it = head->toc.begin();
        for (; it != head->toc.end(); ++it) {
            tmp += String::format("%u ", *it);
        }
        DEBUG("TOC: %s", tmp.c_str());
#endif
    }

    return true;
}

struct Mp3Packetizer : public MediaDevice {
    sp<Buffer>  mBuffer;
    uint32_t    mCommonHead;
    bool        mNeedMoreData;
    bool        mFlushing;
    MediaTime   mNextFrameTime;
    MediaTime   mFrameTime;
    sp<Message> mProperties;

    Mp3Packetizer() : MediaDevice(), mBuffer(new Buffer(4096, Buffer::Ring)),
    mCommonHead(0), mNeedMoreData(true), mFlushing(false),
    mNextFrameTime(kMediaTimeInvalid), mFrameTime(kMediaTimeInvalid) { }

    virtual ~Mp3Packetizer() { }
    
    virtual sp<Message> formats() const {
        // TODO: formats is available after the first frame
        return new Message;
    }
    
    virtual MediaError configure(const sp<Message>&) {
        return kMediaErrorNotSupported;
    }
    
    virtual MediaError push(const sp<MediaFrame>& in) {
        if (in == NULL) {
            DEBUG("flushing");
            mFlushing = true;
            return kMediaNoError;
        }

        if (__builtin_expect(mCommonHead == 0, false)) {
            mNextFrameTime     = in->timecode;
            if (mNextFrameTime == kMediaTimeInvalid) {
                mNextFrameTime = 0;
            }
        }

        while (mBuffer->empty() < in->planes.buffers[0].size) {
            if (mNeedMoreData) {
                CHECK_TRUE(mBuffer->resize(mBuffer->capacity() * 2));
                DEBUG("resize internal buffer => %zu", mBuffer->capacity());
            } else {
                return kMediaErrorResourceBusy;
            }
        }

        mBuffer->writeBytes((const char *)in->planes.buffers[0].data, in->planes.buffers[0].size);
        mNeedMoreData = false;
        return kMediaNoError;
    }

    virtual sp<MediaFrame> pull() {
        DEBUG("internal buffer ready bytes %zu", mBuffer->size());

        if (mBuffer->size() <= 4) {
            if (!mFlushing) mNeedMoreData = true;
            return NULL;
        }

        // only at the very beginning, find the common header
        if (ABE_UNLIKELY(mCommonHead == 0)) {
            DEBUG("find common header");

            MPEGAudioFrameHeader mpa;
            size_t possible = 0;
            uint32_t head = 0;
            ssize_t offset = locateFirstFrame(mBuffer, &mpa, &possible, &head);

            if (offset < 0) {
                size_t junk = possible ? possible : mBuffer->size() - 3;
                DEBUG("missing head, skip %zu junk", offset);
                mBuffer->skipBytes(junk);
                return NULL;
            } else if (offset > 0) {
                DEBUG("skip %zu junk", offset);
                mBuffer->skipBytes(offset);
            }
            mCommonHead = head & kHeaderMask;
            DEBUG("common header %#x", mCommonHead);

            mFrameTime = MediaTime(mpa.samplesPerFrame, mpa.sampleRate);
            mProperties = new Message;
            mProperties->setInt32(kKeyChannels, mpa.numChannels);
            mProperties->setInt32(kKeySampleRate, mpa.sampleRate);
        }
        
        sp<Buffer> frame;
        uint32_t head = 0;
        MPEGAudioFrameHeader mpa;
        while (mBuffer->size()) {
            head = (head << 8) | mBuffer->r8();
            if ((head & kHeaderMask) != mCommonHead) continue;
            
            ssize_t frameLength = decodeFrameHeader(head, &mpa);
            if (frameLength <= 4) continue;
            if (frameLength - 4 > mBuffer->size()) continue;
            
            mBuffer->skipBytes(-4);
            frame = mBuffer->readBytes(frameLength);
            break;
        }
        
        if (frame.isNIL()) return NULL;
        
        sp<MediaFrame> packet = MediaFrame::Create(frame);

        CHECK_TRUE(mFrameTime != kMediaTimeInvalid);
        packet->planes.buffers[0].size  = mpa.frameLengthInBytes;
        packet->timecode                = mNextFrameTime;
        packet->duration                = mFrameTime;
        packet->flags                   = kFrameTypeSync;
        mNextFrameTime                  += mFrameTime;

        return packet;
    }
    
    virtual MediaError reset() {
        mBuffer->clearBytes();
        mNextFrameTime = kMediaTimeInvalid;
        mCommonHead = 0;
        mNeedMoreData = true;
        return kMediaNoError;
    }
};

struct Mp3File : public MediaDevice {
    sp<ABuffer>             mContent;
    int64_t                 mFirstFrameOffset;
    MPEGAudioFrameHeader    mHeader;

    bool                    mVBR;

    int32_t                 mNumFrames;
    int64_t                 mNumBytes;
    MediaTime               mDuration;

    Vector<int64_t>         mTOC;

    MediaTime               mAnchorTime;

    sp<MediaFrame>          mRawPacket;
    sp<MediaDevice>         mPacketizer;
    
    sp<Message>             mID3v1;
    sp<Message>             mID3v2;

    Mp3File() :
        mContent(NULL),
        mFirstFrameOffset(0),
        mVBR(false),
        mNumFrames(0),
        mNumBytes(0),
        mDuration(kMediaTimeInvalid),
        mAnchorTime(0),
        mRawPacket(NULL),
        mPacketizer(new Mp3Packetizer) { }
    
    // refer to:
    // 1. http://gabriel.mp3-tech.org/mp3infotag.html#versionstring
    // 2. http://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header
    // 3. http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
    virtual MediaError init(const sp<ABuffer>& buffer) {
        CHECK_TRUE(buffer != 0);

        sp<Message> outputFormat    = new Message;
        sp<Message> ast             = new Message;

        mID3v2 = ID3::ReadID3v2(buffer);
#if LOG_NDEBUG == 0
        if (mID3v2 != NULL) DEBUG("ID3v2: %s", mID3v2->string().c_str());
#endif
        if (!mID3v2.isNIL()) {
#if 0
            // information for gapless playback
            // http://yabb.jriver.com/interact/index.php?topic=65076.msg436101#msg436101
            const char *iTunSMPB = id3v2->findString(kKeyiTunSMPB);
            if (iTunSMPB != 0) {
                int32_t encodeDelay, encodePadding;
                int32_t originalSampleCount;
                if (sscanf(iTunSMPB, " %*x %x %x %x %*x",
                            &encodeDelay, &encodePadding, &originalSampleCount) == 3) {
                    INFO("iTunSMPB: mEncodePadding %d mEncodePadding %d originalSampleCount %d",
                            encodeDelay, encodePadding, originalSampleCount);
                    ast->setInt32(kKeyEncoderDelay, encodeDelay);
                    ast->setInt32(kKeyEncoderPadding, encodePadding);
                }
            }
#endif
        }
        mFirstFrameOffset = buffer->offset();

        sp<Buffer> scanData = buffer->readBytes(kScanLength);
        if (scanData == 0) {
            ERROR("file is corupt?");
            return kMediaErrorBadFormat;
        }

        // skip junk before first frame.
        ssize_t result = locateFirstFrame(scanData, &mHeader, NULL, NULL);
        if (result < 0) {
            ERROR("failed to locate the first frame.");
            return kMediaErrorBadFormat;
        } else if (result > 0) {
            DEBUG("%zu bytes junk data before first frame", (size_t)result);
        }
        buffer->skipBytes(-(buffer->offset() - result));
        mFirstFrameOffset += result;
        DEBUG("mFirstFrameOffset = %" PRId64, mFirstFrameOffset);

        sp<Buffer> firstFrame = buffer->readBytes(mHeader.frameLengthInBytes);
        if (firstFrame->size() < mHeader.frameLengthInBytes) {
            ERROR("content is too small.");
            return kMediaErrorBadFormat;
        }
        DEBUG("first frame size %zu", mHeader.frameLengthInBytes);
        
        mID3v1 = ID3::ReadID3v1(buffer);
        int64_t totalLength = buffer->size() - (mID3v1.isNIL() ? 0 : ID3V1_LENGTH);

        // decode first frame
        const size_t offset = 4 + kSideInfoOffset[mHeader.Version == MPEG_VERSION_1 ? 0 : 1]
            [mHeader.numChannels - 1];
        firstFrame->skipBytes(offset);

        DEBUG("side info: %s", firstFrame->string().c_str());

        mNumBytes = totalLength - mFirstFrameOffset;    // default value
        XingHeader xing;
        VBRIHeader vbri;
        if (parseXingHeader(firstFrame, &xing)) {
            INFO("Xing header present");
            mVBR        = xing.ID == "Xing";
            mNumBytes   = xing.numBytes;
            mNumFrames  = xing.numFrames;
            List<uint8_t>::iterator it = xing.toc.begin();
            for (; it != xing.toc.end(); ++it) {
                mTOC.push((xing.numBytes * (*it)) / 256 + mFirstFrameOffset);
            }
        } else if (parseVBRIHeader(firstFrame, &vbri)) {
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
                mNumBytes, buffer->size());

        if (mNumFrames != 0) {
            DEBUG("calc duration based on frame count.");
            mDuration   = MediaTime(mNumFrames * mHeader.samplesPerFrame, mHeader.sampleRate);
            if (mVBR) {
                mHeader.bitRate     = 8 * mNumBytes / mDuration.seconds();
            }
        } else {
            DEBUG("calc duration based on bitrate.");
            mDuration   = MediaTime((8 * 1E6 * mNumBytes) / mHeader.bitRate, 1000000LL).rescale(mHeader.sampleRate);
        }
        DEBUG("mBitRate %d, mDuration %.3f(s)", mHeader.bitRate, mDuration.seconds());

        if (mTOC.size()) {
            // add last entry to TOC
            mTOC.push(mFirstFrameOffset + mNumBytes);
            mFirstFrameOffset = mTOC[0];
            buffer->skipBytes(-(buffer->offset() - mTOC[0]));
        } else {
            buffer->skipBytes(-(buffer->offset() - mFirstFrameOffset));
        }

        mContent = buffer;

        DEBUG("firstFrameOffset %" PRId64, mFirstFrameOffset);
        return kMediaNoError;
    }

    virtual ~Mp3File() { }

    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyFormat, kFileFormatMp3);
        info->setInt64(kKeyDuration, mDuration.useconds());

        sp<Message> trak = new Message;
        trak->setInt32(kKeyType, kCodecTypeAudio);
        trak->setInt32(kKeyFormat, kAudioCodecMP3);
        // FIXME:
        //ast->setInt32(Media::Bitrate, mBitRate);
        trak->setInt32(kKeyChannels, mHeader.numChannels);
        trak->setInt32(kKeySampleRate, mHeader.sampleRate);

        info->setObject(kKeyTrack, trak);
        return info;
    }
    
    virtual MediaError configure(const sp<Message>& options) {
        if (options->contains(kKeySeek)) {
            seek(options->findInt64(kKeySeek));
            return kMediaNoError;
        }
        return kMediaErrorNotSupported;
    }
    
    void seek(int64_t us) {
        int64_t pos = 0;
        double percent   = us / mDuration.useconds();
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

        DEBUG("seek to %" PRId64 " of %" PRId64, pos, mContent->size());
        mContent->skipBytes(-(pos - mContent->offset()));
        // TODO: calc anchor time by index.
        mAnchorTime = us;
        mAnchorTime.rescale(mHeader.sampleRate);
        
        mPacketizer->reset();
    }
    
    virtual MediaError push(const sp<MediaFrame>&) {
        return kMediaErrorInvalidOperation;
    }

    virtual sp<MediaFrame> pull() {
        bool sawInputEOS = false;
        sp<MediaFrame> packet;
        for (;;) {
            if (mRawPacket == 0 && !sawInputEOS) {
                DEBUG("read content at %" PRId64 "/%" PRId64,
                        mContent->offset(),
                        mContent->size());
                // mInternalBuffer must be twice of this
                sp<Buffer> data = mContent->readBytes(2048);
                if (data == NULL) {
                    DEBUG("saw content eos..");
                    sawInputEOS = true;
                } else {
                    mRawPacket = MediaFrame::Create(data);
                }
            }

            if (sawInputEOS) {
                DEBUG("flushing...");
                mPacketizer->push(NULL);
            } else if (mPacketizer->push(mRawPacket) == kMediaNoError) {
                DEBUG("enqueue buffer done");
                mRawPacket.clear();
            }

            packet = mPacketizer->pull();
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

        packet->timecode += mAnchorTime;

        return packet;
    }
    
    virtual MediaError reset() {
        mAnchorTime     = 0;
        mPacketizer->reset();
        return kMediaNoError;
    }
};

sp<MediaDevice> CreateMp3File(const sp<ABuffer>& buffer) {
    sp<Mp3File> file = new Mp3File;
    if (file->init(buffer) == kMediaNoError) return file;
    return NIL;
}

sp<MediaDevice> CreateMp3Packetizer() {
    return new Mp3Packetizer;
}

ssize_t decodeMPEG4AudioHeader(uint32_t head, uint32_t * sampleRate, uint32_t * numChannels) {
    MPEGAudioFrameHeader frameHeader;
    ssize_t frameLengthInBytes = decodeFrameHeader(head, &frameHeader);
    if (sampleRate)     *sampleRate = frameHeader.sampleRate;
    if (numChannels)    *numChannels = frameHeader.numChannels;
    return frameLengthInBytes;
}

int IsMp3File(const sp<ABuffer>& buffer) {
    MPEGAudioFrameHeader mpa;
    uint32_t head;
    ssize_t offset = locateFirstFrame(buffer, &mpa, NULL, &head);
    if (offset < 0) return 0;
    
    int score = 0;
    while (score < 100) {
        buffer->skipBytes(mpa.frameLengthInBytes);
        uint32_t next = buffer->rb32();
        if ((head & kHeaderMask) != (next & kHeaderMask)) {
            break;
        }
        score += 10;
    }
    
    return 100;
}

__END_NAMESPACE_MPX

