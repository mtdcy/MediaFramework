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
#include "Mp3File.h"

#include "tags/id3/ID3.h"

#include <stdio.h> // FIXME: sscanf

namespace mtdcy {
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

    const static uint32_t kHeaderMask = (0xffe00000 | (3 << 17) | (3 << 10) | (3 << 19) | (3 << 6));
    static int64_t locateFirstFrame(const Buffer& data, 
            struct MPEGAudioFrameHeader *frameHeader,
            size_t *possible/* possible head position */,
            uint32_t *_head/* found head */) {
        // locate first frame:
        //  current and next frame header is valid
        BitReader br(data);
        uint32_t head = 0;
        for (size_t i = 0; i < br.size(); i++) {
            head = (head << 8) | br.r8();

            // test current frame
            ssize_t frameLength = decodeFrameHeader(head, frameHeader);
            if (frameLength <= 4) continue;

            if (i + frameLength > br.size()) {
                DEBUG("scan to the end.");
                if (possible) *possible = i - 3;
                break;
            }

            // test next head.
            BitReader _br(data);
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
        BitReader br(data);
        uint32_t head = 0;
        for (size_t i = 0; i < br.size(); i++) {
            head = (head << 8) | br.r8();
            if ((head & kHeaderMask) != common) continue;

            ssize_t frameLength = decodeFrameHeader(head, frameHeader);
            if (frameLength <= 4) continue;
            if (i - 3 + frameLength > br.size()) {
                if (possible) *possible = i - 3;
                break;
            }

            return i - 3;
        }
        return NAME_NOT_FOUND;
    }

    ssize_t Mp3File::decodeFrameHeader(uint32_t head) {
        return mtdcy::decodeFrameHeader(head, NULL);
    }

    ssize_t Mp3File::locateFirstFrame(const Buffer& data, size_t *frameLength) {
        MPEGAudioFrameHeader mpa;
        ssize_t offset = mtdcy::locateFirstFrame(data, &mpa, NULL, NULL);

        if (offset < 0) return NAME_NOT_FOUND;

        *frameLength    = mpa.frameLengthInBytes;
        return offset;
    }

    // refer to:
    // 1. http://gabriel.mp3-tech.org/mp3infotag.html#versionstring
    // 2. http://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header
    // 3. http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
    Mp3File::Mp3File(const sp<Content>& pipe, const Message& formats) :
        MediaExtractor(),
        mContent(pipe), mFormats(NULL),
        mFirstFrameOffset(0),
        mVBR(false),
        mNumFrames(0),
        mNumBytes(0),
        mBitRate(0),
        mDuration(0),
        mAnchorTime(0),
        mSamplesRead(0),
        mPartialFrame(0),
        mPacketizer(new Mp3Packetizer)
    {
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
                        const Message& values = id3.values();
                        DEBUG("id3v2 found: %s", values.string().c_str());
#if 1
                        // information for gapless playback
                        // http://yabb.jriver.com/interact/index.php?topic=65076.msg436101#msg436101
                        const char *iTunSMPB = values.findString("iTunSMPB");
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
                        outputFormat->set<Message>(Media::ID3v2, values);
                        mFirstFrameOffset = pipe->tell();
                    }
                }
            }
        }

        size_t totalLength = pipe->size();
        DEBUG("totalLength %zu", totalLength);
        pipe->seek(totalLength - ID3v1::kLength);
        sp<Buffer> tail = pipe->read(ID3v1::kLength);
        if (tail != NULL && tail->size() == ID3v1::kLength) {
            ID3v1 id3;
            if (id3.parse(*tail) == OK) {
                outputFormat->set<Message>(Media::ID3v1, id3.values());
                totalLength -= ID3v1::kLength;
            }
        }

        pipe->seek(mFirstFrameOffset);

        MPEGAudioFrameHeader frameHeader;
        sp<Buffer> scanData = pipe->read(kScanLength);
        if (scanData == 0) {
            ERROR("file is corupt?");
            return;
        }

        // skip junk before first frame.
        ssize_t result = mtdcy::locateFirstFrame(*scanData, &frameHeader, NULL, NULL);
        if (result < 0) {
            ERROR("failed to locate the first frame.");
            return;
        } else if (result > 0) {
            DEBUG("%zu bytes junk data before first frame", (size_t)result);
        }
        mFirstFrameOffset   += result;
        DEBUG("mFirstFrameOffset = %" PRId64, mFirstFrameOffset);

        pipe->seek(mFirstFrameOffset);
        sp<Buffer> firstFrame = pipe->read(frameHeader.frameLengthInBytes);
        if (firstFrame->size() < frameHeader.frameLengthInBytes) {
            ERROR("content is too small.");
            return;
        }
        DEBUG("first frame size %zu", frameHeader.frameLengthInBytes);

        // decode first frame
        const size_t offset = 4 + kSideInfoOffset[frameHeader.Version == MPEG_VERSION_1 ? 0 : 1]
            [frameHeader.numChannels - 1];
        firstFrame->skip(offset);

        DEBUG("side info: %s", firstFrame->string().c_str());

        mNumBytes = totalLength - mFirstFrameOffset;    // default value
        if (!firstFrame->compare("Xing") || !firstFrame->compare("Info")) {
            mVBR                = !firstFrame->compare("Xing");
            mFirstFrameOffset   += frameHeader.frameLengthInBytes;  // skip first frame
            parseXingHeader(*firstFrame, *ast);
            // XXX: Lame's num bytes including the first frame
            mNumBytes           -= frameHeader.frameLengthInBytes;  // remove first frame
        } else if (!firstFrame->compare("VBRI")) {
            mVBR    = true;
            mFirstFrameOffset   += frameHeader.frameLengthInBytes;
            parseVBRIHeader(*firstFrame);
        }

        DEBUG("number bytes of data %" PRId64 " pipe length %" PRId64, 
                mNumBytes, pipe->size());

        if (mNumFrames != 0) {
            DEBUG("calc duration based on frame count.");
            mDuration   = (1E6 * mNumFrames * frameHeader.samplesPerFrame) / frameHeader.sampleRate;
            if (mVBR)
                mBitRate    = (8 * 1E6 * mNumBytes) / mDuration + .5f;
            else
                mBitRate    = frameHeader.bitRate;
        } else {
            DEBUG("calc duration based on bitrate.");
            mBitRate    = frameHeader.bitRate;
            mDuration   = (8 * 1E6 * mNumBytes) / mBitRate;
        }
        DEBUG("mBitRate %d, mDuration %" PRId64, mBitRate, mDuration);

#if 1
        // hack for truncated/incomplete files.
        if (mNumBytes > totalLength - mFirstFrameOffset) {
            WARN("Fix number of bytes, expected %" PRId64 " bytes, but we got max bytes %" PRId64,
                    mNumBytes, totalLength - mFirstFrameOffset);
            mNumBytes = totalLength - mFirstFrameOffset;
            mDuration = (8000000LL * mNumBytes) / mBitRate;
        }
#endif

        if (mTOC.size()) {
            // add last entry to TOC
            mTOC.push(mFirstFrameOffset + mNumBytes);
            pipe->seek(mTOC[0]);
        } else {
            pipe->seek(mFirstFrameOffset);
        }

        DEBUG("firstFrameOffset %" PRId64, mFirstFrameOffset);

        ast->setInt32(kFormat, kAudioCodecFormatMP3);
        // FIXME:
        //ast->setInt64(Media::Duration, mDuration);
        //ast->setInt32(Media::Bitrate, mBitRate);
        ast->setInt32(kChannels, frameHeader.numChannels);
        ast->setInt32(kSampleRate, frameHeader.sampleRate);

        outputFormat->setInt32(kFormat, kFileFormatMP3);
        outputFormat->set<Message>("track-0", *ast);

        mFormats    = outputFormat;
        DEBUG("%s", ast->string().c_str());
        DEBUG("%s", mFormats->string().c_str());
    }

    Mp3File::~Mp3File() {
        DEBUG("~Mp3File");
    }


    String Mp3File::string() const {
        return mFormats != 0 ? mFormats->string() : String("Mp3File");
    }

    status_t Mp3File::status() const {
        return mFormats != 0 ? OK : NO_INIT;
    }

    sp<Message> Mp3File::formats() const {
        return mFormats;
    }

    // http://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header#XINGHeader
    // http://gabriel.mp3-tech.org/mp3infotag.html#versionstring
    bool Mp3File::parseXingHeader(const Buffer& firstFrame, Message& ast) {
        BitReader br(firstFrame);
        br.skipBytes(4);       // skip XING or Info

        const uint32_t flags = br.rb32();
        DEBUG("XING flags: %#x", flags);

        if (flags & 0x0001) {
            mNumFrames      = br.rb32();
            DEBUG("Xing: number frames %d", mNumFrames);
        } else {
            DEBUG("Xing: no number frames.");
        }

        // including the first frame.
        if (flags & 0x0002) {
            mNumBytes       = br.rb32();
            DEBUG("Xing: number bytes %d", (int32_t)mNumBytes);
        } else {
            DEBUG("Xing: no number bytes.");
        }

        if (flags & 0x0004) {
            for (int i = 0; i < 100; i++) {
                uint8_t pos     = br.r8();
                mTOC.push((mNumBytes * pos) / 256 + mFirstFrameOffset);
            }
            DEBUG("TOC: %d %d ... %d %d", mTOC[0], mTOC[1], mTOC[98], mTOC[99]);
        } else {
            DEBUG("Xing: no toc");
        }

        if (flags & 0x0008) {
            int quality = br.rb32();
        }

        // LAME extension 
        String encoder  = br.readS(9);
        br.skipBytes(1);               // Info Tag revision & VBR method
        ast.setString("encoder", encoder);

        uint8_t lpf     = br.r8();     // low pass filter value.
        DEBUG("lpf: %u Hz.", lpf * 100);
        ast.setInt32("low-pass-filter", lpf * 100);

        br.skipBytes(8);               // replay gain
        br.skipBytes(1);               // encode flags & ATH Type
        br.skipBytes(1);               // specified or minimal bitrate

        // refer to ffmpeg/libavformat/mp3dec.c:mp3_parse_info_tag
        if (encoder.startsWith("LAME") || 
                encoder.startsWith("Lavf") ||
                encoder.startsWith("Lavc")) {
            uint32_t delays     = br.read(12);
            uint32_t paddings   = br.read(12);
            if (delays != 0) {
                ast.setInt32("encoder-delay",      delays);
                ast.setInt32("encoder-padding",    paddings);
            }
        }

        // 12 bytes remains.

        return true;
    }

    // https://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header#VBRIHeader
    // https://www.crifan.com/files/doc/docbook/mpeg_vbr/release/webhelp/vbri_header.html
    bool Mp3File::parseVBRIHeader(const Buffer& firstFrame) {
        BitReader br(firstFrame);
        br.skipBytes(4);        // skip 'VBRI'
        br.skipBytes(6);        // skip version & delay & quality

        mNumBytes               = br.rb32();    // total size
        mNumFrames              = br.rb32();    // total frames

        uint16_t numEntries     = br.rb16();
        uint16_t scaleFactor    = br.rb16();
        uint16_t entrySize      = br.rb16();
        uint16_t entryFrames    = br.rb16();

        DEBUG("numEntries %d, entrySize %d, scaleFactor %d, entryFrames %d", 
                numEntries, entrySize, scaleFactor, entryFrames);

        if (numEntries > 0) {
            int64_t offset = mFirstFrameOffset;
            int64_t frames = 0;
            mTOC.push(offset);
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

                offset += length * scaleFactor;
                mTOC.push(offset);
            }
            // end

            DEBUG("TOC: %d %d ... %d %d", 
                    mTOC[0], mTOC[1], 
                    mTOC[numEntries-2],
                    mTOC[numEntries-1]);
        }

        return true;
    }

    status_t Mp3File::configure(const Message& options) {
        MediaTime& seekTime = options.find<MediaTime>(kTime);
        if (seekTimeUs >= 0) {
            int64_t pos = 0;
            float percent   = seekTimeUs / (float)mDuration;
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
            mAnchorTime     = seekTimeUs;
            mSamplesRead    = 0;

            mPacketizer->flush();
        }
        return OK;
    }

    sp<MediaPacket> Mp3File::read(size_t index, eModeReadType mode, int64_t ts) {
        CHECK_EQ(index, 0);
        CHECK_NE(mode, kModeReadPeek); // don't support this 
        bool sawInputEOS = false;

        sp<Buffer> frame = new Buffer(4096); 

        for (;;) {
            if (mPartialFrame == 0 && !sawInputEOS) {
                DEBUG("read content at %" PRId64 "/%" PRId64,
                        mContent->tell(),
                        mContent->size());
                // mInternalBuffer must be twice of this
                mPartialFrame = mContent->read(2048);
                if (mPartialFrame == 0) {
                    DEBUG("saw content eos..");
                    sawInputEOS = true;
                }
            }

            if (sawInputEOS) {
                DEBUG("flushing...");
                mPacketizer->enqueue(Buffer());
            } else if (mPacketizer->enqueue(*mPartialFrame)) {
                DEBUG("enqueue buffer done");
                mPartialFrame.clear();
            }

            if (!mPacketizer->dequeue(*frame)) {
                if (sawInputEOS) {
                    break;
                }
                continue;
            } else {
                break;
            }
        }

        if (frame->size() == 0) {
            INFO("eos...");
            return NULL;
        }

        MPEGAudioFrameHeader mpa;
        BitReader br(*frame);
        ssize_t frameLength = mtdcy::decodeFrameHeader(br.rb32(), &mpa);
        CHECK_GT((int32_t)frameLength, 4);

        mSamplesRead    += mpa.samplesPerFrame;

        sp<Message> meta = frame->meta();

        meta->setInt64(Media::PTS, mAnchorTime);

        mAnchorTime     += (1000000LL * mSamplesRead) / mpa.sampleRate;

        //DEBUG("peek: %s", frame->string().c_str());

        sp<MediaPacket> packet = new MediaPacket;
        return packet;
    }

    ///////////////////////////////////////////////////////////////////////////

    Mp3Packetizer::Mp3Packetizer() :
        mInternalBuffer(4096, Buffer::BUFFER_RING),
        mCommonHead(0), mNeedMoreData(true), mFlushing(false)
    {
    }

    Mp3Packetizer::~Mp3Packetizer() {
    }

    bool Mp3Packetizer::enqueue(const Buffer& in) {
        if (in.size() == 0) {
            DEBUG("flush...");
            mFlushing   = true;
        } else {
            DEBUG("enqueue %zu (%zu)", in.size(), mInternalBuffer.empty());
            while (mInternalBuffer.empty() < in.size()) {
                if (mNeedMoreData) {
                    CHECK_TRUE(mInternalBuffer.resize(mInternalBuffer.capacity() * 2));
                    DEBUG("resize internal buffer => %zu", mInternalBuffer.capacity());
                } else {
                    return false;
                }
            }

            mInternalBuffer.write(in);
            mNeedMoreData   = false;
        }
        return true;
    }

    bool Mp3Packetizer::dequeue(Buffer& out) {
        DEBUG("internal buffer ready bytes %zu", mInternalBuffer.ready());

        if (mInternalBuffer.ready() <= 4) {
            if (!mFlushing) mNeedMoreData = true;
            return false;
        }

        // only at the very beginning, find the common header
        if (__builtin_expect(mCommonHead == 0, false)) {
            DEBUG("find common header");

            MPEGAudioFrameHeader mpa;
            size_t possible = 0;
            uint32_t head = 0;
            ssize_t offset = locateFirstFrame(mInternalBuffer, &mpa, &possible, &head);

            if (offset < 0) {
                size_t junk = possible ? possible : mInternalBuffer.ready() - 3;
                DEBUG("missing head, skip %zu junk", offset);
                mInternalBuffer.skip(junk);
                return false;
            } else if (offset > 0) {
                DEBUG("skip %zu junk", offset);
                mInternalBuffer.skip(offset);
            }
            mCommonHead = head & kHeaderMask;
            DEBUG("common header %#x", mCommonHead);
        }

        size_t possible = 0;
        MPEGAudioFrameHeader mpa;
        ssize_t offset = locateFrame(mInternalBuffer, mCommonHead, &mpa, &possible);
        if (offset < 0) {
            if (mFlushing) {
                DEBUG("%zu drop tailing bytes %s", 
                        mpa.frameLengthInBytes,
                        mInternalBuffer.string().c_str());
            } else {
                size_t junk = possible ? possible : mInternalBuffer.ready() - 3;
                DEBUG("skip junk bytes %zu", junk);
                mNeedMoreData = true;
                mInternalBuffer.skip(junk);
            }
            return false;
        } else if (offset > 0) {
            DEBUG("skip junk bytes %zu before frame", offset);
            mInternalBuffer.skip(offset);
        }

        DEBUG("current frame length %zu (%zu)", 
                mpa.frameLengthInBytes, mInternalBuffer.ready());

        out.write(mInternalBuffer, mpa.frameLengthInBytes);
        mInternalBuffer.skip(mpa.frameLengthInBytes);
        //DEBUG("new packet %s", out.string().c_str());

        return true;
    }

    void Mp3Packetizer::flush() {
        mInternalBuffer.clear();
        mFlushing = false;
    }

    String Mp3Packetizer::string() const  {
        return String::format("Mp3 Packetizer %p", this);
    }
};
