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


// File:    WaveFile.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#define tkLOG_TAG "WaveFile"
//#define tkLOG_NDEBUG 0
#include <toolkit/Toolkit.h>

#include <modules/mp3/id3/ID3.h>

#include "WaveFile.h" 

// TODO: 
// 1. fix support for compressed audio, like mp3...

namespace mtdcy {
    static const size_t kFrameSize  = 4096;

    // refer to: https://tools.ietf.org/html/rfc2361
    enum {
        WAVE_FORMAT_PCM         = 0x0001,
        WAVE_FORMAT_IEEE_FLOAT  = 0x0003,
        WAVE_FORMAT_EXTENSIBLE  = 0xFFFE,
    };

    static const char *WAVE_FORMAT_EXTENSIBLE_GUID = 
        "\x00\x00\x00\x00\x10\x00\x80\x00\x00\xAA\x00\x38\x9B\x71";

    // string should have at least 16 * 3 + 1
    static String printGUID(const sp<Buffer>& guid) {
        const char *s = guid->data();
        const size_t len = guid->size();
        String result;
        for (size_t i = 0; i < len; i++) result.append(String((char)s[i]));
        return result;
    }

    static inline bool isValidChunkID(const String& ckID) {
        for (int i = 0; i < 4; i++) {
            if (ckID[i] < ' ' || ckID[i] > 126) return false;
        }

        return true;
    }

    // refer to:
    // 1. http://www-mmsp.ece.mcgill.ca/documents/audioformats/wave/wave.html
    WaveFile::WaveFile(const sp<Content>& pipe, const sp<Message>& formats)
        :
            MediaFile(), 
            mContent(pipe), mFormats(NULL),
            mDataOffset(0), mDataLength(0), mBytesRead(0)
    {
        sp<Message> outputFormat    = new Message;
        sp<ID3::ID3v2> id3v2        = new ID3::ID3v2(pipe);
        if (id3v2 != 0) {
            sp<Message> values      = id3v2->getValues();
            if (values != 0) {
                outputFormat->setMessage(Media::ID3v2, values);
            } else {
                tkDEBUG("empty ID3v2???");
            }
        }
        const int64_t start = pipe->tell();

        if (!pipe->isStream()) {
            sp<ID3::ID3v1> id3v1    = new ID3::ID3v1(pipe);
            if (id3v1 != 0) {
                sp<Message> values  = id3v1->getValues();
                if (values != 0) 
                    outputFormat->setMessage(Media::ID3v1, values);
                else 
                    tkDEBUG("empty ID3v1???");
            }
            tkCHECK_EQ(start, pipe->tell());
        }

        if (pipe->size() - start < 44) {
            tkERROR("pipe is too small.");
            return;
        }

        sp<Buffer> ck = pipe->read(12);
        sp<BitReader> br = new BitReader(ck);

        if (br->readS(4) != "RIFF") {
            tkERROR("not a RIFF file.");
            return;
        }

        uint32_t ckSize = br->rl32();
        if (br->readS(4) != "WAVE") {
            tkERROR("not a WAVE file.");
            return;
        }

        tkDEBUG("wave file length %u.", ckSize);

        // dwSampleLength: Number of samples (per channel)
        // for non-pcm formats
        uint32_t dwSampleLength = 0;
        int success = 0;
        while (1) {
            sp<Buffer> ck = pipe->read(8);
            sp<BitReader> br = new BitReader(ck);

            String ckID = br->readS(4);
            uint32_t ckSize = br->rl32();

            if (ckID == "fmt ") {
                tkDEBUG("fmt chunk length %u", ckSize);
                sp<Buffer> ckData = pipe->read(ckSize);
                bool ret = parseFormat(ckData);

                if (!ret) {
                    tkERROR("error parse fmt chunk.");
                    break;
                }
            } else if (ckID == "fact") {
                sp<BitReader> fact = new BitReader(pipe->read(4));
                dwSampleLength = fact->rl32();
                tkDEBUG("fact chunk with dwSampleLength = %u.", dwSampleLength);
            } else if (ckID == "data") {
                tkDEBUG("data chunk length %u.", ckSize);
                success = 1;

                mDataOffset     = pipe->tell();
                mDataLength     = ckSize;
                break;
            } else if (!isValidChunkID(ckID) ||
                    pipe->tell() + ckSize > pipe->size()) {
                tkDEBUG("unknown chunk [%s] length %u.",
                        PRINTABLE(ckID), ckSize);
                break;
            } else {
                tkDEBUG("skip chunk [%s] length %u.",
                        PRINTABLE(ckID), ckSize);
                pipe->skip(ckSize);
            }

            // If the chunk size is an odd number of bytes, 
            // a pad byte with value zero is written after ckData
            if (ckSize & 0x1) {
                pipe->skip(1);
            }
        }

        if (!success) {
            tkDEBUG("parse chunks failed.");
            return;
        }

        // calc useful values.
        int64_t duration = 0;
        if (mWave.wFormatTag == WAVE_FORMAT_PCM) {
            tkASSERT(mWave.nAvgBytesPerSec > 0);
            duration = (1000000LL * mDataLength) / mWave.nAvgBytesPerSec;
        } else if (dwSampleLength > 0 && mWave.nSamplesPerSec) {
            duration = (1000000LL * dwSampleLength) / mWave.nSamplesPerSec;
        } else if (mWave.nAvgBytesPerSec) {
            duration = (1000000LL * mDataLength) / mWave.nAvgBytesPerSec;
        } else {
            tkWARN("FIXME: calc duration.");
        }

        if (mWave.wBitsPerSample) {
            mBytesPerSec    = mWave.nSamplesPerSec * mWave.nChannels * mWave.wBitsPerSample / 8;
        } else {
            mBytesPerSec    = mWave.nAvgBytesPerSec;
        }
        tkDEBUG("mBytesPerSec = %.2f", mBytesPerSec);

        // set output formats.
        outputFormat->setString(Media::Format,       Media::File::WAVE);

        String codecType = Media::Codec::PCM;

        sp<Message> audio = new Message;
        audio->setInt32(Media::BufferSize,   kFrameSize);
        audio->setString(Media::Format,      codecType);
        audio->setInt32(Media::Channels,     mWave.nChannels);
        audio->setInt32(Media::SampleRate,   mWave.nSamplesPerSec);
        audio->setInt32(Media::Bitrate,      mWave.nAvgBytesPerSec << 3);
        audio->setInt64(Media::Duration,     duration);

        if (mWave.wBitsPerSample > 0)
            audio->setInt32(Media::BitsPerSample,  mWave.wBitsPerSample);

        outputFormat->setMessage("track-0",     audio);

        mFormats    = outputFormat;
    }

    WaveFile::~WaveFile() {
    }

    String WaveFile::string() const {
        return mFormats != 0 ? mFormats->string() : String("WaveFile");
    }

    status_t WaveFile::status() const {
        return mFormats != 0 ? OK : NO_INIT;
    }

    sp<Message> WaveFile::formats() const {
        return mFormats;
    }

    // There are 3 variants of the Format chunk for sampled data.
    // 1. PCM Format, 2. Non-PCM Formats, 3. Extensible Format.
    bool WaveFile::parseFormat(const sp<Buffer>& ckData) {
        // valid chunk length: 16, 18, 40
        // but we have saw chunk length 50

        if (ckData->size() < 16) {
            tkERROR("invalid format chunk");
            return false;
        }

        sp<BitReader> br        = new BitReader(ckData);

        mWave.wFormatTag        = br->rl16();
        mWave.nChannels         = br->rl16();
        mWave.nSamplesPerSec    = br->rl32();
        mWave.nAvgBytesPerSec   = br->rl32();
        mWave.nBlockAlign       = br->rl16();
        mWave.wBitsPerSample    = br->rl16();

        if (ckData->size() >= 18) {
            mWave.cbSize        = br->rl16();
        }

        tkDEBUG("audio format %u.",     mWave.wFormatTag);
        tkDEBUG("number channels %u.",  mWave.nChannels);
        tkDEBUG("sample rate %u.",      mWave.nSamplesPerSec);
        tkDEBUG("byte rate %u.",        mWave.nAvgBytesPerSec);
        tkDEBUG("block align %u.",      mWave.nBlockAlign);
        tkDEBUG("bits per sample %u.",  mWave.wBitsPerSample);

        if (mWave.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            tkDEBUG("fmt chunk with extensible format.");
            if (ckData->size() < 40 || mWave.cbSize != 22) {
                tkDEBUG("invalid fmt chunk length %u/%u.", ckData->size(), mWave.cbSize);
                return false;
            }

            uint16_t wValidBitsPerSample    = br->rl16();
            uint32_t dwChannelMask          = br->rl32();
            uint16_t wSubFormat             = br->rl16();
            sp<Buffer> guid                 = br->readB(14);

            tkDEBUG("valid bits per sample %u.",    wValidBitsPerSample);
            tkDEBUG("dwChannelMask %#x.",           dwChannelMask);
            tkDEBUG("sub format %u.",               wSubFormat);

            if (!guid->equals(WAVE_FORMAT_EXTENSIBLE_GUID)) {
                tkERROR("invalid extensible format %s.", PRINTABLE(printGUID(guid)));
            }

            if (dwChannelMask != 0 && __builtin_popcount(dwChannelMask) != mWave.nChannels) {
                tkWARN("channels mask mismatch with channel count.");
                dwChannelMask = 0;
            }

            // Fix Format 
            mWave.wFormatTag  = wSubFormat;
        }

        // sanity check.
        if (mWave.wFormatTag == WAVE_FORMAT_PCM || 
                mWave.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            // wBitsPerSample/nChannels/nSamplesPerSec, we have to trust these values
            if (mWave.wBitsPerSample % 8) {
                tkERROR("wBitsPerSample(%u) is wrong.", mWave.wBitsPerSample);
                return false;
            }

            if (!mWave.nChannels || !mWave.nSamplesPerSec) {
                tkERROR("nChannels %d/nSamplesPerSec %d iw wrong", 
                        mWave.nChannels, mWave.nSamplesPerSec);
                return false;
            }

            uint32_t nBytesPerFrame = (mWave.nChannels * mWave.wBitsPerSample) / 8;
            if (mWave.nBlockAlign % nBytesPerFrame) {
                tkERROR("nBlockAlign is wrong.");
                //format.nBlockAlign = a;
                return false;
            }

            // these values we have to trust. and no one will put these wrong.
            uint32_t nBytesPerSec = mWave.nSamplesPerSec * nBytesPerFrame;
            if (nBytesPerSec != mWave.nAvgBytesPerSec) {
                tkWARN("nAvgBytesPerSec correction %d -> %d.",
                        mWave.nAvgBytesPerSec, nBytesPerSec);
                mWave.nAvgBytesPerSec = nBytesPerSec;
            }
        } else {
            if (mWave.wBitsPerSample == 1) {
                // means: don't case about this field.
                tkDEBUG("clear wBitsPerSample(%u).", mWave.wBitsPerSample);
                mWave.wBitsPerSample = 0;
            }
        }

        if (mWave.nBlockAlign == 0) {
            tkDEBUG("fix nBlockAlign => 1");
            mWave.nBlockAlign = 1;
        }

        return true;
    }

    status_t WaveFile::seekTo(int64_t seekTimeUs, int option) {
        int64_t offset = (seekTimeUs * mBytesPerSec) / 1000000LL;

        if (offset > mDataLength) offset = mDataLength;

        if (offset % mWave.nBlockAlign) 
            offset = (offset / mWave.nBlockAlign) * mWave.nBlockAlign;

        mBytesRead  = offset;
        offset      += mDataOffset;

        if (mContent->seek(offset) != offset) {
            tkWARN("seek to %" PRId64 " failed.", offset);
        } else {
            tkINFO("seek to %" PRId64 " usecs, offset = %" PRId64 " bytes", seekTimeUs, offset);
        }

        mOutputEOS  = false;
        return OK;
    }

    sp<Buffer> WaveFile::readFrame() {
        if (mBytesRead >= mDataLength) {
            tkINFO("end of file.");
            return NULL;
        }

        size_t bytesToRead = (kFrameSize / mWave.nBlockAlign) * mWave.nBlockAlign;
        sp<Buffer> frame = mContent->read(bytesToRead);
        if (frame == 0) {
            tkERROR("EOS or Error...");
            return NULL;
        }

        if (frame->size() % mWave.nBlockAlign) {
            tkWARN("this frame is not block aligned. bytesRead = %d, mBlockAlign = %d", 
                    frame->size(), mWave.nBlockAlign);
        }

        int64_t pts = (mBytesRead * 1000000LL) / mBytesPerSec;
        mBytesRead += frame->size();

        frame->meta()->setInt32(Media::Index,  0);
        frame->meta()->setInt32(Media::Flags,  0);
        frame->meta()->setInt64(Media::PTS, pts);
        frame->meta()->setInt64(Media::DTS, pts);
        return frame;
    }
};
