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

#define LOG_TAG "JPEG"
#define LOG_NDEBUG 0
#include "ImageFile.h"

#include "JIF.h"
#include "JFIF.h"

// JPEG, image/jpeg, ISO 10918-1,
#include <stdio.h>
#include "jpeglib.h"

// JPEG 2000, image/jp2, ISO/IEC 15444-1
#include "openjpeg.h"

__BEGIN_NAMESPACE_MPX

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
    if (p_nb_bytes > pipe->size()) return OPJ_FALSE;
    int64_t pos = pipe->seek(p_nb_bytes);
    return OPJ_TRUE;
}

static opj_stream_t * opj2content_bridge(const sp<Content>& pipe) {
    opj_stream_t * opj = opj_stream_create(4096, true);
    
    opj_stream_set_user_data_length(opj, pipe->size());
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
    
    // JPEG
    jpeg_decompress_struct  mJPEGDecoder;
    
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
        mJFIFObject = openJFIF(pipe);
        
        jpeg_create_decompress(&mJPEGDecoder);
        
        
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
