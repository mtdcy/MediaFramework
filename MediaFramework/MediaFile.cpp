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
#include "MediaTypes.h"
#include "id3/ID3.h"
#include "matroska/EBML.h"

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a > b ? b : a)
#define MASK(n) ((1<<(n))-1)

__BEGIN_NAMESPACE_MPX

const static size_t kCommonHeadLength = 32;
const static size_t kScanLength = 32 * 1024ll;

// return score
int IsMp4File(const sp<ABuffer>&);
int IsWaveFile(const sp<ABuffer>&);
int IsAviFile(const sp<ABuffer>&);
int IsMp3File(const sp<ABuffer>&);
static eFileFormat GetFormat(const sp<ABuffer>& data) {
    int score = 0;

    const int64_t startPos = data->offset();
    DEBUG("startPos = %" PRId64, startPos);

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
            {NULL,      0,  NULL,       kFileFormatUnknown  },
        };
        
        eFileFormat format = kFileFormatUnknown;
        for (size_t i = 0; kFourccMap[i].head; i++) {
            const String head = data->rs(strlen(kFourccMap[i].head));
            if (kFourccMap[i].ext) {
                if (kFourccMap[i].skip) data->skipBytes(kFourccMap[i].skip);
                const String ext = data->rs(strlen(kFourccMap[i].ext));
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
            
            data->resetBytes();
        }
        
        // put buffer back to its begin
        if (format != kFileFormatUnknown) return format;
    }

    // formats with lower score by scanning header
    struct {
        int (*scanner)(const sp<ABuffer>&);
        eFileFormat format;
    } kScanners[] = {
        { IsWaveFile,           kFileFormatWave     },
        { IsMp4File,            kFileFormatMp4      },
        { EBML::IsMatroskaFile, kFileFormatMkv      },
        { IsAviFile,            kFileFormatAvi      },
        { IsMp3File,            kFileFormatMp3      },  // this one should locate at end
        { NULL,                 kFileFormatUnknown  }
    };

    eFileFormat format = kFileFormatUnknown;
    for (size_t i = 0; kScanners[i].scanner; i++) {
        // reset buffer to its begin
        data->resetBytes();
        int c = kScanners[i].scanner(data);
        DEBUG("%4s, score = %d", (const char *)&kScanners[i].format, c);
        if (c > score) {
            score = c;
            format = kScanners[i].format;

            if (score >= 100) break;
        }
    }

    return format;
}

sp<MediaFile> CreateMp3File(const sp<ABuffer>&);
sp<MediaFile> CreateMp4File(const sp<ABuffer>&);
sp<MediaFile> CreateMatroskaFile(const sp<ABuffer>&);
sp<MediaFile> CreateLibavformat(const sp<ABuffer>&);
sp<MediaFile> CreateWaveFile(const sp<ABuffer>&);
sp<MediaFile> OpenAviFile(const sp<ABuffer>&);
sp<MediaFile> MediaFile::Create(const sp<ABuffer>& buffer, const eMode mode) {
    CHECK_TRUE(mode == Read, "TODO: only support read");
    
    String env = GetEnvironmentValue("FORCE_AVFORMAT");
    bool force = env.equals("1") || env.lower().equals("yes");
    
    sp<ABuffer> head = buffer->readBytes(kScanLength);
    buffer->skipBytes(-head->size());   // reset our buffer read pointer
    
    // skip id3v2, id3v2 is bad for file format detection
    if (ID3::SkipID3v2(head) == kMediaNoError) {
        head = head->readBytes(head->size());
    }
    
    const eFileFormat format = GetFormat(head);
    if (format == kFileFormatUnknown) return NULL;
    
    switch (format) {
        case kFileFormatWave:
            return force ? CreateLibavformat(buffer) : CreateWaveFile(buffer);
        case kFileFormatMp3:
            return force ? CreateLibavformat(buffer) : CreateMp3File(buffer);
        case kFileFormatMp4:
            return force ? CreateLibavformat(buffer) : CreateMp4File(buffer);
        case kFileFormatMkv:
            return force ? CreateLibavformat(buffer) : CreateMatroskaFile(buffer);
        case kFileFormatAvi:
            return force ? CreateLibavformat(buffer) : OpenAviFile(buffer);
        default:
            return CreateLibavformat(buffer);
    }
}

__END_NAMESPACE_MPX
