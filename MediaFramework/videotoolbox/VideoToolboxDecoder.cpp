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

#define LOG_TAG "videotoolbox"
//#define LOG_NDEBUG 0
#include <ABE/ABE.h>

#include <VideoToolbox/VideoToolbox.h>

#include <MediaFramework/MediaDefs.h>
#include <MediaFramework/MediaDecoder.h>

#include "mpeg4/Systems.h"

//#define VERBOSE
#ifdef VERBOSE
#define DEBUGV(fmt, ...) DEBUG(fmt, ##__VA_ARGS__)
#else
#define DEBUGV(fmt, ...) do {} while(0)
#endif

// kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange <-> nv12
#define kPreferredPixelFormat kPixelFormatNV12

#define FORCE_DTS   0 // for testing

// https://www.objc.io/issues/23-video/videotoolbox/
// http://www.enkichen.com/2018/03/24/videotoolbox/?utm_source=tuicool&utm_medium=referral

__BEGIN_NAMESPACE_MPX

struct {
    eCodecFormat        a;
    CMVideoCodecType    b;
} kCodecMap[] = {
    {kVideoCodecFormatH263,     kCMVideoCodecType_H263},
    {kVideoCodecFormatH264,     kCMVideoCodecType_H264},
    {kVideoCodecFormatHEVC,     kCMVideoCodecType_HEVC},
    {kVideoCodecFormatMPEG4,    kCMVideoCodecType_MPEG4Video},
    // END OF LIST
    {kCodecFormatUnknown,       0},
};

static __ABE_INLINE CMVideoCodecType get_cm_codec_type(eCodecFormat a) {
    for (size_t i = 0; kCodecMap[i].a != kCodecFormatUnknown; ++i) {
        if (kCodecMap[i].a == a)
            return kCodecMap[i].b;
    }
    FATAL("FIXME");
    return 0;
}

static __ABE_INLINE eCodecFormat get_codec_format(CMVideoCodecType b) {
    for (size_t i = 0; kCodecMap[i].a != kCodecFormatUnknown; ++i) {
        if (kCodecMap[i].b == b)
            return kCodecMap[i].a;
    }
    FATAL("FIXME");
    return kCodecFormatUnknown;
}

struct {
    ePixelFormat    a;
    OSType          b;
} kPixelMap[] = {
    {kPixelFormatYUV420P,       kCVPixelFormatType_420YpCbCr8Planar},
    {kPixelFormatUYVY422,       kCVPixelFormatType_422YpCbCr8},
    {kPixelFormatYUYV422,       kCVPixelFormatType_422YpCbCr8_yuvs},
#ifdef kCFCoreFoundationVersionNumber10_7
    {kPixelFormatNV12,          kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange},
#endif
    // END OF LIST
    {kPixelFormatUnknown,       0},
};

static __ABE_INLINE ePixelFormat get_pix_format(OSType b) {
    for (size_t i = 0; kPixelMap[i].a != kPixelFormatUnknown; ++i) {
        if (kPixelMap[i].b == b) return kPixelMap[i].a;
    }
    FATAL("FIXME");
    return kPixelFormatUnknown;
}

static __ABE_INLINE OSType get_cv_pix_format(ePixelFormat a) {
    for (size_t i = 0; kPixelMap[i].a != kPixelFormatUnknown; ++i) {
        if (kPixelMap[i].a == a) return kPixelMap[i].b;
    }
    ERROR("FIXME: add map item for %d", a);
    return 0;
}

// for store unorderred image buffers and sort them
struct __ABE_HIDDEN VTMediaFrame : public MediaFrame {
    __ABE_INLINE VTMediaFrame(CVPixelBufferRef pixbuf, const MediaTime& _pts, const MediaTime& _duration) : MediaFrame() {
        planes[0].data  = NULL;
        pts         = _pts;
        duration    = _duration;
        v.format    = get_pix_format(CVPixelBufferGetPixelFormatType(pixbuf));
        v.width     = CVPixelBufferGetWidth(pixbuf);
        v.height    = CVPixelBufferGetHeight(pixbuf);
        v.rect.x    = 0;
        v.rect.y    = 0;
        v.rect.w    = v.width;
        v.rect.h    = v.height;
        opaque = CVPixelBufferRetain(pixbuf);
    }

    __ABE_INLINE VTMediaFrame(const VTMediaFrame& rhs) {
        planes[0].data  = NULL;
        pts         = rhs.pts;
        duration    = rhs.duration;
        v           = rhs.v;
        opaque      = CVPixelBufferRetain((CVPixelBufferRef)rhs.opaque);
    }

    __ABE_INLINE ~VTMediaFrame() {
        CVPixelBufferRelease((CVPixelBufferRef)opaque);
    }

    __ABE_INLINE bool operator<(const VTMediaFrame& rhs) const {
        return pts < rhs.pts;
    }
};

struct __ABE_HIDDEN VTContext : public SharedObject {
    __ABE_INLINE VTContext() : decompressionSession(NULL), formatDescription(NULL),
    mInputEOS(false), mLastFrameTime(kTimeBegin) { }

    __ABE_INLINE ~VTContext() {
        if (decompressionSession) {
            VTDecompressionSessionInvalidate(decompressionSession);
            CFRelease(decompressionSession);
        }
        if (formatDescription) {
            CFRelease(formatDescription);
        }
    }

    int32_t width, height;
    ePixelFormat pixel;
    VTDecompressionSessionRef       decompressionSession;
    CMVideoFormatDescriptionRef     formatDescription;

    Mutex                           mLock;
    bool                            mInputEOS;
    List<VTMediaFrame>              mImages;
    List<MediaTime>                 mTimestamps;

    MediaTime                       mLastFrameTime;
};

static __ABE_INLINE void OutputCallback(void *decompressionOutputRefCon,
        void *sourceFrameRefCon,
        OSStatus status,
        VTDecodeInfoFlags infoFlags,
        CVImageBufferRef imageBuffer,
        CMTime presentationTimeStamp,
        CMTime presentationDuration) {

    CHECK_NULL(decompressionOutputRefCon);
    CHECK_NULL(sourceFrameRefCon);  // strong ref to the packet
    sp<MediaPacket> *packet = static_cast<sp<MediaPacket> * >(sourceFrameRefCon);

    DEBUG("packet %p, status %d, infoFlags %#x, imageBuffer %p, presentationTimeStamp %.3f(s)/%.3f(s)",
            packet->get()->data, status, infoFlags, imageBuffer,
            CMTimeGetSeconds(presentationTimeStamp),
            CMTimeGetSeconds(presentationDuration));

    delete packet;

    if (status || imageBuffer == NULL) {
        ERROR("decode frame failed, st = %d", status);
        return;
    }

    VTContext *vtc = (VTContext*)decompressionOutputRefCon;
    AutoLock _l(vtc->mLock);

    MediaTime pts;
    if (CMTIME_IS_INVALID(presentationTimeStamp) || FORCE_DTS) {
        //WARN("decode output invalid pts");
        pts = *vtc->mTimestamps.begin();
        vtc->mTimestamps.pop();
    } else {
        pts = MediaTime( presentationTimeStamp.value, presentationTimeStamp.timescale );
    }

#if 0
    if (pts < vtc->mLastFrameTime) {
        ERROR("unorderred frame %.3f(s) < %.3f(s)", pts.seconds(), vtc->mLastFrameTime.seconds());
    }
    vtc->mLastFrameTime = pts;
#endif

    MediaTime duration ( presentationDuration.value, presentationDuration.timescale );
    VTMediaFrame frame(imageBuffer, pts, duration);

    vtc->mImages.push(frame);
    vtc->mImages.sort();
}

static __ABE_INLINE CFDictionaryRef setupFormatDescriptionExtension(const Message& formats,
        CMVideoCodecType cm_codec_type) {
    CFMutableDictionaryRef atoms = CFDictionaryCreateMutable(
            kCFAllocatorDefault,
            1,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);

    switch (cm_codec_type) {
        case kCMVideoCodecType_H264:
            if (formats.contains("avcC")) {
                const Buffer& avcC = formats.find<Buffer>("avcC");
                CFDataRef data = CFDataCreate(kCFAllocatorDefault,
                        (const UInt8*)avcC.data(),
                        avcC.size());
                CFDictionarySetValue(atoms,
                        CFSTR("avcC"),
                        data);
                CFRelease(data);
            } else {
                ERROR("missing avcC");
            }
            break;
        case kCMVideoCodecType_HEVC:
            if (formats.contains("hvcC")) {
                const Buffer& hvcC = formats.find<Buffer>("hvcC");
                CFDataRef data = CFDataCreate(kCFAllocatorDefault,
                        (const UInt8*)hvcC.data(),
                        hvcC.size());
                CFDictionarySetValue(atoms,
                        CFSTR("hvcC"),
                        data);
                CFRelease(data);
            } else {
                ERROR("missing hvcC");
            }
            break;
        case kCMVideoCodecType_MPEG4Video:
            if (formats.contains("esds")) {
                const Buffer& esds = formats.find<Buffer>("esds");
                BitReader br(esds);
                MPEG4::ES_Descriptor esd(br);
                if (esd.valid) {
                    CFDataRef data = CFDataCreate(kCFAllocatorDefault,
                            (const UInt8*)esds.data(),
                            esds.size());
                    CFDictionarySetValue(atoms,
                            CFSTR("esds"),
                            data);
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

static __ABE_INLINE CFDictionaryRef setupImageBufferAttributes(int32_t width,
        int32_t height,
        OSType cv_pix_fmt,
        bool opengl) {

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
    if (opengl) {
        // https://ffmpeg.org/pipermail/ffmpeg-devel/2017-December/222481.html
#if TARGET_OS_IPHONE
        CFDictionarySetValue(attr, kCVPixelBufferOpenGLESCompatibilityKey, kCFBooleanTrue);
#else
        //kCVPixelBufferOpenGLCompatibilityKey
        CFDictionarySetValue(attr, kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey, kCFBooleanTrue);
#endif
    }

    CFRelease(surfaceProp);
    CFRelease(fmt);
    CFRelease(w);
    CFRelease(h);

    return attr;

}

// https://github.com/jyavenard/DecodeTest/blob/master/DecodeTest/VTDecoder.mm
static __ABE_INLINE sp<VTContext> createSession(const Message& formats, const Message& options, MediaError *err) {
    sp<VTContext> vtc = new VTContext;

    eCodecFormat codec_format = (eCodecFormat)formats.findInt32(kKeyFormat);
    int32_t width = formats.findInt32(kKeyWidth);
    int32_t height = formats.findInt32(kKeyHeight);
    int32_t requested_format = options.findInt32(kKeyRequestFormat, kPreferredPixelFormat);
    OSType cv_pix_fmt = get_cv_pix_format((ePixelFormat)requested_format);
    bool opengl = options.findInt32(kKeyOpenGLCompatible);

    if (cv_pix_fmt == 0) {
        ERROR("request format is not supported, fall to %d", kPreferredPixelFormat);
        cv_pix_fmt = get_cv_pix_format(kPreferredPixelFormat);
        //*err = kMediaErrorNotSupported;
        //return NULL;
    }

    CMVideoCodecType cm_codec_type = get_cm_codec_type(codec_format);
    if (cm_codec_type == 0) {
        ERROR("unsupported codec");
        *err = kMediaErrorNotSupported;
        return NULL;
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
            width,
            height,
            videoDecoderSpecification,
            &vtc->formatDescription);
    if (status) {
        ERROR("create video format desc failed");
        CFRelease(videoDecoderSpecification);
        return NULL;
    }

    CFDictionaryRef destinationImageBufferAttributes = setupImageBufferAttributes(
            width,
            height,
            cv_pix_fmt,
            opengl);

    VTDecompressionOutputCallbackRecord callback;
    callback.decompressionOutputCallback = OutputCallback;
    callback.decompressionOutputRefCon = vtc.get();
    status = VTDecompressionSessionCreate(
            NULL,
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
            ERROR("Unknown VideoToolbox session creation error %d", (int)status);
            break;
    }

    if (status) {
        return NULL;
    } else {
        vtc->width  = width;
        vtc->height = height;
        vtc->pixel  = get_pix_format(cv_pix_fmt);
        return vtc;
    }
}

static __ABE_INLINE CMSampleBufferRef createCMSampleBuffer(sp<VTContext>& vtc,
        const sp<MediaPacket>& packet) {
    DEBUG("CMBlockBufferGetTypeID: %#x", CMBlockBufferGetTypeID());
    CMBlockBufferRef  blockBuffer = NULL;
    CMSampleBufferRef sampleBuffer = NULL;

    OSStatus status = CMBlockBufferCreateWithMemoryBlock(
            kCFAllocatorDefault,        // structureAllocator -> default allocator
            (char*)packet->data,        // memoryBlock
            packet->size,               // blockLength
            kCFAllocatorNull,           // blockAllocator -> no deallocation
            NULL,                       // customBlockSource
            0,                          // offsetToData
            packet->size,               // dataLength
            0,                          // flags
            &blockBuffer);

    if (kCMBlockBufferNoErr != status) {
        ERROR("CMBlockBufferCreateWithMemoryBlock failed, error = %d", status);
        return NULL;
    }
    CHECK_NULL(blockBuffer);

    CMSampleTimingInfo timingInfo[1];
    if (packet->dts != kTimeInvalid) {
        timingInfo[0].decodeTimeStamp = CMTimeMake(packet->dts.value, packet->dts.timescale);
    } else {
        WARN("dts is missing");
        timingInfo[0].decodeTimeStamp = kCMTimeInvalid;
    }
    if (packet->pts != kTimeInvalid && !FORCE_DTS) {
        timingInfo[0].presentationTimeStamp = CMTimeMake(packet->pts.value, packet->pts.timescale);
    } else {
        //WARN("pts is missing");
        timingInfo[0].presentationTimeStamp = kCMTimeInvalid;
        vtc->mTimestamps.push(packet->dts);
        vtc->mTimestamps.sort();
    }
    // FIXME
    timingInfo[0].duration = kCMTimeInvalid;

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
            NULL,                   // sampleSizeArray
            &sampleBuffer);

    CFRelease(blockBuffer);

    if (status) {
        ERROR("CMSampleBufferCreate faled, error = %#x", status);
        return NULL;
    } else {
        return sampleBuffer;
    }
}

__ABE_HIDDEN sp<MediaFrame> readVideoToolboxFrame(CVPixelBufferRef pixbuf) {
    sp<MediaFrame> frame = MediaFrameCreate(get_pix_format(CVPixelBufferGetPixelFormatType(pixbuf)),
            CVPixelBufferGetWidth(pixbuf),
            CVPixelBufferGetHeight(pixbuf));

    CVReturn err = CVPixelBufferLockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
    if (err != kCVReturnSuccess) {
        ERROR("Error locking the pixel buffer");
        return NULL;
    }

    size_t left, right, top, bottom;
    CVPixelBufferGetExtendedPixels(pixbuf, &left, &right, &top, &bottom);
    DEBUGV("paddings %zu %zu %zu %zu", left, right, top, bottom);

    if (CVPixelBufferIsPlanar(pixbuf)) {
        // as we have to copy the data, copy to continueslly space
        DEBUGV("CVPixelBufferGetDataSize %zu", CVPixelBufferGetDataSize(pixbuf));
        DEBUGV("CVPixelBufferGetPlaneCount %zu", CVPixelBufferGetPlaneCount(pixbuf));

        sp<Buffer> data = new Buffer(CVPixelBufferGetDataSize(pixbuf));
        size_t planes = CVPixelBufferGetPlaneCount(pixbuf);
        for (size_t i = 0; i < planes; i++) {
            DEBUGV("CVPixelBufferGetBaseAddressOfPlane %p", CVPixelBufferGetBaseAddressOfPlane(pixbuf, i));
            DEBUGV("CVPixelBufferGetBytesPerRowOfPlane %zu", CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i));
            DEBUGV("CVPixelBufferGetWidthOfPlane %zu", CVPixelBufferGetWidthOfPlane(pixbuf, i));
            DEBUGV("CVPixelBufferGetHeightOfPlane %zu", CVPixelBufferGetHeightOfPlane(pixbuf, i));

            CHECK_LE(CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i) *
                    CVPixelBufferGetHeightOfPlane(pixbuf, i),
                    frame->planes[i].size);
            memcpy(frame->planes[i].data,
                    CVPixelBufferGetBaseAddressOfPlane(pixbuf, i),
                    CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i) *
                    CVPixelBufferGetHeightOfPlane(pixbuf, i));
        }
    } else {
        // FIXME: is this right
        CHECK_LE(CVPixelBufferGetBytesPerRow(pixbuf), frame->planes[0].size);
        memcpy(frame->planes[0].data,
                CVPixelBufferGetBaseAddress(pixbuf),
                CVPixelBufferGetBytesPerRow(pixbuf));
    }

    CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);

    return frame;
}

struct __ABE_HIDDEN VideoToolboxDecoder : public MediaDecoder {
    sp<VTContext>   mVTContext;

    VideoToolboxDecoder() { }
    virtual ~VideoToolboxDecoder() { }

    virtual String string() const { return ""; }

    virtual Message formats() const {
        Message formats;
        formats.setInt32(kKeyWidth, mVTContext->width);
        formats.setInt32(kKeyHeight, mVTContext->height);
        formats.setInt32(kKeyFormat, mVTContext->pixel);
        formats.setInt32(kKeyOpenGLCompatible, 1);
        DEBUG(" => %s", formats.string().c_str());
        return formats;
    }

    virtual MediaError configure(const Message& options) {
        return kMediaErrorNotSupported;
    }

    virtual MediaError init(const Message& format, const Message& options) {
        INFO("create VideoToolbox for %s", format.string().c_str());

        DEBUG("VTDecompressionSessionGetTypeID: %#x", VTDecompressionSessionGetTypeID());

        MediaError err = kMediaErrorUnknown;
        mVTContext = createSession(format, options, &err);
        if (mVTContext == NULL) return err;
        return kMediaNoError;
    }

    virtual MediaError write(const sp<MediaPacket>& input) {
        if (input == NULL) {
            INFO("eos");
            VTDecompressionSessionFinishDelayedFrames(mVTContext->decompressionSession);
            // no need to wait here.
            mVTContext->mInputEOS = true;
            return kMediaNoError;
        }

        DEBUG("queue packet %p: size %zu, dts %.3f(s), pts %.3f(s)",
                input->data,
                input->size,
                input->dts.seconds(),
                input->pts.seconds());

        CHECK_TRUE(input->dts != kTimeInvalid || input->pts != kTimeInvalid);

        CMSampleBufferRef sampleBuffer = createCMSampleBuffer(mVTContext, input);

        // FIXME:
        // kVTDecodeFrame_EnableTemporalProcessing is not working as expected.
        // it may be something wrong with the packet's pts => find out!!!
        VTDecodeFrameFlags decodeFlags = kVTDecodeFrame_EnableTemporalProcessing;
        //decodeFlags |= kVTDecodeFrame_EnableAsynchronousDecompression;
        //decodeFlags |= kVTDecodeFrame_1xRealTimePlayback;
        if (input->flags & kFrameFlagReference) {
            INFO("reference frame");
            decodeFlags |= kVTDecodeFrame_DoNotOutputFrame;
        }
        VTDecodeInfoFlags infoFlagsOut;
        /**
         * NOTE:
         * kVTDecodeFrame_EnableTemporalProcessing may not working as expected,
         * if the pts is not set properly.
         */
        OSStatus status = VTDecompressionSessionDecodeFrame(
                mVTContext->decompressionSession,   // VTDecompressionSessionRef
                sampleBuffer,                       // CMSampleBufferRef
                decodeFlags,                        // VTDecodeFrameFlags
                new sp<MediaPacket>(input),         // sourceFrameRefCon
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

    virtual sp<MediaFrame> read() {
        if (mVTContext->mInputEOS && mVTContext->mImages.empty()) {
            INFO("eos...");
            return NULL;
        }

        // for frame reordering
        if (mVTContext->mImages.size() < 4 && !mVTContext->mInputEOS) {
            INFO("no frames ready");
            return NULL;
        }

        AutoLock _l(mVTContext->mLock);
        VTMediaFrame& frame = *mVTContext->mImages.begin();

        // fix the width & height
        frame.v.rect.w  = mVTContext->width;
        frame.v.rect.h  = mVTContext->height;

#if 0
        sp<MediaFrame> out = readVideoToolboxFrame((CVPixelBufferRef)frame.opaque);
#else
        sp<MediaFrame> out = new VTMediaFrame(frame);
#endif
        mVTContext->mImages.pop();
        return out;
    }

    virtual MediaError flush() {
        VTDecompressionSessionFinishDelayedFrames(mVTContext->decompressionSession);
        VTDecompressionSessionWaitForAsynchronousFrames(mVTContext->decompressionSession);

        AutoLock _l(mVTContext->mLock);
        mVTContext->mImages.clear();
        return kMediaNoError;
    }
};

bool IsVideoToolboxSupported(eCodecFormat format) {
    CMPixelFormatType cm = get_cm_codec_type(format);
    if (cm == 0) return false;
    return VTIsHardwareDecodeSupported(cm);
}

sp<MediaDecoder> CreateVideoToolboxDecoder() {
    return new VideoToolboxDecoder();
}

__END_NAMESPACE_ABE
