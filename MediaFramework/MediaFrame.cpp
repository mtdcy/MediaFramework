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

__END_DECLS

__BEGIN_NAMESPACE_MFWK

String GetMediaBufferListString(const MediaBufferList& list) {
    String line = String::format("MediaBufferList %zu planes", list.count);
    for (UInt32 i = 0; i < list.count; ++i) {
        line += String::format(", [%zu] %zu bytes @ %p", i, list.buffers[i].size, list.buffers[i].data);
    }
    return line;
}

#define NB_PLANES   (8)
struct FullPlaneMediaFrame : public MediaFrame {
    MediaBuffer     extend_planes[NB_PLANES];   // placeholder
    sp<Buffer>      underlyingBuffer;
    
    FullPlaneMediaFrame(sp<Buffer>& buffer) : MediaFrame(), underlyingBuffer(buffer) {
        // pollute only the first plane
        planes.count                = 1;
        planes.buffers[0].capacity  = underlyingBuffer->capacity();
        planes.buffers[0].size      = underlyingBuffer->size();
        planes.buffers[0].data      = (UInt8 *)underlyingBuffer->data();
    }
};

MediaFrame::MediaFrame() : SharedObject(), id(0),
timecode(kMediaTimeInvalid), duration(kMediaTimeInvalid),
format(0), opaque(Nil) {
    
}

sp<MediaFrame> MediaFrame::Create(UInt32 n) {
    sp<Buffer> buffer = new Buffer(n);
    sp<MediaFrame> frame = new FullPlaneMediaFrame(buffer);
    return frame;
}

sp<MediaFrame> MediaFrame::Create(sp<Buffer>& buffer) {
    sp<MediaFrame> frame = new FullPlaneMediaFrame(buffer);
    return frame;
}

sp<MediaFrame> MediaFrame::Create(const AudioFormat& audio) {
    const UInt32 bytes = GetSampleFormatBytes(audio.format);
    const UInt32 total = bytes * audio.channels * audio.samples;
    sp<Buffer> buffer = new Buffer(total);
    sp<MediaFrame> frame = new FullPlaneMediaFrame(buffer);
    
    if (IsPlanarSampleFormat(audio.format)) {
        // plannar samples
        frame->planes.count                 = audio.channels;
        frame->planes.buffers[0].size       = 0;
        frame->planes.buffers[0].capacity   = bytes * audio.samples;
        UInt8 * next = frame->planes.buffers[0].data + frame->planes.buffers[0].capacity;
        for (UInt32 i = 1; i < audio.channels; ++i) {
            frame->planes.buffers[i].size       = 0;
            frame->planes.buffers[i].capacity   = frame->planes.buffers[0].capacity;
            frame->planes.buffers[i].data       = next;
            next += frame->planes.buffers[i].capacity;
        }
        frame->planes.count = audio.channels;
    }
    
    frame->audio    = audio;
    return frame;
}

sp<MediaFrame> MediaFrame::Create(const ImageFormat& image, sp<Buffer>& buffer) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(image.format);
    CHECK_NULL(desc);
    const UInt32 bytes = (image.width * image.height * desc->bpp) / 8;
    if (buffer->capacity() < bytes) {
        ERROR("bad buffer capacity, expect %zu bytes, got %zu bytes", bytes, buffer->capacity());
        return Nil;
    }
    if (buffer->size() && buffer->size() < bytes) {
        ERROR("bad buffer data, expect %zu bytes, but got %zu bytes", bytes, buffer->size());
        return Nil;
    }
    
    sp<MediaFrame> frame    = new FullPlaneMediaFrame(buffer);
    frame->video            = image;
    
    if (desc->nb_planes > 1) {
        UInt8 * next = frame->planes.buffers[0].data;
        for (UInt32 i = 0; i < desc->nb_planes; ++i) {
            const UInt32 bytes = (image.width * image.height * desc->planes[i].bpp) /
                                 (8 * desc->planes[i].hss * desc->planes[i].vss);
            
            frame->planes.buffers[i].capacity   = bytes;
            frame->planes.buffers[i].size       = buffer->size() ? bytes : 0;
            frame->planes.buffers[i].data       = next;
            next += bytes;
        }
        frame->planes.count = desc->nb_planes;
    }
    
    DEBUG("create: %s", frame->string().c_str());
    return frame;
}

sp<MediaFrame> MediaFrame::Create(const ImageFormat& image) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(image.format);
    CHECK_NULL(desc);
    const UInt32 bytes = (image.width * image.height * desc->bpp) / 8;
    
    sp<Buffer> buffer = new Buffer(bytes);
    return MediaFrame::Create(image, buffer);
}

String MediaFrame::string() const {
    String line = String::format("[%.4s] id:%" PRIu32 ", flags:%#x, timecode:%" PRId64 "/%" PRId64 ", duration:%" PRId64 "/%" PRId64,
                                 (const Char *)&format, id, flags, timecode.value, timecode.scale, duration.value, duration.scale);
    for (UInt32 i = 0; i < planes.count; ++i) {
        line += String::format(", plane[%zu]: %zu bytes @ %p", i, planes.buffers[i].size, planes.buffers[i].data);
    }
    return line;
}

void MediaFrame::onFirstRetain() {
    
}

void MediaFrame::onLastRetain() {
    
}

sp<ABuffer> MediaFrame::readPlane(UInt32 index) const {
    CHECK_LT(index, planes.count);
    if (planes.buffers[index].data == Nil) return Nil;
    return new Buffer((const Char *)planes.buffers[index].data, planes.buffers[index].size);
}

String GetAudioFormatString(const AudioFormat& a) {
    return String::format("audio %.4s: ch %d, freq %d, samples %d",
                          (const Char *)&a.format,
                          a.channels,
                          a.freq,
                          a.samples);
}

String GetPixelFormatString(const ePixelFormat& pixel) {
    const PixelDescriptor * desc = GetPixelFormatDescriptor(pixel);
    if (desc == Nil) return "unknown pixel";
    
    String line = String::format("pixel %s: [%zu planes %zu bpp]",
                                 desc->name, desc->nb_planes, desc->bpp);
    return line;
}

String GetImageFormatString(const ImageFormat& image) {
    String line = GetPixelFormatString(image.format);
    line += String::format(" [%d x %d] [%d, %d, %d, %d] [%.4s]",
                           image.width, image.height,
                           image.rect.x, image.rect.y, image.rect.w, image.rect.h,
                           (const Char *)&image.matrix);
    return line;
}

String GetImageFrameString(const sp<MediaFrame>& frame) {
    String line = GetImageFormatString(frame->video);
    for (UInt32 i = 0; i < frame->planes.count; ++i) {
        if (frame->planes.buffers[i].data == Nil) break;
        line += String::format(" %zu@[%zu@%p]", i,
                               frame->planes.buffers[i].size, frame->planes.buffers[i].data);
    }
    line += String::format(", timecode %" PRId64 "/%" PRId64,
                           frame->timecode.value, frame->timecode.scale);
    return line;
}

__END_NAMESPACE_MFWK
