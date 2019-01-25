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
#include <MediaToolkit/Toolkit.h>

#include "videotoolbox/VTDecoder.h"

#include <VideoToolbox/VideoToolbox.h>

//#define VERBOSE
#ifdef VERBOSE
#define DEBUGV(fmt, ...) DEBUG(fmt, ##__VA_ARGS__)
#else
#define DEBUGV(fmt, ...) do {} while(0)
#endif

// kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange -> nv12
#define kPreferredPixelFormat kPixelFormatNV12

// https://www.objc.io/issues/23-video/videotoolbox/
// http://www.enkichen.com/2018/03/24/videotoolbox/?utm_source=tuicool&utm_medium=referral
namespace mtdcy { namespace VideoToolbox {

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

    CMVideoCodecType get_cm_codec_type(eCodecFormat a) {
        for (size_t i = 0; kCodecMap[i].a != kCodecFormatUnknown; ++i) {
            if (kCodecMap[i].a == a)
                return kCodecMap[i].b;
        }
        FATAL("FIXME");
        return 0;
    }

    eCodecFormat get_codec_format(CMVideoCodecType b) {
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
        {kPixelFormatYUV422P,       kCVPixelFormatType_422YpCbCr8},
#ifdef kCFCoreFoundationVersionNumber10_7
        {kPixelFormatNV12,          kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange},
#endif
        // END OF LIST
        {kPixelFormatUnknown,       0},
    };

    ePixelFormat get_pix_format(OSType b) {
        for (size_t i = 0; kPixelMap[i].a != kPixelFormatUnknown; ++i) {
            if (kPixelMap[i].b == b) return kPixelMap[i].a;
        }
        FATAL("FIXME");
        return kPixelFormatUnknown;
    }

    OSType get_cv_pix_format(ePixelFormat a) {
        for (size_t i = 0; kPixelMap[i].a != kPixelFormatUnknown; ++i) {
            if (kPixelMap[i].a == a) return kPixelMap[i].b;
        }
        FATAL("FIXME");
        return 0;
    }

    // for store unorderred image buffers and sort them
    struct Image {
        Image(CVPixelBufferRef pix, MediaTime& pts, MediaTime& duration) :
        image(CVPixelBufferRetain(pix)), pts(pts), duration(duration) { }
        
        Image(const Image& rhs) :
        image(CVPixelBufferRetain(rhs.image)), pts(rhs.pts), duration(rhs.duration) { }
        
        ~Image() { if (image) CFRelease(image); }
        
        CVPixelBufferRef    image;
        MediaTime           pts;
        MediaTime           duration;
        bool operator<(const Image& rhs) const {
            return pts < rhs.pts;
        }
    };

    struct VTContext {
        VTContext() : session(NULL), vt_fmt_desc(NULL),
        inputEOS(false) { }

        ~VTContext() {
            if (session) {
                VTDecompressionSessionInvalidate(session);
                CFRelease(session);
            }
            if (vt_fmt_desc) {
                CFRelease(vt_fmt_desc);
            }
        }

        int32_t width, height;
        ePixelFormat pixel;
        VTDecompressionSessionRef       session;
        VTDecompressionOutputCallback   callback;
        CMVideoFormatDescriptionRef     vt_fmt_desc;
        //Decoder                         *target;
        bool                            inputEOS;

        Mutex                           mLock;
        List<Image>                     mImages;
    };

    static void OutputCallback(void *decompressionOutputRefCon,
            void *sourceFrameRefCon,
            OSStatus status,
            VTDecodeInfoFlags infoFlags,
            CVImageBufferRef imageBuffer,
            CMTime presentationTimeStamp,
            CMTime presentationDuration) {
        DEBUG("status %d, infoFlags %#x, imageBuffer %p, presentationTimeStamp %.3f(s)/%.3f(s)",
                status, infoFlags, imageBuffer,
                CMTimeGetSeconds(presentationTimeStamp),
                CMTimeGetSeconds(presentationDuration));
        
        if (imageBuffer == NULL) {
            INFO("null frame");
            return;
        }

        VTContext *vtc = (VTContext*)decompressionOutputRefCon;
        AutoLock _l(vtc->mLock);

        MediaTime pts ( presentationTimeStamp.value, presentationTimeStamp.timescale );
        MediaTime duration ( presentationDuration.value, presentationDuration.timescale );
        Image frame(imageBuffer, pts, duration);

        vtc->mImages.push(frame);
        vtc->mImages.sort();
    }

    CFDictionaryRef setupFormatDescriptionExtension(const Message& formats,
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
                    CFDataRef data = CFDataCreate(kCFAllocatorDefault,
                            (const UInt8*)esds.data(),
                            esds.size());
                    CFDictionarySetValue(atoms,
                            CFSTR("esds"),
                            data);
                    CFRelease(data);
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

    CFDictionaryRef setupImageBufferAttributes(int32_t width,
            int32_t height,
            OSType cv_pix_fmt) {

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
#if 1 // https://ffmpeg.org/pipermail/ffmpeg-devel/2017-December/222481.html
#if TARGET_OS_IPHONE
        CFDictionarySetValue(attr, kCVPixelBufferOpenGLESCompatibilityKey, kCFBooleanTrue);
#else
        //kCVPixelBufferOpenGLCompatibilityKey
        CFDictionarySetValue(attr, kCVPixelBufferIOSurfaceOpenGLTextureCompatibilityKey, kCFBooleanTrue);
#endif
#endif

        CFRelease(surfaceProp);
        CFRelease(fmt);
        CFRelease(w);
        CFRelease(h);

        return attr;

    }

    // https://github.com/jyavenard/DecodeTest/blob/master/DecodeTest/VTDecoder.mm
    sp<VTContext> createSession(const Message& formats) {
        sp<VTContext> vtc = new VTContext;

        eCodecFormat codec_format = (eCodecFormat)formats.findInt32(kKeyFormat);
        int32_t width = formats.findInt32(kKeyWidth);
        int32_t height = formats.findInt32(kKeyHeight);

        CMVideoCodecType cm_codec_type = get_cm_codec_type(codec_format);
        if (cm_codec_type == 0) {
            ERROR("unsupported codec");
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
                &vtc->vt_fmt_desc);
        if (status) {
            ERROR("create video format desc failed");
            CFRelease(videoDecoderSpecification);
            return NULL;
        }

        CFDictionaryRef destinationImageBufferAttributes = setupImageBufferAttributes(
                width,
                height,
                get_cv_pix_format(kPreferredPixelFormat));

        VTDecompressionOutputCallbackRecord callback;
        callback.decompressionOutputCallback = OutputCallback;
        callback.decompressionOutputRefCon = vtc.get();
        status = VTDecompressionSessionCreate(
                NULL,
                vtc->vt_fmt_desc,
                (CFDictionaryRef)videoDecoderSpecification,
                destinationImageBufferAttributes,
                &callback,
                &vtc->session);

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
            vtc->pixel  = kPreferredPixelFormat;
            return vtc;
        }
    }

    VTDecoder::VTDecoder(const Message& formats) {
        INFO("%s", formats.string().c_str());

        DEBUG("VTDecompressionSessionGetTypeID: %#x", VTDecompressionSessionGetTypeID());

        mVTContext = createSession(formats);
    }

    VTDecoder::~VTDecoder() {
    }

    String VTDecoder::string() const {
        return "";
    }

    status_t VTDecoder::status() const {
        return mVTContext != NULL ? OK : UNKNOWN_ERROR;
    }

    Message VTDecoder::formats() const {
        Message formats;
        formats.setInt32(kKeyWidth, mVTContext->width);
        formats.setInt32(kKeyHeight, mVTContext->height);
        formats.setInt32(kKeyFormat, mVTContext->pixel);
        INFO(" => %s", formats.string().c_str());
        return formats;
    }

    status_t VTDecoder::configure(const Message& options) {
        return INVALID_OPERATION;
    }

    static CMSampleBufferRef createCMSampleBuffer(const sp<VTContext>& vtc,
            const sp<MediaPacket>& packet)
    {
        DEBUG("CMBlockBufferGetTypeID: %#x", CMBlockBufferGetTypeID());
        CMBlockBufferRef  blockBuffer = NULL;
        CMSampleBufferRef sample = NULL;

        OSStatus status = CMBlockBufferCreateWithMemoryBlock(
                kCFAllocatorDefault,        // structureAllocator
                const_cast<char*>(packet->data->data()), // memoryBlock
                packet->data->capacity(),   // blockLength
                kCFAllocatorNull,           // blockAllocator
                NULL,                       // customBlockSource
                0,                          // offsetToData
                packet->data->size(),       // dataLength
                0,                          // flags
                &blockBuffer);

        if (kCMBlockBufferNoErr != status) {
            ERROR("CMBlockBufferCreateWithMemoryBlock failed, error = %d", status);
            return NULL;
        } else {
            CMSampleTimingInfo timingInfo[1];
            timingInfo[0].decodeTimeStamp = CMTimeMake(packet->dts.value, packet->dts.timescale);
            //timingInfo[0].presentationTimeStamp = kCMTimeInvalid;
            timingInfo[0].presentationTimeStamp = CMTimeMake(packet->pts.value, packet->pts.timescale);
            timingInfo[0].duration = kCMTimeInvalid;
            status = CMSampleBufferCreate(
                    kCFAllocatorDefault,  // allocator
                    blockBuffer,          // dataBuffer
                    TRUE,                 // dataReady
                    0,                    // makeDataReadyCallback
                    0,                    // makeDataReadyRefcon
                    vtc->vt_fmt_desc,     // formatDescription
                    1,                    // numSamples
                    1,                    // numSampleTimingEntries
                    &timingInfo[0],       // sampleTimingArray
                    0,                    // numSampleSizeEntries
                    NULL,                 // sampleSizeArray
                    &sample);
        }

        if (blockBuffer) CFRelease(blockBuffer);

        if (status) {
            ERROR("CMSampleBufferCreate faled, error = %#x", status);
            return NULL;
        } else {
            return sample;
        }
    }

    status_t VTDecoder::write(const sp<MediaPacket>& input) {
        if (input == NULL) {
            INFO("eos");
            VTDecompressionSessionFinishDelayedFrames(mVTContext->session);
            mVTContext->inputEOS = true;
        } else {
            DEBUG("queue packet: size %zu, dts %.3f(s)",
                    input->data->size(),
                    (double)input->dts / mVTContext->timescale);

            CMSampleBufferRef sample = createCMSampleBuffer(mVTContext, input);

            VTDecodeFrameFlags decodeFlags = kVTDecodeFrame_EnableTemporalProcessing;
            decodeFlags |= kVTDecodeFrame_1xRealTimePlayback;
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
                    mVTContext->session,
                    sample,
                    decodeFlags,    // VTDecodeFrameFlags
                    NULL,       // sourceFrameRefCon
                    &infoFlagsOut       // VTDecodeInfoFlags
                    );
            DEBUG("decode info flag %#x", infoFlagsOut);

            CFRelease(sample);

            if (status) {
                ERROR("VTDecompressionSessionDecodeFrame failed, error = %#x", status);
                return UNKNOWN_ERROR;
            }
        }
        VTDecompressionSessionWaitForAsynchronousFrames(mVTContext->session);
        return OK;
    }
    
    struct PixelBuffer : public Memory {
        PixelBuffer(CVPixelBufferRef ref) : Memory(), pixbuf(CVPixelBufferRetain(ref)) { }
        virtual ~PixelBuffer() { CFRelease(data()); }
        
        virtual void*       data() { return pixbuf; };
        virtual const void* data() const { return pixbuf; };
        virtual size_t      capacity() const { return 1; } ;
        virtual status_t    resize(size_t) { return INVALID_OPERATION; }
        
        CVPixelBufferRef pixbuf;
    };

    sp<MediaFrame> createMediaFrame(CVPixelBufferRef pixbuf) {
        sp<MediaFrame> frame = new MediaFrame;
        
        // TODO: find out this frame is backend by gpu memory or system memory
        IOSurfaceRef surface = CVPixelBufferGetIOSurface(pixbuf);
        if (surface) {
            frame->data[0]          = new Buffer(new PixelBuffer(pixbuf));
            frame->format           = kPixelFormatVideoToolbox;
            frame->v.strideWidth    = CVPixelBufferGetWidth(pixbuf);
            frame->v.sliceHeight    = CVPixelBufferGetHeight(pixbuf);
        } else {
            OSType cvPixelFormat = CVPixelBufferGetPixelFormatType(pixbuf);
            frame->format = get_pix_format(cvPixelFormat);

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
                    data->write((const char *)CVPixelBufferGetBaseAddressOfPlane(pixbuf, i),
                            CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i) *
                            CVPixelBufferGetHeightOfPlane(pixbuf, i));
                }
                frame->data[0] = data;
            } else {
                // FIXME: is this right
                frame->data[0] = new Buffer(
                        (const char*)CVPixelBufferGetBaseAddress(pixbuf),
                        CVPixelBufferGetBytesPerRow(pixbuf));
            }

            frame->v.strideWidth    = CVPixelBufferGetWidth(pixbuf);
            frame->v.sliceHeight    = CVPixelBufferGetHeight(pixbuf);

            CVPixelBufferUnlockBaseAddress(pixbuf, kCVPixelBufferLock_ReadOnly);
        }
        
        return frame;
    }

    sp<MediaFrame> VTDecoder::read() {
        if (mVTContext->inputEOS && mVTContext->mImages.empty()) {
            INFO("eos...");
            return NULL;
        }

        // for frame reordering
        if (mVTContext->mImages.size() < 4) {
            INFO("no frames ready");
            return NULL;
        }

        AutoLock _l(mVTContext->mLock);
        Image frame = *mVTContext->mImages.begin();
        mVTContext->mImages.pop();

        sp<MediaFrame> out = createMediaFrame(frame.image);
        if (out == NULL) {
            ERROR("createMediaFrame failed.");
            return NULL;
        }

        out->v.width = mVTContext->width;
        out->v.height = mVTContext->height;
        out->pts = frame.pts;
        
        DEBUG("frame size %zu %.3f(s) => %d x %d => %d x %d",
                out->data[0]->size(),
                (double)out->pts / mVTContext->timescale,
                out->v.width,
                out->v.height,
                out->v.strideWidth,
                out->v.sliceHeight);

        return out;
    }

    status_t VTDecoder::flush() {
        // FIXME: how to flush directly
        VTDecompressionSessionFinishDelayedFrames(mVTContext->session);
        VTDecompressionSessionWaitForAsynchronousFrames(mVTContext->session);
        AutoLock _l(mVTContext->mLock);
        mVTContext->mImages.clear();
        return OK;
    }

}; };
