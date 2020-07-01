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


// File:    MediaFrame.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "MediaFrame"
#define LOG_NDEBUG 0
#include "MediaTypes.h"
#include "MediaFrame.h"

__BEGIN_DECLS

struct {
    eSampleFormat   plannar;
    eSampleFormat   packed;
} kSampleFormatMap[] = {
    { kSampleFormatU8,      kSampleFormatU8Packed   },
    { kSampleFormatS16,     kSampleFormatS16Packed  },
    { kSampleFormatS32,     kSampleFormatS32Packed  },
    { kSampleFormatFLT,     kSampleFormatFLTPacked  },
    { kSampleFormatDBL,     kSampleFormatDBLPacked  },
    // END OF LIST
    { kSampleFormatUnknown, kSampleFormatUnknown    },
};

eSampleFormat GetSimilarSampleFormat(eSampleFormat format) {
    for (size_t i = 0; kSampleFormatMap[i].plannar != kSampleFormatUnknown; ++i) {
        if (format == kSampleFormatMap[i].plannar)
            return kSampleFormatMap[i].packed;
        else if (format == kSampleFormatMap[i].packed)
            return kSampleFormatMap[i].plannar;
    }
    FATAL("SHOULD NOT BE here");
    return kSampleFormatUnknown;
};

size_t GetSampleFormatBytes(eSampleFormat format) {
    switch (format) {
        case kSampleFormatU8Packed:
        case kSampleFormatU8:       return sizeof(uint8_t);
        case kSampleFormatS16Packed:
        case kSampleFormatS16:      return sizeof(int16_t);
        case kSampleFormatS32Packed:
        case kSampleFormatS32:      return sizeof(int32_t);
        case kSampleFormatFLTPacked:
        case kSampleFormatFLT:      return sizeof(float);
        case kSampleFormatDBLPacked:
        case kSampleFormatDBL:      return sizeof(double);
        case kSampleFormatUnknown:  return 0;
        default:                    break;
    }
    FATAL("FIXME");
    return 0;
}

bool IsPackedSampleFormat(eSampleFormat format) {
    switch (format) {
        case kSampleFormatU8Packed:
        case kSampleFormatS16Packed:
        case kSampleFormatS32Packed:
        case kSampleFormatFLTPacked:
        case kSampleFormatDBLPacked:
            return true;
        default:
            return false;;
    }
}
__END_DECLS

__BEGIN_NAMESPACE_MPX

#define NB_PLANES   (8)
struct FullPlaneMediaFrame : public MediaFrame {
    MediaBuffer     extend_planes[NB_PLANES -1];    // placeholder
    sp<Buffer>      underlyingBuffer;
    
    FullPlaneMediaFrame(sp<Buffer>& buffer) : MediaFrame(), underlyingBuffer(buffer) {
        // pollute only the first plane
        planes.count                = 1;
        planes.buffers[0].capacity  = underlyingBuffer->capacity();
        planes.buffers[0].size      = underlyingBuffer->size();
        planes.buffers[0].data      = (uint8_t *)underlyingBuffer->data();  // FIXME: unsafe cast
    }
};

MediaFrame::MediaFrame() : SharedObject(), id(0),
timecode(kMediaTimeInvalid), duration(kMediaTimeInvalid),
format(0), opaque(NULL) {
    
}

sp<MediaFrame> MediaFrame::Create(size_t n) {
    sp<Buffer> buffer = new Buffer(n);
    sp<MediaFrame> frame = new FullPlaneMediaFrame(buffer);
    return frame;
}

sp<MediaFrame> MediaFrame::Create(sp<Buffer>& buffer) {
    sp<MediaFrame> frame = new FullPlaneMediaFrame(buffer);
    return frame;
}

sp<MediaFrame> MediaFrame::Create(const AudioFormat& audio) {
    const size_t bytes = GetSampleFormatBytes(audio.format);
    const size_t total = bytes * audio.channels * audio.samples;
    sp<Buffer> buffer = new Buffer(total);
    sp<MediaFrame> frame = new FullPlaneMediaFrame(buffer);
    
    if (!IsPackedSampleFormat(audio.format)) {
        // plannar samples
        frame->planes.buffers[0].size = bytes * audio.samples;
        uint8_t * next = frame->planes.buffers[0].data + frame->planes.buffers[0].size;
        for (size_t i = 1; i < audio.channels; ++i) {
            frame->planes.buffers[i].size   = frame->planes.buffers[0].size;
            frame->planes.buffers[i].data   = next;
            next += frame->planes.buffers[i].size;
        }
        frame->planes.count = audio.channels;
    }
    
    frame->audio    = audio;
    return frame;
}

sp<MediaFrame> MediaFrame::Create(const ImageFormat& image, sp<Buffer>& buffer) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(image.format);
    CHECK_NULL(desc);
    const size_t bytes = (image.width * image.height * desc->bpp) / 8;
    if (buffer->capacity() < bytes) return NULL;
    
    sp<MediaFrame> frame = new FullPlaneMediaFrame(buffer);
    
    if (desc->planes > 1) {
        frame->planes.buffers[0].size = (image.width * image.height * desc->plane[0].bpp) /
        (8 * desc->plane[0].hss * desc->plane[0].vss);
        uint8_t * next = frame->planes.buffers[0].data + frame->planes.buffers[0].size;
        for (size_t i = 1; i < desc->planes; ++i) {
            const size_t bytes = (image.width * image.height * desc->plane[i].bpp) /
                                 (8 * desc->plane[i].hss * desc->plane[i].vss);
            frame->planes.buffers[i].data   = next;
            frame->planes.buffers[i].size   = bytes;
            next += bytes;
        }
        frame->planes.count = desc->planes;
    }
    
    frame->video    = image;
    
    DEBUG("create: %s", frame->string().c_str());
    return frame;
}

sp<MediaFrame> MediaFrame::Create(const ImageFormat& image) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(image.format);
    CHECK_NULL(desc);
    const size_t bytes = (image.width * image.height * desc->bpp) / 8;
    
    sp<Buffer> buffer = new Buffer(bytes);
    return MediaFrame::Create(image, buffer);
}

String MediaFrame::string() const {
    String line = String::format("[%.4s] id:%" PRIu32 ", flags:%#x, timecode:%" PRId64 "/%" PRId64 ", duration:%" PRId64 "/%" PRId64,
                                 (const char *)&format, id, flags, timecode.value, timecode.scale, duration.value, duration.scale);
    for (size_t i = 0; i < planes.count; ++i) {
        line += String::format(", plane[%zu]: %zu bytes @ %p", i, planes.buffers[i].size, planes.buffers[i].data);
    }
    return line;
}

sp<ABuffer> MediaFrame::readPlane(size_t index) const {
    CHECK_LT(index, planes.count);
    if (planes.buffers[index].data == NULL) return NULL;
    return new Buffer((const char *)planes.buffers[index].data, planes.buffers[index].size);
}

size_t GetImageFormatPlaneLength(const ImageFormat& image, size_t i) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(image.format);
    CHECK_NULL(desc);
    return (image.width * image.height * desc->plane[i].bpp) /
            (8 * desc->plane[i].hss * desc->plane[i].vss);
}

size_t GetImageFormatBufferLength(const ImageFormat& image) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(image.format);
    CHECK_NULL(desc);
    return (image.width * image.height * desc->bpp) / 8;;
}

String GetAudioFormatString(const AudioFormat& a) {
    return String::format("audio %.4s: ch %d, freq %d, samples %d",
                          (const char *)&a.format,
                          a.channels,
                          a.freq,
                          a.samples);
}

String GetPixelFormatString(const ePixelFormat& pixel) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(pixel);
    if (desc == NULL) return "unknown pixel";
    
    String line = String::format("pixel %s: [%zu planes %zu bpp]",
                                 desc->name, desc->planes, desc->bpp);
    return line;
}

String GetImageFormatString(const ImageFormat& image) {
    String line = GetPixelFormatString(image.format);
    line += String::format(" [%d x %d] [%d, %d, %d, %d]",
                           image.width, image.height,
                           image.rect.x, image.rect.y, image.rect.w, image.rect.h);
    return line;
}

String GetImageFrameString(const sp<MediaFrame>& frame) {
    String line = GetImageFormatString(frame->video);
    for (size_t i = 0; i < frame->planes.count; ++i) {
        if (frame->planes.buffers[i].data == NULL) break;
        line += String::format(" %zu@[%zu@%p]", i,
                               frame->planes.buffers[i].size, frame->planes.buffers[i].data);
    }
    line += String::format(", timecode %" PRId64 "/%" PRId64,
                           frame->timecode.value, frame->timecode.scale);
    return line;
}

__END_NAMESPACE_MPX
