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
#include <MediaFramework/MediaDefs.h>
#include <MediaFramework/tags/id3/ID3.h>

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

static MediaFile::eFormat GetFormat(sp<Content>& pipe) {
    int score = 0;
    
    sp<Buffer> header = pipe->read(kCommonHeadLength);
    if (header == 0) {
        ERROR("content size is too small");
        return MediaFile::Invalid;
    }

    // skip id3v2
    if (!header->compare("ID3")) {
        ssize_t id3Len = ID3::ID3v2::isID3v2(*header);
        if (id3Len < 0) {
            ERROR("invalid id3v2 header.");
        } else {
            INFO("id3 len = %lu", id3Len);

            if (pipe->length() < 10 + id3Len + 32) {
                return MediaFile::Invalid;
            }

            pipe->skip(10 + id3Len - kCommonHeadLength);

            header = pipe->read(kCommonHeadLength);
            if (header == 0) {
                ERROR("not enough data after skip id3v2");
                return MediaFile::Invalid;
            }
        }
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
            MediaFile::eFormat  format;
        } kFourccMap[] = {
            {"fLaC",    0,  NULL,       MediaFile::Flac     },
            {"RIFF",    4,  "WAVE",     MediaFile::Wave     },
            {"RIFF",    4,  "AVI ",     MediaFile::Avi      },
            {"RIFF",    4,  "AVIX",     MediaFile::Avi      },
            {"RIFF",    4,  "AVI\x19",  MediaFile::Avi      },
            {"RIFF",    4,  "AMV ",     MediaFile::Avi      },
            // END OF LIST
            {NULL,      0,  NULL,       MediaFile::Invalid  },
        };
        
        MediaFile::eFormat format = MediaFile::Invalid;
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
        
        if (format != MediaFile::Invalid) {
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
        MediaFile::eFormat format;
    } kScanners[] = {
        { IsMp4File,    MediaFile::Mp4      },
        { scanMatroska, MediaFile::Mkv      },
        { scanMP3,      MediaFile::Mp3      },
        { NULL,         MediaFile::Invalid  }
    };

    MediaFile::eFormat format = MediaFile::Invalid;
    for (size_t i = 0; kScanners[i].scanner; i++) {
        int c = kScanners[i].scanner(header);
        DEBUG("%#x, score = %d", kScanners[i].format, c);
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

int scanMatroska(const sp<Buffer>& data) {
    // refer to:
    // 1. http://www.matroska.org/technical/specs/index.html
    // 2. ffmpeg::libavformat::matroskadec.c
    int score = 0;
    if (!data->compare("\x1A\x45\xDF\xA3", 4)) {
        DEBUG("Media::File::MKV");
        score = 20;

        BitReader br(data->data(), data->size());
        br.skipBytes(4);   // skip EBML ID

        uint32_t headerLength   = br.r8();
        size_t lengthBytes = __builtin_clz(headerLength) - 24; // excluding the first 24 bits

        headerLength &= MASK(8-lengthBytes-1);
        while (lengthBytes--) {
            headerLength = (headerLength << 8) | br.r8();
        }

        DEBUG("headerLength %d", headerLength);

        int done = 0;
        while (score < 100 && !done && br.remains() >= 3 * 8) {
            uint16_t Id     = br.rb16();   // EBML ID
            uint32_t size   = br.r8();
            size_t bytes    = __builtin_clz(size) - 24;
            size            &= MASK(8-bytes-1);

            if (br.remains() < size * 8) break;

            while (bytes--) size = (size << 8) | br.r8();

            DEBUG("EMBL ID %#x size %d", Id, size);

            switch (Id) {
                case 0x4282:    // DocType
                    {
                        String docType = br.readS(size);
                        if (docType.startsWith("matroska")) {
                            DEBUG("scanMatroska found DocType matroska");
                            score = 100;
                        } else if (docType.startsWith("webm")) {
                            DEBUG("scanMatroska found DocType webm");
                            score = 100;
                        } else {
                            DEBUG("unknown docType %s", docType.c_str());
                            score += 10;
                        }
                    } break;
                case 0x4286:    // EBMLVersion
                case 0x42F7:    // EBMLReadVersion
                case 0x42F2:    // EBMLMaxIDLength
                case 0x42F3:    // EBMLMaxSizeLength
                case 0x4287:    // DocTypeVersion
                    br.skipBytes(size);
                    score += 10;
                    break;
                default:
                    done = 1;
                    ERROR("unknown ebml id %" PRIx16, Id);
                    break;
            } // switch
        } // while
    } // if
    return score;
}

sp<MediaFile> CreateMp3File(sp<Content>& pipe);
sp<MediaFile> CreateMp4File(sp<Content>& pipe);
sp<MediaFile> CreateMatroskaFile(sp<Content>& pipe);
sp<MediaFile> CreateLibavformat(sp<Content>& pipe);

sp<MediaFile> MediaFile::Create(sp<Content>& pipe, const eMode mode) {
    CHECK_TRUE(mode == Read, "TODO: only support read");
    
    const MediaFile::eFormat format = GetFormat(pipe);
    switch (format) {
#if 0
        case MediaFile::Mp3:
            return CreateMp3File(pipe);
        case MediaFile::Mp4:
            return CreateMp4File(pipe);
        case MediaFile::Mkv:
            return CreateMatroskaFile(pipe);
#endif
        default:
            return CreateLibavformat(pipe);
    }
}

__END_NAMESPACE_MPX
