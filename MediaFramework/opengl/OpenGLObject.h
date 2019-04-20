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


// File:    OpenGLObject.h
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//

#ifndef _MPX_OPENGL_OBJECT_H
#define _MPX_OPENGL_OBJECT_H

#include <MediaFramework/MediaDefs.h>
#include <MediaFramework/MediaFrame.h>

__BEGIN_NAMESPACE_MPX


template <typename TYPE, size_t N> struct Matrix {
    TYPE    value[N][N];
    
    Matrix();
    
    void        reset();
    
    Matrix      operator-(const Matrix&);
    Matrix      operator+(const Matrix&);
    Matrix      operator*(const Matrix&);
    Matrix      operator/(const Matrix&);
};

template <typename TYPE, size_t N> Matrix<TYPE, N>::Matrix() {
    reset();
}

template <typename TYPE, size_t N> void Matrix<TYPE, N>::reset() {
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j)  value[i][j] = 0;
        value[i][i] = 1;
    }
}

template <typename TYPE, size_t N>
Matrix<TYPE, N> Matrix<TYPE, N>::operator+(const Matrix& rhs) {
    
    for (size_t i = 0; i < N; ++i) {
        //for (size_t j = 0; j < N; ++j)
    }
}

struct OpenGLContext;
struct OpenGLObject : public SharedObject {
    
    OpenGLObject() { }
    
    MediaError init(const ImageFormat&, bool offscreen = true);
    
    //MediaError setTransformMatrix();
    
    MediaError draw(const sp<MediaFrame>&);
    
    sp<OpenGLContext> mOpenGL;
};

__END_NAMESPACE_MPX

#endif // _MPX_OPENGL_OBJECT_H
