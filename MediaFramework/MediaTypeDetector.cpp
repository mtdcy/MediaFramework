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


// File:    MediaTypeDetector.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#define LOG_TAG "MTD"
#define LOG_NDEBUG 0

#include "MediaFile.h"
#include <MediaFramework/MediaDefs.h>
#include <MediaFramework/tags/id3/ID3.h>

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a > b ? b : a)
#define MASK(n) ((1<<(n))-1)

__BEGIN_NAMESPACE_MPX

const static size_t kCommonHeadLength = 32;
const static size_t kScanLength = 8 * 1024ll;

// return score
static int scanMP3(const sp<Buffer>& data);
static int scanAAC(const sp<Buffer>& data);
static int scanMatroska(const sp<Buffer>& data);
static int scanMP4(const sp<Buffer>& data);

eFileFormat MediaFormatDetect(const String& url) {
    sp<Content> pipe = Content::Create(url);
    if (pipe == 0) return kFileFormatUnknown;
    return MediaFormatDetect(*pipe);
}

eFileFormat MediaFormatDetect(Content& pipe) {
    int score = 0;

    sp<Buffer> header = pipe.read(kCommonHeadLength);
    if (header == 0) {
        ERROR("content size is too small");
        return kFileFormatUnknown;
    }

    // skip id3v2
    if (!header->compare("ID3")) {
        ssize_t id3Len = ID3::ID3v2::isID3v2(*header);
        if (id3Len < 0) {
            ERROR("invalid id3v2 header.");
        } else {
            INFO("id3 len = %lu", id3Len);

            if (pipe.length() < 10 + id3Len + 32) {
                return kFileFormatUnknown;
            }

            pipe.skip(10 + id3Len - kCommonHeadLength);

            header = pipe.read(kCommonHeadLength);
            if (header == 0) {
                ERROR("not enough data after skip id3v2");
                return kFileFormatUnknown;
            }
        }
    }

    const int64_t startPos = pipe.tell() - kCommonHeadLength;
    DEBUG("startPos = %" PRId64, startPos);

    BitReader br(header->data(), header->size());

    // formats with 100 score by check fourcc
    {
        String fourcc = br.readS(4);

        static struct {
            const char *fourcc;
            eFileFormat   fileType;
        } kFourccMap[] = {
            {"fLaC",        kFileFormatFlac    },
            {NULL,          kFileFormatUnknown },
        };

        for (size_t i = 0; kFourccMap[i].fourcc; i++) {
            if (fourcc.startsWith(kFourccMap[i].fourcc)) {
                DEBUG("%s", kFourccMap[i].fileType);
                return kFourccMap[i].fileType;
            }
        }

        // formats need check fourcc and extra values
        if (fourcc == "RIFF") {
            // RIFF
            br.skipBytes(4);
            String sub = br.readS(4);
            if (sub == "WAVE") {
                DEBUG("Media::File::WAVE");
                return kFileFormatWave;
            } else if (sub == "AVI " ||
                    sub == "AVIX" ||
                    sub == "AVI\x19" ||
                    sub == "AMV ") {
                DEBUG("Media::File::AVI");
                return kFileFormatAVI;
            }
        }
#if 0
        else if (fourcc == "FRM8") {
            br.skipBytes(8);
            String sub = br->readS(4);
            if (sub == "DSD ") {
                DEBUG("Media::File::DFF");
                return Media::File::DFF;
            }
        }
#endif
        br.reset();
    }

    // formats with lower score by scanning header
    header->resize(kScanLength);
    header->write(*pipe.read(kScanLength - kCommonHeadLength));

    struct {
        int (*scanner)(const sp<Buffer>& data);
        eFileFormat   fileType;
    } kScanners[] = {
        { scanMP4,      kFileFormatMP4    },
        { scanMatroska, kFileFormatMKV    },
        { scanAAC,      kFileFormatAAC    },
        { scanMP3,      kFileFormatMP3    },
        { NULL,         kFileFormatUnknown}
    };

    eFileFormat format = kFileFormatUnknown;
    for (size_t i = 0; kScanners[i].scanner; i++) {
        int c = kScanners[i].scanner(header);
        DEBUG("%#x, score = %d", kScanners[i].fileType, c);
        if (c > score) {
            score = c;
            format = kScanners[i].fileType;

            if (score >= 100) break;
        }
    }

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

int scanAAC(const sp<Buffer>& data) {
    // 1. http://wiki.multimedia.cx/index.php?title=ADTS

    BitReader br(data->data(), data->size());
    const size_t length = data->ready();
    const char *s = data->data();

    int score = 0;
    uint16_t sync = 0;
    for (size_t i = 0; i < length; i++) {
        sync = (sync << 8) | (uint8_t)s[i];

        if ((sync & 0xfff6) != 0xfff0) continue;

        DEBUG("found aac frame %#x @ %zu", sync, i);

        br.reset();
        br.skipBytes(i);
        br.skip(6);
        size_t frameSize = br.read(13);

        score = 20;
        size_t j = i + frameSize - 1;
        while (j + 1 < length && score < 100) {
            br.reset();
            br.skipBytes(j);

            if ((br.rb16() & 0xfff6) != 0xfff6) {
                DEBUG("first frame is invalid");
                score = 0;
                break;
            }

            br.skip(6);
            j += br.read(13);
        }
    }

    return score;
}

int scanMP4(const sp<Buffer>& data) {
    // 1. libavformat::mov.c::mov_probe
    // 2. MTK::MPEG4Extractor.cpp
    BitReader br(data->data(), data->size());

    int score = 0;
    uint32_t ckSize = br.rb32();
    String id = br.readS(4);
    DEBUG("id = %s", id.c_str());
    if (id == "ftyp" || id == "moov" ||
            id == "mdat" || id == "free" ||
            id == "wide") {
        DEBUG("Media::File::MP4");
        br.skipBytes(ckSize - 8);

        score = 20; 
        int done = 0;
        while (!done && br.remains() > 8 * 8 && score < 100) {
            uint32_t ckSize     = br.rb32();
            String ckType       = br.readS(4);

            DEBUG("chunk [%s] size %d", ckType.c_str(), ckSize);

            if (br.remains() < (ckSize - 8) * 8) break;

            br.skipBytes(ckSize - 8);
            if (ckType == "moov") {
                score = MAX(100, score);
                break;
            } else if (ckType == "ftyp" ||
                    ckType == "mdat" ||
                    ckType == "pnot" || /* detect movs with preview pics like ew.mov and april.mov */
                    ckType == "udat" || /* Packet Video PVAuthor adds this and a lot of more junk */
                    ckType == "wide" ||
                    ckType == "ediw" || /* xdcam files have reverted first tags */
                    ckType == "free" ||
                    ckType == "junk" ||
                    ckType == "pict") {
                score += 10;
            }
        }
    }

    return score;
}

__END_NAMESPACE_MPX
