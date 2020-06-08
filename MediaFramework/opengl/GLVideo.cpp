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


// File:    SDLVideo.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//
#define GL_SILENCE_DEPRECATION

#define LOG_TAG "GLVideo"
//#define LOG_NDEBUG 0
#include <ABE/ABE.h>

#include "MediaOut.h"
#include "OpenGLObject.h"
#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>

__BEGIN_NAMESPACE_MPX

////////////////////////////////////////////////////////////////////
struct GLVideo : public MediaOut {
    ImageFormat         mFormat;
    sp<OpenGLObject>    mOpenGL;
    
    GLVideo() : MediaOut(), mOpenGL(NULL) { }
    
    MediaError prepare(const sp<Message>& format, const sp<Message>& options) {
        CHECK_TRUE(format != NULL);
        INFO("gl video => %s", format->string().c_str());
        if (options != NULL) {
            INFO("\t options: %s", options->string().c_str());
        }

        int32_t width       = format->findInt32(kKeyWidth);
        int32_t height      = format->findInt32(kKeyHeight);
        ePixelFormat pixel  = (ePixelFormat)format->findInt32(kKeyFormat);
        
        mFormat.format      = pixel;
        mFormat.width       = width;
        mFormat.height      = height;
        
        void * openGL       = options->findPointer("OpenGLContext");
        CGLSetCurrentContext((CGLContextObj)openGL);
        
        mOpenGL             = new OpenGLObject();

        return mOpenGL->init(mFormat);
    }
    
    virtual sp<Message> formats() const {
        sp<Message> info = new Message;
        info->setInt32(kKeyWidth,   mFormat.width);
        info->setInt32(kKeyHeight,  mFormat.height);
        info->setInt32(kKeyFormat,  mFormat.format);
        return info;
    }

    virtual MediaError configure(const sp<Message> &options) {
        if (options->contains(kKeyRotate)) {
            eRotate angle = (eRotate)options->findInt32(kKeyRotate);
            mOpenGL->rotate(angle);
        }
        return kMediaErrorInvalidOperation;
    }

    virtual MediaError write(const sp<MediaFrame> &input) {
        if (input == NULL) {
            INFO("eos...");
            return kMediaNoError;
        }
        
        DEBUG("write : %s", GetImageFrameString(input).c_str());
        return mOpenGL->draw(input);
    }

    virtual MediaError flush() {
        //glClearColor(0, 0, 0, 0);
        //glClear(GL_COLOR_BUFFER_BIT);
        //glFlush();
        return kMediaNoError;
    }
};

sp<MediaOut> CreateGLVideo(const sp<Message>& formats, const sp<Message>& options) {
    sp<GLVideo> gl = new GLVideo();
    if (gl->prepare(formats, options) == kMediaNoError)
        return gl;
    return NULL;
}
__END_NAMESPACE_MPX
