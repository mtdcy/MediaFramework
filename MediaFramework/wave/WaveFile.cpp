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

#define LOG_TAG "WaveFile"
#define LOG_NDEBUG 0
#include "MediaDefs.h"
#include "MediaFile.h"
#include "asf/Asf.h"
#include "tags/id3/ID3.h"

// TODO: 
// 1. fix support for compressed audio, like mp3...
__BEGIN_NAMESPACE_MPX

static const size_t kFrameSize  = 2048;

static FORCE_INLINE bool isValidChunkID(const String& ckID) {
    for (int i = 0; i < 4; i++) {
        if (ckID[i] < ' ' || ckID[i] > 126) return false;
    }

    return true;
}

struct WaveFile : public MediaFile {
    sp<Content>         mContent;
    int64_t             mDataOffset;
    int64_t             mDataLength;
    sp<Message>         mID3v1;
    sp<Message>         mID3v2;
    ASF::WAVEFORMATEX   mWAVE;

    // dwSampleLength: Number of samples (per channel)
    // for non-pcm formats
    uint32_t            dwSampleLength;

    WaveFile() : MediaFile(),
    mDataOffset(0), mDataLength(0),
    mWAVE(), dwSampleLength(0) { }

    // refer to:
    // 1. http://www-mmsp.ece.mcgill.ca/documents/audioformats/wave/wave.html
    MediaError init(sp<Content>& pipe) {
        mContent = pipe;

        mID3v2 = ID3::ReadID3v2(pipe);
        const int64_t waveStart = pipe->tell();
        mID3v1 = ID3::ReadID3v1(pipe);
        const int64_t waveEnd = pipe->tell();
        
        if (pipe->length() - waveStart < 44) {
            ERROR("pipe is too small.");
            return kMediaErrorBadContent;
        }
        pipe->seek(waveStart);

        sp<Buffer> ck = pipe->read(12);
        BitReader br(ck->data(), ck->size());

        if (br.readS(4) != "RIFF") {
            ERROR("not a RIFF file.");
            return kMediaErrorBadContent;
        }

        uint32_t ckSize = br.rl32();
        if (br.readS(4) != "WAVE") {
            ERROR("not a WAVE file.");
            return kMediaErrorBadContent;
        }

        DEBUG("wave file length %u.", ckSize);

        bool success = false;
        while (pipe->tell() + 8 < pipe->length()) {
            sp<Buffer> ck = pipe->read(8);
            BitReader br(ck->data(), ck->size());

            String ckID     = br.readS(4);
            size_t ckSize   = br.rl32();

            if (ckID == "data") {
                DEBUG("data chunk length %zu.", ckSize);
                mDataOffset     = pipe->tell();
                mDataLength     = ckSize;
                success         = true;
                break;
            } else if (!isValidChunkID(ckID) || pipe->tell() + ckSize > pipe->length()) {
                DEBUG("unknown chunk [%s] length %zu.", ckID.c_str(), ckSize);
                break;
            }
            
            if (pipe->length() < pipe->tell() + ckSize) {
                ERROR("corrupt file?");
                break;
            }

            DEBUG("chunk [%s] length %zu", ckID.c_str(), ckSize);
            sp<Buffer> ckData = pipe->read(ckSize);
            BitReader brData(ckData->data(), ckData->size());

            if (ckID == "fmt ") {
                if (mWAVE.parse(brData) != kMediaNoError) {
                    ERROR("error parse fmt chunk.");
                    break;
                }
            } else if (ckID == "fact") {
                dwSampleLength = brData.rl32();
                DEBUG("fact chunk with dwSampleLength = %u.", dwSampleLength);
            }

            // If the chunk size is an odd number of bytes,
            // a pad byte with value zero is written after ckData
            if (ckSize & 0x1) {
                pipe->skip(1);
            }
        }

        return success ? kMediaNoError : kMediaErrorBadContent;
    }

    sp<Message> formats() const {

        sp<Message> formats = new Message;
        formats->setInt32(kKeyFormat, kFileFormatWave);
        // calc useful values.
        int64_t duration = 0;
        if (mWAVE.wFormatTag == ASF::WAVE_FORMAT_PCM) {
            CHECK_GT(mWAVE.nAvgBytesPerSec, 0);
            duration = (1000000LL * mDataLength) / mWAVE.nAvgBytesPerSec;
        } else if (dwSampleLength > 0 && mWAVE.nSamplesPerSec) {
            duration = (1000000LL * dwSampleLength) / mWAVE.nSamplesPerSec;
        } else if (mWAVE.nAvgBytesPerSec) {
            duration = (1000000LL * mDataLength) / mWAVE.nAvgBytesPerSec;
        } else {
            WARN("FIXME: calc duration.");
        }
        if (duration) formats->setInt64(kKeyDuration, duration);

        int32_t bitrate = 0;
        if (mWAVE.wBitsPerSample) {
            bitrate = mWAVE.nSamplesPerSec * mWAVE.nChannels * mWAVE.wBitsPerSample;
        } else {
            bitrate = mWAVE.nAvgBytesPerSec << 3;
        }
        if (bitrate) formats->setInt32(kKeyBitrate, bitrate);

        sp<Message> track = new Message;

        track->setInt32(kKeyCodecType,      kCodecTypeAudio);
        track->setInt32(kKeyFormat,         kAudioCodecPCM);
        track->setInt32(kKeyChannels,       mWAVE.nChannels);
        track->setInt32(kKeySampleRate,     mWAVE.nSamplesPerSec);
        if (bitrate)
            track->setInt32(kKeyBitrate,    bitrate);
        if (duration)
            track->setInt64(kKeyDuration,   duration);

        if (mWAVE.wBitsPerSample)
            track->setInt32(kKeyBits,       mWAVE.wBitsPerSample);

        formats->setObject("track-0",       track);
        return formats;
    }

    void seek(const MediaTime& time) {
        int32_t byterate = mWAVE.nAvgBytesPerSec;
        if (mWAVE.wBitsPerSample) {
            byterate = (mWAVE.nChannels * mWAVE.nSamplesPerSec * mWAVE.wBitsPerSample) / 8;
        }
        int64_t offset = time.seconds() * byterate;

        if (offset > mDataLength) offset = mDataLength;
        int32_t align = (mWAVE.nChannels * mWAVE.wBitsPerSample) / 8;
        if (mWAVE.nBlockAlign) align = mWAVE.nBlockAlign;

        offset = (offset / mWAVE.nBlockAlign) * mWAVE.nBlockAlign;

        mContent->seek(mDataOffset + offset);
    }

    virtual sp<MediaPacket> read(const eReadMode& mode, const MediaTime& ts) {
        if (ts != kMediaTimeInvalid) {
            seek(ts);
        }

        const size_t sampleBytes = ((mWAVE.nChannels * mWAVE.wBitsPerSample) >> 3);
        int64_t samplesRead = (mContent->tell() - mDataOffset) / sampleBytes;
        MediaTime pts (samplesRead, mWAVE.nSamplesPerSec);

        sp<Buffer> data = mContent->read(kFrameSize * sampleBytes);
        if (data.isNIL()) {
            INFO("EOS...");
            return NULL;
        }

        sp<MediaPacket> packet = MediaPacketCreate(data);

        packet->index       = 0;
        packet->type        = kFrameTypeSync;
        packet->pts         = pts;
        packet->dts         = pts;
        packet->duration    = (packet->size) / sampleBytes;

        return packet;
    }
};

sp<MediaFile> CreateWaveFile(sp<Content>& pipe) {
    sp<WaveFile> wave = new WaveFile;
    if (wave->init(pipe) == kMediaNoError)
        return wave;
    return NULL;
}

int IsWaveFile(const sp<Buffer>& data) {
    BitReader br(data->data(), data->size());
    
    int score = 0;
    while (br.remianBytes() > 16 && score < 100) {
        String ckID = br.readS(4);
        uint32_t ckSize = br.rl32();
        DEBUG("found trunk %s length %zu", ckID.c_str(), ckSize);
        
        if (ckID == "RIFF" && (br.readS(4) == "WAVE")) {
            score += 50;
            continue;
        }
        
        if (ckID == "fmt " || ckID == "data" || ckID == "fact") {
            score += 10;
        }
        
        if (ckSize > br.remianBytes()) break;
        
        br.skipBytes(ckSize);
    }
    return score > 100 ? 100 : score;
}

__END_NAMESPACE_MPX
