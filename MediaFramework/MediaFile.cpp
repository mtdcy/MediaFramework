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
 * 3. Neither the name of the copyright holder nor the names of its 
 *    contributors may be used to endorse or promote products derived from 
 *    this software without specific prior written permission.
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


// File:    MediaFile.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#define LOG_TAG "MediaFile"
#define LOG_NDEBUG 0

#include "MediaFile.h"
#include <MediaFramework/MediaTypes.h>
#include <MediaFramework/tags/id3/ID3.h>
#include "matroska/EBML.h"

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a > b ? b : a)
#define MASK(n) ((1<<(n))-1)

__BEGIN_NAMESPACE_MPX

const static size_t kCommonHeadLength = 32;
const static size_t kScanLength = 32 * 1024ll;

// return score
static int scanMP3(const sp<Buffer>& data);
static int scanMatroska(const sp<Buffer>& data);
int IsMp4File(const sp<Buffer>& data);
int IsWaveFile(const sp<Buffer>& data);

static eFileFormat GetFormat(sp<Content>& pipe) {
    int score = 0;

    // skip id3v2
    ID3::SkipID3v2(pipe);
    
    sp<Buffer> header = pipe->read(kCommonHeadLength);
    if (header == 0) {
        ERROR("content size is too small");
        return kFileFormatInvalid;
    }

    const int64_t startPos = pipe->tell() - kCommonHeadLength;
    DEBUG("startPos = %" PRId64, startPos);

    BitReader br(header->data(), header->size());

    // formats with 100 score by check fourcc
    {
        static struct {
            const char *        head;
            const size_t        skip;
            const char *        ext;        // extra text
            eFileFormat         format;
        } kFourccMap[] = {
            {"fLaC",    0,  NULL,       kFileFormatFlac     },
            {"RIFF",    4,  "WAVE",     kFileFormatWave     },
            {"RIFF",    4,  "AVI ",     kFileFormatAvi      },
            {"RIFF",    4,  "AVIX",     kFileFormatAvi      },
            {"RIFF",    4,  "AVI\x19",  kFileFormatAvi      },
            {"RIFF",    4,  "AMV ",     kFileFormatAvi      },
            // END OF LIST
            {NULL,      0,  NULL,       kFileFormatInvalid  },
        };
        
        eFileFormat format = kFileFormatInvalid;
        for (size_t i = 0; kFourccMap[i].head; i++) {
            const String head = br.readS(strlen(kFourccMap[i].head));
            if (kFourccMap[i].ext) {
                if (kFourccMap[i].skip) br.skip(kFourccMap[i].skip);
                const String ext = br.readS(strlen(kFourccMap[i].ext));
                if (head == kFourccMap[i].head &&
                    ext == kFourccMap[i].ext) {
                    format = kFourccMap[i].format;
                    break;
                }
            } else {
                if (head == kFourccMap[i].head) {
                    format = kFourccMap[i].format;
                    break;
                }
            }
            
            br.reset();
        }
        
        if (format != kFileFormatInvalid) {
            // reset pipe to start pos
            pipe->seek(startPos);
            return format;
        }

        br.reset();
    }

    // formats with lower score by scanning header
    header->resize(kScanLength);
    header->write(*pipe->read(kScanLength - kCommonHeadLength));

    struct {
        int (*scanner)(const sp<Buffer>& data);
        eFileFormat format;
    } kScanners[] = {
        { IsWaveFile,           kFileFormatWave     },
        { IsMp4File,            kFileFormatMp4      },
        { EBML::IsMatroskaFile, kFileFormatMkv      },
        { scanMP3,              kFileFormatMp3      },
        { NULL,                 kFileFormatInvalid  }
    };

    eFileFormat format = kFileFormatInvalid;
    for (size_t i = 0; kScanners[i].scanner; i++) {
        int c = kScanners[i].scanner(header);
        DEBUG("%4s, score = %d", (const char *)&kScanners[i].format, c);
        if (c > score) {
            score = c;
            format = kScanners[i].format;

            if (score >= 100) break;
        }
    }

    // TODO: seek to the right pos
    pipe->seek(startPos);
    return format;
}

ssize_t locateFirstFrame(const Buffer& data, size_t *frameLength);
int scanMP3(const sp<Buffer>& data) {
    size_t frameLength = 0;
    ssize_t offset = locateFirstFrame(*data, &frameLength);

    if (offset >= 0 && frameLength > 4) {
        return 100;
    }

    return 0;
}

sp<MediaFile> CreateMp3File(sp<Content>& pipe);
sp<MediaFile> CreateMp4File(sp<Content>& pipe);
sp<MediaFile> CreateMatroskaFile(sp<Content>& pipe);
sp<MediaFile> CreateLibavformat(sp<Content>& pipe);
sp<MediaFile> CreateWaveFile(sp<Content>& pipe);
sp<MediaFile> MediaFile::Create(sp<Content>& pipe, const eMode mode) {
    CHECK_TRUE(mode == Read, "TODO: only support read");
    
    String env = GetEnvironmentValue("FORCE_AVFORMAT");
    bool force = env.equals("1") || env.lower().equals("yes");
    
    const eFileFormat format = GetFormat(pipe);
    switch (format) {
        case kFileFormatWave:
            return force ? CreateLibavformat(pipe) : CreateWaveFile(pipe);
        case kFileFormatMp3:
            return force ? CreateLibavformat(pipe) : CreateMp3File(pipe);
        case kFileFormatMp4:
            return force ? CreateLibavformat(pipe) : CreateMp4File(pipe);
        case kFileFormatMkv:
            return force ? CreateLibavformat(pipe) : CreateMatroskaFile(pipe);
        default:
            return CreateLibavformat(pipe);
    }
}

__END_NAMESPACE_MPX
