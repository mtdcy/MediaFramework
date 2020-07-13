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


// File:    videotoolbox/Decoder.cpp
// Author:  mtdcy.chen
// Changes: 
//          1. 20160701     initial version
//

#define LOG_TAG "mac.VLD"
//#define LOG_NDEBUG 0
#include <ABE/ABE.h>

#include "MediaTypes.h"
#include "MediaDevice.h"

#include <VideoToolbox/VideoToolbox.h>

#include "mpeg4/Systems.h"

//#define VERBOSE
#ifdef VERBOSE
#define DEBUGV(fmt, ...) DEBUG(fmt, ##__VA_ARGS__)
#else
#define DEBUGV(fmt, ...) do {} while(0)
#endif

// https://www.objc.io/issues/23-video/videotoolbox/
// http://www.enkichen.com/2018/03/24/videotoolbox/?utm_source=tuicool&utm_medium=referral

__BEGIN_NAMESPACE_MPX

struct {
    eVideoCodec         a;
    CMVideoCodecType    b;
} kCodecMap[] = {
    {kVideoCodecH263,       kCMVideoCodecType_H263},
    {kVideoCodecH264,       kCMVideoCodecType_H264},
    {kVideoCodecHEVC,       kCMVideoCodecType_HEVC},
    {kVideoCodecMPEG4,      kCMVideoCodecType_MPEG4Video},
    // END OF LIST
    {kVideoCodecUnknown,    0},
};

static FORCE_INLINE CMVideoCodecType get_cm_codec_type(eVideoCodec a) {
    for (UInt32 i = 0; kCodecMap[i].a != kVideoCodecUnknown; ++i) {
        if (kCodecMap[i].a == a)
            return kCodecMap[i].b;
    }
    return 0;
}

static FORCE_INLINE eVideoCodec get_codec_format(CMVideoCodecType b) {
    for (UInt32 i = 0; kCodecMap[i].a != kVideoCodecUnknown; ++i) {
        if (kCodecMap[i].b == b)
            return kCodecMap[i].a;
    }
    FATAL("FIXME");
    return kVideoCodecUnknown;
}

struct {
    ePixelFormat    a;
    OSType          b;
} kPixelMap[] = {
    {kPixelFormat420YpCbCrPlanar,       kCVPixelFormatType_420YpCbCr8Planar},
    //{kPixelFormatUYVY422,             kCVPixelFormatType_422YpCbCr8},
    {kPixelFormat422YpCbCr,             kCVPixelFormatType_422YpCbCr8_yuvs},
#ifdef kCFCoreFoundationVersionNumber10_7
    {kPixelFormat420YpCbCrSemiPlanar,   kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange},
#endif
    // END OF LIST
    {kPixelFormatUnknown,       0},
};

static FORCE_INLINE ePixelFormat get_pix_format(OSType b) {
    for (UInt32 i = 0; kPixelMap[i].a != kPixelFormatUnknown; ++i) {
        if (kPixelMap[i].b == b) return kPixelMap[i].a;
    }
    FATAL("FIXME");
    return kPixelFormatUnknown;
}

static FORCE_INLINE OSType get_cv_pix_format(ePixelFormat a) {
    for (UInt32 i = 0; kPixelMap[i].a != kPixelFormatUnknown; ++i) {
        if (kPixelMap[i].a == a) return kPixelMap[i].b;
    }
    ERROR("FIXME: add map item for %d", a);
    return 0;
}

// for store unorderred image buffers and sort them
struct VTMediaFrame : public MediaFrame {
    MediaBuffer     extended_buffers[3];    // placeholder
    
    FORCE_INLINE VTMediaFrame(CVPixelBufferRef pixbuf, const MediaTime& pts, const MediaTime& _duration) : MediaFrame() {
        planes.buffers[0].data  = Nil;
        timecode        = pts;
        duration        = _duration;
        video.format    = kPixelFormatVideoToolbox;
        video.width     = CVPixelBufferGetWidth(pixbuf);
        video.height    = CVPixelBufferGetHeight(pixbuf);
        video.rect.x    = 0;
        video.rect.y    = 0;
        video.rect.w    = video.width;
        video.rect.h    = video.height;
        opaque = CVPixelBufferRetain(pixbuf);
    }

    FORCE_INLINE ~VTMediaFrame() {
        CVPixelBufferRelease((CVPixelBufferRef)opaque);
    }
    
    virtual sp<ABuffer> readPlane(UInt32 index) const {
        CVPixelBufferRef pixbuf = (CVPixelBufferRef)opaque;
        sp<Buffer> plane;
        CVReturn err = CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
        if (err != kCVReturnSuccess) {
            ERROR("Error locking the pixel buffer");
            return Nil;
        }
        
        if (CVPixelBufferIsPlanar(pixbuf)) {
            UInt32 n = CVPixelBufferGetPlaneCount(pixbuf);
            CHECK_LT(index, n);
            plane = new Buffer((const Char *)CVPixelBufferGetBaseAddressOfPlane(pixbuf, index),
                      CVPixelBufferGetBytesPerRowOfPlane(pixbuf, index) *
                      CVPixelBufferGetHeightOfPlane(pixbuf, index));
        } else {
            CHECK_EQ(index, 0);
            plane = new Buffer((const Char *)CVPixelBufferGetBaseAddress(pixbuf),
                          CVPixelBufferGetBytesPerRow(pixbuf) *
                          CVPixelBufferGetHeight(pixbuf));
        }
        CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
        return plane;
    }
};

struct VTContext : public SharedObject {
    FORCE_INLINE VTContext() : decompressionSession(Nil), formatDescription(Nil),
    mInputEOS(False) { }

    FORCE_INLINE ~VTContext() {
        if (decompressionSession) {
            VTDecompressionSessionInvalidate(decompressionSession);
            CFRelease(decompressionSession);
        }
        if (formatDescription) {
            CFRelease(formatDescription);
        }
    }

    Int32 width, height;
    ePixelFormat pixel;
    VTDecompressionSessionRef       decompressionSession;
    CMVideoFormatDescriptionRef     formatDescription;

    Mutex                           mLock;
    Bool                            mInputEOS;
    List<sp<VTMediaFrame> >         mImages;
    // when B frames output in order, only need to cache one I/P frame.
    // But if B frames out of order, then we need to cache a B frame too.
    sp<VTMediaFrame>                mUnorderImage0;     // B frame
    sp<VTMediaFrame>                mUnorderImage1;     // I/P frame
};

static FORCE_INLINE void OutputCallback(void *decompressionOutputRefCon,
        void *sourceFrameRefCon,
        OSStatus status,
        VTDecodeInfoFlags infoFlags,
        CVImageBufferRef imageBuffer,
        CMTime presentationTimeStamp,
        CMTime presentationDuration) {

    CHECK_NULL(decompressionOutputRefCon);
    CHECK_NULL(sourceFrameRefCon);  // strong ref to the packet
    MediaFrame *packet = (MediaFrame*)sourceFrameRefCon;
    packet->ReleaseObject();
    DEBUG("status %d, infoFlags %#x, imageBuffer %p, presentationTimeStamp %.3f(s)/%.3f(s)",
            status, infoFlags, imageBuffer,
            CMTimeGetSeconds(presentationTimeStamp),
            CMTimeGetSeconds(presentationDuration));

    if (status == 0 && imageBuffer == Nil) {
        DEBUG("decoder output nothing, reference frame ?");
        return;
    }
    
    if (status) {
        ERROR("decode frame failed, st = %d", status);
        return;
    }

    VTContext *vtc = (VTContext*)decompressionOutputRefCon;
    AutoLock _l(vtc->mLock);

    CHECK_TRUE(CMTIME_IS_VALID(presentationTimeStamp));
    MediaTime pts = MediaTime( presentationTimeStamp.value, presentationTimeStamp.timescale );

    MediaTime duration;
    if (CMTIME_IS_INVALID(presentationDuration)) {
        duration = kMediaTimeInvalid;
    } else {
        duration = MediaTime( presentationDuration.value, presentationDuration.timescale );
    }
    sp<VTMediaFrame> frame = new VTMediaFrame(imageBuffer, pts, duration);

    // fix the width & height
    frame->video.rect.w  = vtc->width;
    frame->video.rect.h  = vtc->height;

    // vt feed on packet in dts order and output is also in dts order
    // we have to reorder frames in pts order
    if (vtc->mUnorderImage1.isNil()) {
        vtc->mUnorderImage1 = frame;
    } else if (frame->timecode > vtc->mUnorderImage1->timecode) {
        // a new I/P frame show up
        if (!vtc->mUnorderImage0.isNil())
            vtc->mImages.push(vtc->mUnorderImage0);
        vtc->mImages.push(vtc->mUnorderImage1);
        vtc->mUnorderImage0.clear();
        vtc->mUnorderImage1 = frame;
    } else {
        if (vtc->mUnorderImage0.isNil()) {
            vtc->mUnorderImage0 = frame;
        } else if (frame->timecode > vtc->mUnorderImage0->timecode) {
            // a new B frame show up
            vtc->mImages.push(vtc->mUnorderImage0);
            vtc->mUnorderImage0 = frame;
        } else {
            vtc->mImages.push(frame);
        }
    }
    
    if (vtc->mInputEOS) {
        if (!vtc->mUnorderImage0.isNil())
            vtc->mImages.push(vtc->mUnorderImage0);
        if (!vtc->mUnorderImage1.isNil())
            vtc->mImages.push(vtc->mUnorderImage1);
        vtc->mUnorderImage0.clear();
        vtc->mUnorderImage1.clear();
    }
}

static FORCE_INLINE CFDictionaryRef setupFormatDescriptionExtension(const sp<Message>& formats,
        CMVideoCodecType cm_codec_type) {
    CFMutableDictionaryRef atoms = CFDictionaryCreateMutable(
            kCFAllocatorDefault,
            1,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);

    switch (cm_codec_type) {
        case kCMVideoCodecType_H264:
            if (formats->contains(kKeyavcC)) {
                sp<Buffer> avcC = formats->findObject(kKeyavcC);
                CFDataRef data = CFDataCreate(kCFAllocatorDefault, (const UInt8*)avcC->data(), avcC->size());
                CFDictionarySetValue(atoms, CFSTR("avcC"), data);
                CFRelease(data);
            } else {
                ERROR("missing avcC");
            }
            break;
        case kCMVideoCodecType_HEVC:
            if (formats->contains(kKeyhvcC)) {
                sp<Buffer> hvcC = formats->findObject(kKeyhvcC);
                CFDataRef data = CFDataCreate(kCFAllocatorDefault, (const UInt8*)hvcC->data(), hvcC->size());
                CFDictionarySetValue(atoms, CFSTR("hvcC"), data);
                CFRelease(data);
            } else {
                ERROR("missing hvcC");
            }
            break;
        case kCMVideoCodecType_MPEG4Video:
            if (formats->contains(kKeyESDS)) {
                sp<Buffer> esds = formats->findObject(kKeyESDS);
                sp<MPEG4::ESDescriptor> esd = MPEG4::ReadESDS(esds);
                if (!esd.isNil()) {
                    CFDataRef data = CFDataCreate(kCFAllocatorDefault, (const UInt8*)esds->data(), esds->size());
                    CFDictionarySetValue(atoms, CFSTR("esds"), data);
                    CFRelease(data);
                } else {
                    ERROR("bad esds");
                }
            } else {
                ERROR("missing esds");
            }
            break;
        default:
            FATAL("FIXME");
            break;
    }

    return atoms;
}

static FORCE_INLINE CFDictionaryRef setupImageBufferAttributes(Int32 width, Int32 height) {

    const OSType cv_pix_fmt = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;  // nv12
    
    CFNumberRef w   = CFNumberCreate(kCFAllocatorDefault,
            kCFNumberSInt32Type,
            &width);
    CFNumberRef h   = CFNumberCreate(kCFAllocatorDefault,
            kCFNumberSInt32Type,
            &height);
    CFNumberRef fmt = CFNumberCreate(kCFAllocatorDefault,
            kCFNumberSInt32Type,
            &cv_pix_fmt);

    CFMutableDictionaryRef attr = CFDictionaryCreateMutable(
            kCFAllocatorDefault,
            4,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);

    CFMutableDictionaryRef surfaceProp = CFDictionaryCreateMutable(
            kCFAllocatorDefault,
            0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);


    CFDictionarySetValue(attr, kCVPixelBufferPixelFormatTypeKey, fmt);
    CFDictionarySetValue(attr, kCVPixelBufferIOSurfacePropertiesKey, surfaceProp);
    CFDictionarySetValue(attr, kCVPixelBufferWidthKey, w);
    CFDictionarySetValue(attr, kCVPixelBufferHeightKey, h);
    // https://ffmpeg.org/pipermail/ffmpeg-devel/2017-December/222481.html
#if TARGET_OS_IPHONE
    CFDictionarySetValue(attr, kCVPixelBufferOpenGLESCompatibilityKey, kCFBooleanTrue);
#else
    //kCVPixelBufferOpenGLCompatibilityKey
    CFDictionarySetValue(attr, kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey, kCFBooleanTrue);
#endif

    CFRelease(surfaceProp);
    CFRelease(fmt);
    CFRelease(w);
    CFRelease(h);

    return attr;
}

// https://github.com/jyavenard/DecodeTest/blob/master/DecodeTest/VTDecoder.mm
static FORCE_INLINE sp<VTContext> createSession(const sp<Message>& formats, const sp<Message>& options, MediaError *err) {
    sp<VTContext> vtc = new VTContext;

    eVideoCodec codec = (eVideoCodec)formats->findInt32(kKeyFormat);
    vtc->width  = formats->findInt32(kKeyWidth);
    vtc->height = formats->findInt32(kKeyHeight);
    vtc->pixel  = kPixelFormatVideoToolbox;

    CMVideoCodecType cm_codec_type = get_cm_codec_type(codec);
    if (cm_codec_type == 0) {
        ERROR("unsupported codec");
        *err = kMediaErrorNotSupported;
        return Nil;
    }

    // setup video decoder specification
    //CFDictionaryRef videoDecoderSpecification;
    CFMutableDictionaryRef videoDecoderSpecification = CFDictionaryCreateMutable(
            kCFAllocatorDefault,
            1,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);

    // enable hwaccel
    CFDictionarySetValue(videoDecoderSpecification,
            cm_codec_type == kCMVideoCodecType_HEVC ?
            kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder :
            kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder,
            kCFBooleanTrue);

    // esds ? avcC ? hvcC ?
    CFDictionaryRef atoms = setupFormatDescriptionExtension(
            formats,
            cm_codec_type);
    CFDictionarySetValue(videoDecoderSpecification,
            kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms,
            atoms);
    CFRelease(atoms);

    // setup video format description
    OSStatus status = CMVideoFormatDescriptionCreate(
            kCFAllocatorDefault,
            cm_codec_type,
            vtc->width,
            vtc->height,
            videoDecoderSpecification,
            &vtc->formatDescription);
    if (status) {
        ERROR("create video format desc failed");
        CFRelease(videoDecoderSpecification);
        return Nil;
    }

    CFDictionaryRef destinationImageBufferAttributes = setupImageBufferAttributes(
            vtc->width,
            vtc->height);

    VTDecompressionOutputCallbackRecord callback;
    callback.decompressionOutputCallback = OutputCallback;
    callback.decompressionOutputRefCon = vtc.get();
    status = VTDecompressionSessionCreate(
            Nil,
            vtc->formatDescription,
            (CFDictionaryRef)videoDecoderSpecification,
            destinationImageBufferAttributes,
            &callback,
            &vtc->decompressionSession);

    CFRelease(videoDecoderSpecification);
    CFRelease(destinationImageBufferAttributes);

    switch (status) {
        case kVTVideoDecoderNotAvailableNowErr:
            ERROR("VideoToolbox session not available.");
            break;
        case kVTVideoDecoderUnsupportedDataFormatErr:
            ERROR("VideoToolbox does not support this format.");
            break;
        case kVTCouldNotFindVideoDecoderErr:
            ERROR("VideoToolbox decoder for this format not found.");
            break;
        case kVTVideoDecoderMalfunctionErr:
            ERROR("VideoToolbox malfunction.");
            break;
        case kVTVideoDecoderBadDataErr:
            ERROR("VideoToolbox reported invalid data.");
            break;
        case 0:
            break;
        default:
            ERROR("Unknown VideoToolbox session creation error %d", (Int)status);
            break;
    }

    return status ? Nil : vtc;
}

static FORCE_INLINE CMSampleBufferRef createCMSampleBuffer(sp<VTContext>& vtc,
        const sp<MediaFrame>& packet) {
    DEBUG("CMBlockBufferGetTypeID: %#x", CMBlockBufferGetTypeID());
    CMBlockBufferRef  blockBuffer = Nil;
    CMSampleBufferRef sampleBuffer = Nil;

    OSStatus status = CMBlockBufferCreateWithMemoryBlock(
            kCFAllocatorDefault,        // structureAllocator -> default allocator
            (Char*)packet->planes.buffers[0].data,        // memoryBlock
            packet->planes.buffers[0].size,               // blockLength
            kCFAllocatorNull,           // blockAllocator -> no deallocation
            Nil,                       // customBlockSource
            0,                          // offsetToData
            packet->planes.buffers[0].size,               // dataLength
            0,                          // flags
            &blockBuffer);

    if (kCMBlockBufferNoErr != status) {
        ERROR("CMBlockBufferCreateWithMemoryBlock failed, error = %d", status);
        return Nil;
    }
    CHECK_NULL(blockBuffer);

    CMSampleTimingInfo timingInfo[1];
    CHECK_TRUE(packet->timecode != kMediaTimeInvalid);
    timingInfo[0].decodeTimeStamp = kCMTimeInvalid;
    timingInfo[0].presentationTimeStamp = CMTimeMake(packet->timecode.value, packet->timecode.scale);
#if 0
    if (packet->pts != kMediaTimeInvalid) {
        timingInfo[0].presentationTimeStamp = CMTimeMake(packet->pts.value, packet->pts.timescale);
    } else {
        // assume decoding order = presentation order
        timingInfo[0].presentationTimeStamp = timingInfo[0].decodeTimeStamp;
    }
#endif
    if (packet->duration != kMediaTimeInvalid) {
        timingInfo[0].duration = CMTimeMake(packet->duration.value, packet->duration.scale);
    } else {
        timingInfo[0].duration = kCMTimeInvalid;
    }

    status = CMSampleBufferCreate(
            kCFAllocatorDefault,    // allocator
            blockBuffer,            // dataBuffer
            TRUE,                   // dataReady
            0,                      // makeDataReadyCallback
            0,                      // makeDataReadyRefcon
            vtc->formatDescription, // formatDescription
            1,                      // numSamples
            1,                      // numSampleTimingEntries
            &timingInfo[0],         // sampleTimingArray
            0,                      // numSampleSizeEntries
            Nil,                   // sampleSizeArray
            &sampleBuffer);

    CFRelease(blockBuffer);

    if (status) {
        ERROR("CMSampleBufferCreate faled, error = %#x", status);
        return Nil;
    } else {
        return sampleBuffer;
    }
}

sp<MediaFrame> readVideoToolboxFrame(CVPixelBufferRef pixbuf) {
    sp<MediaFrame> frame;

    CVReturn err = CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
    if (err != kCVReturnSuccess) {
        ERROR("Error locking the pixel buffer");
        return Nil;
    }

    size_t left, right, top, bottom;
    CVPixelBufferGetExtendedPixels(pixbuf, &left, &right, &top, &bottom);
    DEBUGV("paddings %zu %zu %zu %zu", left, right, top, bottom);
    DEBUGV("CVPixelBufferGetBytesPerRow %zu", CVPixelBufferGetBytesPerRow(pixbuf));

    if (CVPixelBufferIsPlanar(pixbuf)) {
        ImageFormat format;
        format.format = get_pix_format(CVPixelBufferGetPixelFormatType(pixbuf));
        format.width = CVPixelBufferGetBytesPerRowOfPlane(pixbuf, 0);
        format.height = CVPixelBufferGetHeight(pixbuf);
        frame = MediaFrame::Create(format);
        DEBUGV("CVPixelBufferGetWidth %zu", CVPixelBufferGetWidth(pixbuf));
        DEBUGV("CVPixelBufferGetHeight %zu", CVPixelBufferGetHeight(pixbuf));

        // as we have to copy the data, copy to continueslly space
        DEBUGV("CVPixelBufferGetDataSize %zu", CVPixelBufferGetDataSize(pixbuf));
        DEBUGV("CVPixelBufferGetPlaneCount %zu", CVPixelBufferGetPlaneCount(pixbuf));

        UInt32 planes = CVPixelBufferGetPlaneCount(pixbuf);
        for (UInt32 i = 0; i < planes; i++) {
            DEBUGV("CVPixelBufferGetBaseAddressOfPlane %p", CVPixelBufferGetBaseAddressOfPlane(pixbuf, i));
            DEBUGV("CVPixelBufferGetBytesPerRowOfPlane %zu", CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i));
            DEBUGV("CVPixelBufferGetWidthOfPlane %zu", CVPixelBufferGetWidthOfPlane(pixbuf, i));
            DEBUGV("CVPixelBufferGetHeightOfPlane %zu", CVPixelBufferGetHeightOfPlane(pixbuf, i));

            CHECK_LE((UInt32)(CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i) *
                    CVPixelBufferGetHeightOfPlane(pixbuf, i)),
                     frame->planes.buffers[i].size);
            frame->planes.buffers[i].size = CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i) * CVPixelBufferGetHeightOfPlane(pixbuf, i);
            memcpy(frame->planes.buffers[i].data,
                    CVPixelBufferGetBaseAddressOfPlane(pixbuf, i),
                   frame->planes.buffers[i].size);
        }
    } else {
        // FIXME: is this right
        ImageFormat format;
        format.format = get_pix_format(CVPixelBufferGetPixelFormatType(pixbuf));
        format.width = CVPixelBufferGetBytesPerRow(pixbuf);
        format.height = CVPixelBufferGetHeight(pixbuf);
        frame = MediaFrame::Create(format);
        CHECK_LE((UInt32)(CVPixelBufferGetBytesPerRow(pixbuf) * CVPixelBufferGetHeight(pixbuf)), frame->planes.buffers[0].size);
        frame->planes.buffers[0].size = CVPixelBufferGetBytesPerRow(pixbuf) * CVPixelBufferGetHeight(pixbuf);
        memcpy(frame->planes.buffers[0].data,
                CVPixelBufferGetBaseAddress(pixbuf),
               frame->planes.buffers[0].size);
    }

    frame->video.rect.x     = 0;
    frame->video.rect.y     = 0;
    frame->video.rect.w     = CVPixelBufferGetWidth(pixbuf);
    frame->video.rect.h     = CVPixelBufferGetHeight(pixbuf);

    CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);

    return frame;
}

struct VideoToolboxDecoder : public MediaDevice {
    sp<VTContext>   mVTContext;

    VideoToolboxDecoder() { }
    virtual ~VideoToolboxDecoder() { }

    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyWidth, mVTContext->width);
        info->setInt32(kKeyHeight, mVTContext->height);
        info->setInt32(kKeyFormat, mVTContext->pixel);
        DEBUG(" => %s", info->string().c_str());
        return info;
    }

    virtual MediaError init(const sp<Message>& format, const sp<Message>& options) {
        INFO("create VideoToolbox for %s", format->string().c_str());

        DEBUG("VTDecompressionSessionGetTypeID: %#x", VTDecompressionSessionGetTypeID());

        MediaError err = kMediaNoError;
        mVTContext = createSession(format, options, &err);
        return mVTContext.isNil() ? kMediaErrorNotSupported : kMediaNoError;
    }

    virtual MediaError configure(const sp<Message>& options) {
        return kMediaErrorInvalidOperation;
    }

    virtual MediaError push(const sp<MediaFrame>& input) {
        if (input == Nil) {
            INFO("eos");
            VTDecompressionSessionFinishDelayedFrames(mVTContext->decompressionSession);
            // no need to wait here.
            mVTContext->mInputEOS = True;
            return kMediaNoError;
        }

        DEBUG("push %s", input->string().c_str());

        CHECK_TRUE(input->timecode != kMediaTimeInvalid);

        CMSampleBufferRef sampleBuffer = createCMSampleBuffer(mVTContext, input);

        // FIXME:
        // kVTDecodeFrame_EnableTemporalProcessing is not working as expected.
        VTDecodeFrameFlags decodeFlags = kVTDecodeFrame_EnableTemporalProcessing;
        //decodeFlags |= kVTDecodeFrame_EnableAsynchronousDecompression;
        //decodeFlags |= kVTDecodeFrame_1xRealTimePlayback;
        if (input->flags & kFrameTypeReference) {
            INFO("reference frame");
            decodeFlags |= kVTDecodeFrame_DoNotOutputFrame;
        }
        VTDecodeInfoFlags infoFlagsOut;
        OSStatus status = VTDecompressionSessionDecodeFrame(
                mVTContext->decompressionSession,   // VTDecompressionSessionRef
                sampleBuffer,                       // CMSampleBufferRef
                decodeFlags,                        // VTDecodeFrameFlags
                input.get()->RetainObject(),        // sourceFrameRefCon
                &infoFlagsOut                       // VTDecodeInfoFlags
                );
        DEBUG("decode info flag %#x", infoFlagsOut);

        CFRelease(sampleBuffer);

        if (status) {
            ERROR("VTDecompressionSessionDecodeFrame failed, error = %#x", status);
            return kMediaErrorUnknown;
        }
        return kMediaNoError;
    }

    virtual sp<MediaFrame> pull() {
        AutoLock _l(mVTContext->mLock);
        if (mVTContext->mImages.empty()) {
            if (mVTContext->mInputEOS) INFO("eos...");
            else INFO("no frames ready");
            return Nil;
        }
        
        sp<VTMediaFrame> frame = mVTContext->mImages.front();
        mVTContext->mImages.pop();
        DEBUG("pull %s", frame->string().c_str());
        return frame;
    }

    virtual MediaError reset() {
        VTDecompressionSessionFinishDelayedFrames(mVTContext->decompressionSession);
        VTDecompressionSessionWaitForAsynchronousFrames(mVTContext->decompressionSession);

        AutoLock _l(mVTContext->mLock);
        mVTContext->mImages.clear();
        mVTContext->mUnorderImage0.clear();
        mVTContext->mUnorderImage1.clear();
        return kMediaNoError;
    }
};

// FIXME: VTIsHardwareDecodeSupported is not working as expected
Bool IsVideoToolboxSupported(eVideoCodec format) {
    CMPixelFormatType cm = get_cm_codec_type(format);
    if (cm == 0) return False;
    return True;
    //return VTIsHardwareDecodeSupported(cm);
}

sp<MediaDevice> CreateVideoToolboxDecoder(const sp<Message>& formats, const sp<Message>& options) {
    sp<VideoToolboxDecoder> vt = new VideoToolboxDecoder();
    if (vt->init(formats, options) == kMediaNoError) return vt;
    return Nil;
}

__END_NAMESPACE_ABE
