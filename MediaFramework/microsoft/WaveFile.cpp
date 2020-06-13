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
#include "MediaTypes.h"
#include "MediaFile.h"
#include "Microsoft.h"
#include "RIFF.h"
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

struct FMTChunk : public RIFF::Chunk {
    Microsoft::WAVEFORMATEX     Wave;
    
    FMTChunk(uint32_t length) : RIFF::Chunk(FOURCC('fmt '), length) { }
    
    virtual MediaError parse(BitReader& br) {
        if (br.remianBytes() < WAVEFORMATEX_MIN_LENGTH)
            return kMediaErrorBadContent;
        return Wave.parse(br);
    }
};

static sp<RIFF::Chunk> ReadChunk(sp<Content>& pipe) {
    sp<Buffer> data = pipe->read(RIFF_CHUNK_MIN_LENGTH);
    if (data.isNIL() || data->size() < RIFF_CHUNK_MIN_LENGTH)
        return NULL;
    BitReader br (data->data(), data->size());
    
    const uint32_t name     = br.rl32();
    const uint32_t length   = br.rl32();
    sp<RIFF::Chunk> ck;
    uint32_t _length = length;  // we may no read all data
    if (name == FOURCC('RIFF')) {
        ck = new RIFF::RIFFChunk(length);
        _length = 4;
    } else if (name == FOURCC('data')) {
        ck = new RIFF::VOIDChunk(name, length);
        _length = 0;
    } else if (name == FOURCC('fmt ')) {
        ck = new FMTChunk(length);
    } else {
        ck = new RIFF::SKIPChunk(name, length);
    }
    
    if (_length) {
        data = pipe->read(_length);
        if (data.isNIL() || data->size() < _length)
            return NULL;
        
        BitReader br0 (data->data(), data->size());
        if (ck->parse(br0) == kMediaNoError) return ck;
        else return NULL;
    }
    return ck;
}

struct WaveFile : public MediaFile {
    sp<Content>         mContent;
    int64_t             mDataOffset;
    int64_t             mDataLength;
    sp<Message>         mID3v1;
    sp<Message>         mID3v2;
    sp<FMTChunk>        mFormat;

    // dwSampleLength: Number of samples (per channel)
    // for non-pcm formats
    uint32_t            dwSampleLength;

    WaveFile() : MediaFile(),
    mDataOffset(0), mDataLength(0),
    mFormat(NULL), dwSampleLength(0) { }

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
        
        sp<RIFF::RIFFChunk> ck = ReadChunk(pipe);
        if (ck.isNIL() || ck->Name != FOURCC('RIFF') || ck->Data != FOURCC('WAVE')) {
            ERROR("not a WAVE file.");
            return kMediaErrorBadContent;
        }

        DEBUG("wave file length %u.", ck->Length);

        bool success = false;
        for (;;) {
            sp<RIFF::Chunk> ck = ReadChunk(pipe);
            if (ck.isNIL()) break;

            if (ck->Name == FOURCC('data')) {
                DEBUG("data chunk length %zu.", ck->Length);
                mDataOffset     = pipe->tell();
                mDataLength     = ck->Length;
                success         = true;
                break;
            } else if (ck->Name == FOURCC('fmt ')) {
                mFormat = ck;
            }
        }

        return success && !mFormat.isNIL() ? kMediaNoError : kMediaErrorBadContent;
    }

    sp<Message> formats() const {
        if (mFormat.isNIL()) return NULL;
        const Microsoft::WAVEFORMATEX& wave = mFormat->Wave;

        sp<Message> formats = new Message;
        formats->setInt32(kKeyFormat, kFileFormatWave);
        // calc useful values.
        int64_t duration = 0;
        if (wave.wFormat == Microsoft::WAVE_FORMAT_PCM) {
            CHECK_GT(wave.nAvgBytesPerSec, 0);
            duration = (1000000LL * mDataLength) / wave.nAvgBytesPerSec;
        } else if (dwSampleLength > 0 && wave.nSamplesPerSec) {
            duration = (1000000LL * dwSampleLength) / wave.nSamplesPerSec;
        } else if (wave.nAvgBytesPerSec) {
            duration = (1000000LL * mDataLength) / wave.nAvgBytesPerSec;
        } else {
            WARN("FIXME: calc duration.");
        }
        if (duration) formats->setInt64(kKeyDuration, duration);

        int32_t bitrate = 0;
        if (wave.wBitsPerSample) {
            bitrate = wave.nSamplesPerSec * wave.nChannels * wave.wBitsPerSample;
        } else {
            bitrate = wave.nAvgBytesPerSec << 3;
        }
        if (bitrate) formats->setInt32(kKeyBitrate, bitrate);

        sp<Message> track = new Message;

        track->setInt32(kKeyType,      kCodecTypeAudio);
        track->setInt32(kKeyFormat,         kAudioCodecPCM);
        track->setInt32(kKeyChannels,       wave.nChannels);
        track->setInt32(kKeySampleRate,     wave.nSamplesPerSec);
        if (bitrate)
            track->setInt32(kKeyBitrate,    bitrate);
        if (duration)
            track->setInt64(kKeyDuration,   duration);

        if (wave.wBitsPerSample)
            track->setInt32(kKeySampleBits, wave.wBitsPerSample);
        
        sp<Buffer> acm = new Buffer(WAVEFORMATEX_MAX_LENGTH);
        BitWriter bw(acm->data(), acm->capacity());
        wave.compose(bw);
        acm->step(bw.size());
        track->setObject(kKeyMicorsoftACM, acm);

        formats->setObject(kKeyTrack,       track);
        return formats;
    }

    void seek(const MediaTime& time) {
        const Microsoft::WAVEFORMATEX& wave = mFormat->Wave;
        int32_t byterate = wave.nAvgBytesPerSec;
        if (wave.wBitsPerSample) {
            byterate = (wave.nChannels * wave.nSamplesPerSec * wave.wBitsPerSample) / 8;
        }
        int64_t offset = time.seconds() * byterate;

        if (offset > mDataLength) offset = mDataLength;
        int32_t align = (wave.nChannels * wave.wBitsPerSample) / 8;
        if (wave.nBlockAlign) align = wave.nBlockAlign;

        offset = (offset / wave.nBlockAlign) * wave.nBlockAlign;

        mContent->seek(mDataOffset + offset);
    }

    virtual sp<MediaPacket> read(const eReadMode& mode, const MediaTime& ts) {
        if (ts != kMediaTimeInvalid) {
            seek(ts);
        }
        
        const Microsoft::WAVEFORMATEX& wave = mFormat->Wave;
        const size_t sampleBytes = ((wave.nChannels * wave.wBitsPerSample) >> 3);
        int64_t samplesRead = (mContent->tell() - mDataOffset) / sampleBytes;
        MediaTime pts (samplesRead, wave.nSamplesPerSec);

        sp<Buffer> data = mContent->read(kFrameSize * sampleBytes);
        if (data.isNIL()) {
            INFO("EOS...");
            return NULL;
        }

        sp<MediaPacket> packet = MediaPacket::Create(data);

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
    if (data->size() < RIFF_CHUNK_LENGTH) return 0;
    
    BitReader br(data->data(), data->size());
    
    // RIFF CHUNK
    uint32_t name   = br.rl32();
    uint32_t length = br.rl32();
    uint32_t wave   = br.rl32();
    if (name != FOURCC('RIFF') || wave != FOURCC('WAVE'))
        return 0;
    
    int score = 60;
    
    while (br.remianBytes() > RIFF_CHUNK_MIN_LENGTH && score < 100) {
        name    = br.rl32();
        length  = br.rl32();
        
        // known chunk names
        if (name == FOURCC('fmt ') ||
            name == FOURCC('data') ||
            name == FOURCC('fact')) {
            score += 20;
        }
        
        if (length > br.remianBytes()) break;
        
        br.skipBytes(length);
    }
    
    return score > 100 ? 100 : score;
}

__END_NAMESPACE_MPX
