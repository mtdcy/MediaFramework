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


// File:    JPEG.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#define LOG_TAG "JFIF"
#define LOG_NDEBUG 0
#include "JFIF.h"
#include "JPEG.h"
#include "Exif.h"

#include "ImageFile.h"

// JPEG 2000, image/jp2, ISO/IEC 15444-1
#include "openjpeg.h"

__BEGIN_NAMESPACE_MPX
__BEGIN_NAMESPACE(JFIF)

sp<AppHeader> readAppHeader(const BitReader& br, size_t length) {
    DEBUG("JFIF: APP0 %zu bytes", length);
    const size_t start = br.offset() / 8;
    
    // 5 bytes
    String id = br.readS(5);
    if (id != "JFIF") {
        ERROR("JFIF: bad identifier %s", id.c_str());
        return NIL;
    }
    
    sp<AppHeader> header = new AppHeader;
    
    // 2 + 1 + 2 + 2 = 7 bytes
    header->version     = br.rb16();
    header->units       = br.r8();
    header->x           = br.rb16();
    header->y           = br.rb16();
    
    if (length >= 14) {
        // <- 12 bytes
        header->width0  = br.r8();
        header->height0 = br.r8();
        // XXX: a lot programs set width0 & height0 to 0
        if (header->width0 && header->height0) {
            // <- 12 + 2 bytes
            // width0 * height0 * 3 RGB bytes
            const size_t rgb = header->width0 * header->height0 * 3;
            if (length >= 14 + rgb) {
                header->thunmbnail = br.readB(rgb);
            } else {
                ERROR("JFIF: bad data");
            }
        }
    } else {
        header->width0  = 0;
        header->height0 = 0;
    }
    
    header->extension   = 0;
    return header;
}

MediaError extendAppHeader(sp<AppHeader>& header, const BitReader& br, size_t length) {
    const size_t start = br.offset() / 8;
    String id = br.readS(5);
    if (id != "JFXX") {
        ERROR("JFIF: bad extension APP0");
        return kMediaErrorUnknown;
    }
    
    header->extension = br.r8();
    // <- 6 bytes
    
    if (header->extension == 0x10) {    // JPEG
        // <- 6 bytes
        sp<JPEG::JIFObject> jif = JPEG::readJIFLazy(br, length - 6);
    } else if (header->extension == 0x11) { // 1 byte/pixel
        header->width0 = br.r8();
        header->height0 = br.r8();
        CHECK_GT(header->width0, 0);
        CHECK_GT(header->height0, 0);
        // <- 6 + 2
        // <- 768 bytes RGB palette
        // <- width0 * height0 bytes
        if (length >= 6 + 2 + 768 + header->width0 * header->height0) {
            sp<Buffer> palette = br.readB(768);
            header->thunmbnail = new Buffer(header->width0 * header->height0 * 3);
            for (size_t i = 0; i < header->width0; ++i) {
                uint8_t x = br.r8();
                header->thunmbnail->write(palette->data() + x * 3, 3);
            }
        } else {
            ERROR("JFIF/JFXX: bad data");
            return kMediaErrorUnknown;
        }
    } else if (header->extension == 0x13) {
        header->width0 = br.r8();
        header->height0 = br.r8();
        CHECK_GT(header->width0, 0);
        CHECK_GT(header->height0, 0);
        // <- 6 + 2
        // <= width0 * height0 * 3 RGB bytes
        if (length >= 6 + 2 + header->width0 * header->height0 * 3) {
            header->thunmbnail = br.readB(header->width0 * header->height0 * 3);
        } else {
            ERROR("JFIF/JFXX: bad data");
            return kMediaErrorUnknown;
        }
    }
    
    return kMediaNoError;
}

void printAppHeader(const sp<AppHeader>& header) {
    DEBUG("\tAPP0: version %#x, units: %" PRIu8 ", [%" PRIu16 " x %" PRIu16 "]",
          header->version, header->units, header->x, header->y);
    if (!header->thunmbnail.isNIL()) {
        DEBUG("\t\tthumbnail: [%" PRIu8 " x %" PRIu8 "], length %zu",
              header->width0, header->height0, header->thunmbnail->size());
    }
    if (header->extension) {
        DEBUG("\t\textension: %" PRIu8, header->extension);
        if (header->extension == 0x10 && header->jif != NIL) {
            JPEG::printJIFObject(header->jif);
        }
    }
}

__END_NAMESPACE(JFIF)

static FORCE_INLINE uint16_t RB16(sp<Content>& pipe) {
    sp<Buffer> data = pipe->read(2);
    return BitReader(data->data(), data->size()).rb16();
}
#define readMarker(pipe)    ((JPEG::eMarker)RB16(pipe))
#define readLength(pipe)    ((size_t)RB16(pipe))
sp<JFIFObject> openJFIF(sp<Content>& pipe) {
    sp<JFIFObject> jfif = new JFIFObject;
    
    JPEG::eMarker SOI = readMarker(pipe);
    if (SOI != JPEG::SOI) {
        ERROR("missing JFIF SOI segment, unexpected marker %s", JPEG::MarkerName(SOI));
        return NIL;
    }
    
    const int64_t start = pipe->tell() - 2;
    while (pipe->tell() + 4 < pipe->length()) {
        JPEG::eMarker marker = readMarker(pipe);
        if ((marker & 0xff00) != 0xff00) {
            ERROR("JFIF bad marker %#x", marker);
            break;
        }
        
        size_t length = readLength(pipe);
        DEBUG("JFIF %s: length %zu", JPEG::MarkerName(marker), length);
        
        length -= 2;
        sp<Buffer> data = pipe->read(length);
        if (data->size() < length) {
            ERROR("JFIF: corrupt image");
            break;
        }
        
        BitReader br (data->data(), data->size());
        
        if (marker == JPEG::APP0) {
            if (jfif->mAppHeader.isNIL())
                jfif->mAppHeader = JFIF::readAppHeader(br, length);
            else
                JFIF::extendAppHeader(jfif->mAppHeader, br, length);
        } else if (marker == JPEG::APP1) {
            if (jfif->mAttributeInformation != NIL) {
                DEBUG("APP1 already exists");
                continue;
            }
            jfif->mAttributeInformation = EXIF::readAttributeInformation(br, length);
        } else if (marker == JPEG::SOF0) {
            jfif->mFrameHeader = JPEG::readFrameHeader(br, length);
        } else if (marker == JPEG::DHT) {
            jfif->mHuffmanTables.push(JPEG::readHuffmanTable(br, length));
        } else if (marker == JPEG::DQT) {
            jfif->mQuantizationTables.push(JPEG::readQuantizationTable(br, length));
        } else if (marker == JPEG::DRI) {
            jfif->mRestartInterval = JPEG::readRestartInterval(br, length);
        } else if (marker == JPEG::SOS) {
            jfif->mScanHeader = JPEG::readScanHeader(br, length);
            break;
        } else {
            INFO("ignore marker %#x", marker);
        }
    }
    
    pipe->seek(pipe->length() - 2);
    JPEG::eMarker marker = readMarker(pipe);
    if (marker != JPEG::EOI) {
        ERROR("JFIF: missing EOI, read to the end");
        const size_t size = pipe->tell() - start;
        pipe->seek(start);
        jfif->mData = pipe->read(size);
    } else {
        const size_t size = pipe->tell() - start - 2;
        DEBUG("JFIF: compressed image @ %" PRId64 ", length %" PRId64, start, size);
        pipe->seek(start);
        jfif->mData = pipe->read(size);
    }
    DEBUG("pos: %" PRId64 "/%" PRId64, pipe->tell(), pipe->length());
    return jfif;
}

void printJFIFObject(const sp<JFIFObject>& jfif) {
    INFO("----------------------------------");
    INFO("JFIF object:");
    JPEG::printJIFObject(jfif);
    
    if (jfif->mAppHeader != NIL) {
        INFO("JFIF APP0:");
        JFIF::printAppHeader(jfif->mAppHeader);
    }
    
    if (jfif->mAttributeInformation != NIL) {
        INFO("JFIF Exif:");
        EXIF::printAttributeInformation(jfif->mAttributeInformation);
    }
    INFO("----------------------------------");
}

static OPJ_SIZE_T opj2content_bridge_read(void * p_buffer,
                                          OPJ_SIZE_T p_nb_bytes,
                                          void * p_user_data) {
    sp<Content> pipe = p_user_data;
    sp<Buffer> buffer = pipe->read(p_nb_bytes);
    if (buffer.isNIL()) {
        DEBUG("eos");
        return 0;
    }
    size_t n = buffer->read((char *)p_buffer, p_nb_bytes);
    DEBUG("read %zu / %zu bytes", n, p_nb_bytes);
    return n;
}

static OPJ_OFF_T opj2content_bridge_skip(OPJ_OFF_T p_nb_bytes,
                                         void * p_user_data) {
    DEBUG("skip %" PRId64, p_nb_bytes);
    sp<Content> pipe = p_user_data;
    const int64_t cur = pipe->tell();
    return pipe->skip(p_nb_bytes) - cur;
}

static OPJ_BOOL opj2content_bridge_seek(OPJ_OFF_T p_nb_bytes,
                                        void * p_user_data) {
    DEBUG("seek to %" PRId64, p_nb_bytes);
    sp<Content> pipe = p_user_data;
    if (p_nb_bytes > pipe->length()) return OPJ_FALSE;
    int64_t pos = pipe->seek(p_nb_bytes);
    return OPJ_TRUE;
}

static opj_stream_t * opj2content_bridge(const sp<Content>& pipe) {
    opj_stream_t * opj = opj_stream_create(4096, true);
    
    opj_stream_set_user_data_length(opj, pipe->length());
    opj_stream_set_read_function(opj, opj2content_bridge_read);
    opj_stream_set_seek_function(opj, opj2content_bridge_seek);
    opj_stream_set_skip_function(opj, opj2content_bridge_skip);
    opj_stream_set_user_data(opj, pipe.get(), NULL);
    return opj;
}

static void opj2log_bridge(const char *msg, void *client_data) {
    INFO("%s", msg);
}

struct JPEG_JFIF : public ImageFile {
    sp<JFIFObject>      mJFIFObject;
    
    // JPEG 2000
    opj_stream_t *      opj_stream;
    opj_codec_t *       opj_codec;
    opj_dparameters_t   opj_dparam;
    opj_image_t *       opj_image;
    
    JPEG_JFIF() : opj_stream(NULL), opj_codec(NULL), opj_image(NULL) { }
    
    ~JPEG_JFIF() {
        if (opj_stream) opj_stream_destroy(opj_stream);
        if (opj_codec) opj_destroy_codec(opj_codec);
        if (opj_image) opj_image_destroy(opj_image);
    }
    
    virtual MediaError init(sp<Content>& pipe, const sp<Message>& options) {
        
        int64_t start = pipe->tell();
        
        
        mJFIFObject = openJFIF(pipe);
        printJFIFObject(mJFIFObject);
        decodeJIFObject(mJFIFObject);
        
#if 0
        
        opj_stream = opj2content_bridge(pipe);
        
        opj_set_default_decoder_parameters(&opj_dparam);
        
        opj_codec = opj_create_decompress(OPJ_CODEC_JP2);
        opj_set_info_handler(opj_codec, opj2log_bridge, NULL);
        opj_set_warning_handler(opj_codec, opj2log_bridge, NULL);
        opj_set_error_handler(opj_codec, opj2log_bridge, NULL);
        if (opj_setup_decoder(opj_codec, &opj_dparam) == false) {
            ERROR("setup decoder failed.");
            return kMediaErrorUnknown;
        }
        
        if (opj_read_header(opj_stream, opj_codec, &opj_image)) {
            INFO("[%u, %u, %u, %u], numcomps %u, %d",
                 opj_image->x0, opj_image->y0,
                 opj_image->x1, opj_image->y1,
                 opj_image->numcomps, opj_image->color_space);
        }
        
        OPJ_INT32 width = opj_image->x1 - opj_image->x0;
        OPJ_INT32 height = opj_image->y1 - opj_image->y0;
        
        opj_codestream_info_v2_t * info = opj_get_cstr_info(opj_codec);
        INFO("tiles %d x %d", info->tdx, info->tdy);
        if (opj_set_decode_area(opj_codec, opj_image,
                                opj_image->x0, opj_image->y0,
                                opj_image->x0 + info->tdx,
                                opj_image->y0 + info->tdy)) {
            
            if (opj_decode(opj_codec, opj_stream, opj_image) == false) {
                ERROR("decode failed");
            }
        } else {
            ERROR("set decode area failed");
        }
        opj_destroy_cstr_info(&info);
        opj_end_decompress(opj_codec, opj_stream);
#endif
        return kMediaNoError;
    }
    
    virtual MediaError configure(const sp<Message>& options) { return kMediaErrorNotSupported; }
    
    virtual sp<Message> formats() const {
        
    }
    
    virtual sp<MediaPacket> read() {
        
    }
    
    virtual sp<MediaFrame> readImage() {
        
    }
    
    virtual MediaError write(const sp<MediaPacket>&) {
        
    }
    
    virtual MediaError writeImage(const sp<MediaFrame>&) {
        
    }
};

sp<ImageFile> CreateJPEG() {
    return new JPEG_JFIF;
}

__END_NAMESPACE_MPX
