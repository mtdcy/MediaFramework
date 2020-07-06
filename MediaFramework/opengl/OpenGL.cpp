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


// File:    OpenGLObject.cpp
// Author:  mtdcy.chen
// Changes:
//          1. 20160701     initial version
//
#define GL_SILENCE_DEPRECATION

#define LOG_TAG "OpenGL"
//#define LOG_NDEBUG 0
#include "MediaTypes.h"
#include "MediaDevice.h"

#define SL(x)   #x

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>
#include <CoreVideo/CoreVideo.h>
#else
#if defined(__MINGW32__)
#include <GL/glew.h>   // glCreateShader ...
#endif
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#endif

#define CHECK_GL_ERROR() do {       \
    GLenum err = glGetError();      \
    if (err != GL_NO_ERROR)         \
    ERROR("error %d", err);         \
} while(0)

__BEGIN_NAMESPACE_MPX

#define printMAT4(x) do {               \
    for (size_t i = 0; i < 4; ++i) {    \
        INFO("{ %f, %f, %f, %f }",      \
            x[i][0], x[i][1],           \
            x[i][2], x[i][3]);          \
    }                                   \
} while(0)

// https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
/**
 * ITU-R BT.601 for video range YpCbCr -> RGB @see eColorMatrix
 * @note default matrix
 * @note these are row matrix
 */
static GLfloat VEC4_VideoRangeBias[4] = {
    0.063,   0.5,    0.5,    0
};
static const GLfloat MAT4_BT601_VideoRange[16] = {    // SDTV
    // y, u, v, a
    1.164,      0,          1.596,      0,      // r
    1.164,      -0.391,     -0.813,     0,      // g
    1.164,      2.018,      0,          0,      // b
    0,          0,          0,          1.0     // a
};
static const GLfloat MAT4_BT709_VideoRange[16] = {    // HDTV
    // y, u, v, a
    1.164,      0,          1.793,      0,      // r
    1.164,      -2.213,     -0.533,     0,      // g
    1.164,      2.112,      0,          0,      // b
    0,          0,          0,          1.0     // a
};
static const GLfloat MAT4_BT2020_VideoRange[16] = {    // UHDTV
    // y, u, v, a
    1.164384,   0,          1.67867,    0,      // r
    1.164384,   -0.187326,  -0.65042,   0,      // g
    1.164384,   2.14177,    0,          0,      // b
    0,          0,          0,          1.0     // a
};

/**
 * ITU-R BT.601 for full range YpCbCr -> RGB
 * @note JFIF JPEG: using full range
 * @note these are row matrix
 */
static GLfloat VEC4_FullRangeBias[4] = {
 0,      0.5,    0.5,    0
};
static const GLfloat MAT4_BT601_FullRange[16] = {
    // y, u, v, a
    1,      0,          1.402,      0,      // r
    1,  -0.344136,     -0.714136,   0,      // g
    1,      1.772,      0,          0,      // b
    0,      0,          0,          1.0     // a
};

static const GLfloat MAT4_Identity[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

// 2D rectangle vertices : vec4
// 4 points in 2-space with point in row vector
static const GLfloat RECT_VERTICES_2D[8] = {
    // x,y
    -1.0,   -1.0,   // left bottom
    1.0,    -1.0,   // right botton
    -1.0,   1.0,    // left top
    1.0,    1.0,    // right top
};

// 2D rectangle texture : vec2
// 4 points in 2-space with point in row vector
static const GLfloat RECT_TEXTURE_2D[8] = {
    0.0,    1.0,    // top left
    1.0,    1.0,    // top right
    0.0,    0.0,    // bottom left
    1.0,    0.0     // bottom right
};

struct TextureFormat {
    const GLint     internalformat;
    const GLenum    format;
    const GLenum    type;
};

struct OpenGLConfig {
    const char *            s_vsh;                      // vertex sl source
    const char *            s_fsh;                      // fragment sl source
    const GLenum            e_target;                   // texture target
    const GLsizei           n_textures;                 // n texture for n planes
    const TextureFormat     a_format[4];                // texture format for each plane
};

static const char * vsh = SL(
        uniform mat4 u_mvp;
        attribute vec4 a_position;              // each point in a column vector
        attribute vec2 a_texcoord;              // each point in a row vector
        varying vec2 v_texcoord;
        void main(void)
        {
            gl_Position = u_mvp * a_position;
            v_texcoord = a_texcoord;
        }
    );

static const char * fsh_rgb = SL(
        varying vec2 v_texcoord;
        uniform sampler2D u_planes[1];
        void main(void)
        {
            gl_FragColor = texture2D(u_planes[0], v_texcoord);
        }
    );

static const char * fsh_yuv1 = SL(
        varying vec2 v_texcoord;
        uniform sampler2D u_planes[1];
        uniform vec4 u_color_bias;
        uniform mat4 u_color_matrix;
        void main(void)
        {
            vec4 yuv = texture2D(u_planes[0], v_texcoord);
            gl_FragColor = (yuv - u_color_bias) * u_color_matrix;
        }
    );

static const char * fsh_yuv2 = SL(
        varying vec2 v_texcoord;
        uniform sampler2D u_planes[2];
        uniform vec4 u_color_bias;
        uniform mat4 u_color_matrix;
        void main(void)
        {
            vec3 yuv;
            yuv.x = texture2D(u_planes[0], v_texcoord).r;
            yuv.yz = texture2D(u_planes[1], v_texcoord).rg;
            gl_FragColor = (vec4(yuv, 1.0) - u_color_bias) * u_color_matrix;
        }
    );

static const char * fsh_yuv3 = SL(
        varying vec2 v_texcoord;
        uniform sampler2D u_planes[3];
        uniform vec4 u_color_bias;
        uniform mat4 u_color_matrix;
        void main(void)
        {
            vec3 yuv;
            yuv.x = texture2D(u_planes[0], v_texcoord).r;
            yuv.y = texture2D(u_planes[1], v_texcoord).r;
            yuv.z = texture2D(u_planes[2], v_texcoord).r;
            gl_FragColor = (vec4(yuv, 1.0) - u_color_bias) * u_color_matrix;
        }
    );

static const OpenGLConfig YpCbCrPlanar = {  // tri-planar
    .s_vsh      = vsh,
    .s_fsh      = fsh_yuv3,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 3,
    .a_format   = {
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE},
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE},
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE},
    },
};

static const OpenGLConfig YpCbCrSemiPlanar = {  // bi-planar
    .s_vsh      = vsh,
    .s_fsh      = fsh_yuv2,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 2,
    .a_format   = {
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE},
        {GL_RG, GL_RG, GL_UNSIGNED_BYTE},
    },
};

static const OpenGLConfig YpCrCbSemiPlanar = {  // TODO: implement uv swap
    .s_vsh      = vsh,
    .s_fsh      = fsh_yuv2,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 2,
    .a_format   = {
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE},
        {GL_RG, GL_RG, GL_UNSIGNED_SHORT},
    },
};

static const OpenGLConfig YpCbCr = {        // packed
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGB, GL_RGB, GL_UNSIGNED_BYTE},
    },
};

static const OpenGLConfig RGB565 = {
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5_REV},
    },
};

static const OpenGLConfig BGR565 = {
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
    },
};

static const OpenGLConfig RGB = {
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGB, GL_RGB, GL_UNSIGNED_BYTE},
    },
};

static const OpenGLConfig BGR = {   // read as bytes -> GL_BGR
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGB, GL_BGR, GL_UNSIGNED_BYTE},
    },
};

static const OpenGLConfig RGBA = {  // read as bytes -> GL_RGBA
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE},
    },
};

static const OpenGLConfig ABGR = {  // RGBA in word-order, so read as int
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGBA, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8},
    },
};

static const OpenGLConfig ARGB = {  // read as int -> GL_BGRA
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGBA, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8},
    },
};

static const OpenGLConfig BGRA = {  // read as byte -> GL_BGRA
    .s_vsh      = vsh,
    .s_fsh      = fsh_rgb,
    .e_target   = GL_TEXTURE_2D,
    .n_textures = 1,
    .a_format   = {
        {GL_RGBA, GL_BGRA, GL_UNSIGNED_BYTE},
    },
};


#ifdef __APPLE__
// about rectangle texture
// https://www.khronos.org/opengl/wiki/Rectangle_Texture
// about yuv422
// https://www.khronos.org/registry/OpenGL/extensions/APPLE/APPLE_ycbcr_422.txt
// xxx: WHY NO COLOR MATRIX NEED HERE?
static const char * fsh_YpCbCr422_APPLE = SL(
        varying vec2 v_texcoord;
        uniform sampler2DRect u_planes[1];
        uniform vec4 u_color_bias;
        uniform mat4 u_color_matrix;
        uniform vec2 u_resolution;
        void main(void)
        {
            gl_FragColor = texture2DRect(u_planes[0], v_texcoord * u_resolution);
        }
    );

static const OpenGLConfig YpCbCr422_APPLE = {
    .s_vsh      = vsh,
    .s_fsh      = fsh_YpCbCr422_APPLE,
    .e_target   = GL_TEXTURE_RECTANGLE_ARB,
    .n_textures = 1,
    .a_format   = {
        {GL_RGB, GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE},
    },
};

static const char * fsh_YpCbCrSemiPlanar_APPLE = SL(
        varying vec2 v_texcoord;
        uniform sampler2DRect u_planes[2];
        uniform vec4 u_color_bias;
        uniform mat4 u_color_matrix;
        uniform vec2 u_resolution;
        void main(void)
        {
            vec3 yuv;
            vec2 coord = v_texcoord * u_resolution;
            yuv.x = texture2DRect(u_planes[0], coord).r;
            yuv.yz = texture2DRect(u_planes[1], coord * 0.5).rg;
            gl_FragColor = (vec4(yuv, 1.0) - u_color_bias) * u_color_matrix;
        }
    );

static const OpenGLConfig YpCbCrSemiPlanar_APPLE = {
    .s_vsh      = vsh,
    .s_fsh      = fsh_YpCbCrSemiPlanar_APPLE,
    .e_target   = GL_TEXTURE_RECTANGLE_ARB,
    .n_textures = 2,
    .a_format   = {
        {GL_RED, GL_RED, GL_UNSIGNED_BYTE},
        {GL_RG, GL_RG, GL_UNSIGNED_BYTE},
    },
};
#endif

static const OpenGLConfig * getOpenGLConfig(const ePixelFormat& pixel) {
    struct {
        ePixelFormat            pixel;
        const OpenGLConfig *    config;
    } kMap[] = {
        { kPixelFormat420YpCbCrSemiPlanar,  &YpCbCrSemiPlanar   },
        { kPixelFormat420YpCrCbSemiPlanar,  &YpCrCbSemiPlanar   },
        { kPixelFormat420YpCbCrPlanar,      &YpCbCrPlanar       },
        { kPixelFormat422YpCbCrPlanar,      &YpCbCrPlanar       },
        { kPixelFormat444YpCbCrPlanar,      &YpCbCrPlanar       },
        { kPixelFormat444YpCbCr,            &YpCbCr             },
        { kPixelFormatRGB565,               &RGB565             },
        { kPixelFormatBGR565,               &BGR565             },
        { kPixelFormatRGB,                  &RGB                },
        { kPixelFormatBGR,                  &BGR                },
        { kPixelFormatARGB,                 &ARGB               },
        { kPixelFormatBGRA,                 &BGRA               },
        { kPixelFormatRGBA,                 &RGBA               },
        { kPixelFormatABGR,                 &ABGR               },
#ifdef __APPLE__
        { kPixelFormatVideoToolbox,     &YpCbCrSemiPlanar_APPLE },
#endif
        { kPixelFormatUnknown,              NULL                }
    };
    
    for (size_t i = 0; ; ++i) {
        if (kMap[i].pixel == kPixelFormatUnknown) break;
        if (kMap[i].pixel == pixel) {
            return  kMap[i].config;
        }
    }
    return NULL;
}

struct OpenGLContext : public SharedObject {
    const OpenGLConfig *    mOpenGLConfig;
    const PixelDescriptor * mPixelDescriptor;
    
    // local context
    Matrix<GLfloat, 4>  mModelMatrix;
    Matrix<GLfloat, 4>  mViewMatrix;
    Matrix<GLfloat, 4>  mProjectionMatrix;
    
    // opengl context
    // opengl obj
    GLuint          mVertexShader;
    GLuint          mFragShader;
    GLuint          mProgram;
    GLuint          mTextures[4];
    // opengl attribute
    GLint           mVertexCoord;       // vec4
    GLint           mTextureCoord;      // vec2
    // opengl uniform
    GLint           mMVPMatrix;         // mat4
    GLint           mTextureLocation;   // sampler2D array
    
    // opengl uniforms for yuv -> rgb
    GLint           mColorBias;         // vec4
    GLint           mColorMatrix;       // mat4
    
    // special
    GLint           mResolution;        // vec2: width, height
    
    ~OpenGLContext() {
        glDeleteTextures(mOpenGLConfig->n_textures, mTextures);
        glDeleteShader(mVertexShader); mVertexShader = 0;
        glDeleteShader(mFragShader); mFragShader = 0;
        glDeleteProgram(mProgram);
    }
    
    OpenGLContext() :SharedObject(), mVertexShader(0), mFragShader(0), mProgram(0) {
        for (size_t i = 0; i < 4; ++i) mTextures[i] = 0;
    }
    
    MediaError applyMVP() {
        Matrix<GLfloat, 4> mvp = mModelMatrix * mViewMatrix * mProjectionMatrix;
        glUniformMatrix4fv(mMVPMatrix, 1, GL_FALSE, &mvp[0][0]);
        CHECK_GL_ERROR();
        return kMediaNoError;
    }
};

static GLuint initShader(GLenum type, const char *sl) {
    CHECK_NULL(sl);
    GLuint sh = glCreateShader(type);
    if (sh == 0) {
        ERROR("create shader of %#x failed.", type);
        return 0;
    }
    
    glShaderSource(sh, 1, &sl, NULL);
    glCompileShader(sh);
    
    GLint ok;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        GLchar log[len];
        glGetShaderInfoLog(sh, len, NULL, log);
        ERROR("compile shader failed. %s", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static FORCE_INLINE GLuint initProgram(GLuint vsh, GLuint fsh) {
    CHECK_NE(vsh, 0);
    CHECK_NE(fsh, 0);
    GLuint program = glCreateProgram();
    if (program == 0) {
        ERROR("create program failed.");
        return 0;
    }
    
    glAttachShader(program, vsh);
    glAttachShader(program, fsh);
    glLinkProgram(program);
    CHECK_GL_ERROR();
    
    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        GLchar log[len];
        glGetProgramInfoLog(program, len, NULL, log);
        //ERROR("program link failed. %s", log);
        
        glDeleteProgram(program);
        return 0;
    }
    
    return program;
}

static FORCE_INLINE size_t initTextures(size_t n, GLenum target, GLuint *textures) {
    glGenTextures(n, textures);
    for (size_t i = 0; i < n; ++i) {
        glBindTexture(target, textures[i]);
#if 0  //def __APPLE__
        // do we need this???
        if (target == GL_TEXTURE_RECTANGLE_ARB) {
            glTexParameteri(target, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_CACHED_APPLE);
        }
#endif
        glTexParameterf(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        CHECK_GL_ERROR();
        
        glBindTexture(target, 0);
    }
    return n;
}

static sp<OpenGLContext> initOpenGLContext(const ImageFormat& image, const OpenGLConfig * config) {
    sp<OpenGLContext> glc = new OpenGLContext;
    glc->mOpenGLConfig = config;
    glc->mPixelDescriptor = GetPixelFormatDescriptor(image.format);
#ifdef __APPLE__
    if (image.format == kPixelFormatVideoToolbox) {
        // TODO: get real pixel format from CVPixelBufferRef
        glc->mPixelDescriptor = GetPixelFormatDescriptor(kPixelFormat420YpCbCrSemiPlanar);
    }
#endif
    CHECK_NULL(glc->mPixelDescriptor);
    CHECK_NULL(glc->mOpenGLConfig);
    
    glc->mVertexShader = initShader(GL_VERTEX_SHADER, config->s_vsh);
    glc->mFragShader = initShader(GL_FRAGMENT_SHADER, config->s_fsh);
    if (glc->mVertexShader == 0 || glc->mFragShader == 0) return NULL;
    
    glc->mProgram = initProgram(glc->mVertexShader, glc->mFragShader);
    if (glc->mProgram == 0) return NULL;
    
    glUseProgram(glc->mProgram);
    CHECK_GL_ERROR();
    
    size_t n = initTextures(config->n_textures, config->e_target, glc->mTextures);
    CHECK_EQ(n, config->n_textures);
    
    // attribute of vertex shader
    glc->mVertexCoord = glGetAttribLocation(glc->mProgram, "a_position");
    glc->mTextureCoord = glGetAttribLocation(glc->mProgram, "a_texcoord");
    CHECK_GL_ERROR();
    CHECK_GE(glc->mVertexCoord, 0);
    CHECK_GE(glc->mTextureCoord, 0);
    
    // uniform of vertex shader
    glc->mMVPMatrix = glGetUniformLocation(glc->mProgram, "u_mvp");
    CHECK_GE(glc->mMVPMatrix, 0);
    
    // uniform of fragment shader
    glc->mTextureLocation = glGetUniformLocation(glc->mProgram, "u_planes");
    CHECK_GE(glc->mTextureLocation, 0);
    
    // mandatory for yuv
    glc->mColorBias  = glGetUniformLocation(glc->mProgram, "u_color_bias");
    glc->mColorMatrix = glGetUniformLocation(glc->mProgram, "u_color_matrix");
    
    // optional
    glc->mResolution = glGetUniformLocation(glc->mProgram, "u_resolution");
    
    // setup default value
    glVertexAttribPointer(glc->mVertexCoord, 2, GL_FLOAT, 0, 0, RECT_VERTICES_2D);
    glEnableVertexAttribArray(glc->mVertexCoord);
    CHECK_GL_ERROR();
    
    glVertexAttribPointer(glc->mTextureCoord, 2, GL_FLOAT, 0, 0, RECT_TEXTURE_2D);
    glEnableVertexAttribArray(glc->mTextureCoord);
    CHECK_GL_ERROR();
    
    glUniformMatrix4fv(glc->mMVPMatrix, 1, GL_FALSE, MAT4_Identity);
    CHECK_GL_ERROR();
    
    if (glc->mPixelDescriptor->color == kColorYpCbCr) {
        CHECK_GT(glc->mColorBias, 0);
        CHECK_GT(glc->mColorMatrix, 0);
        glUniform4fv(glc->mColorBias, 1, VEC4_VideoRangeBias);
        glUniformMatrix4fv(glc->mColorMatrix, 1, GL_FALSE, MAT4_BT601_VideoRange);
    }
    CHECK_GL_ERROR();
    
    if (glc->mResolution >= 0) {
        glUniform2f(glc->mResolution, (GLfloat)image.width, (GLfloat)image.height);
        CHECK_GL_ERROR();
    }
    
    return glc;
}

#ifdef __APPLE__
static void drawVideoToolboxFrame(const sp<OpenGLContext>& glc, const sp<MediaFrame>& frame) {
    CHECK_TRUE(frame->video.format == kPixelFormatVideoToolbox);
    CHECK_NULL(frame->opaque);
    CVPixelBufferRef pixbuf = (CVPixelBufferRef)frame->opaque;
    CHECK_NULL(pixbuf);
    
    OSType pixtype = CVPixelBufferGetPixelFormatType(pixbuf);
    if (CVPixelBufferIsPlanar(pixbuf)) {
        CHECK_EQ(CVPixelBufferGetPlaneCount(pixbuf), glc->mOpenGLConfig->n_textures);
    } else {
        CHECK_EQ(1, glc->mOpenGLConfig->n_textures);
    }
    //CHECK_TRUE(pixtype == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange); // nv12
    
    IOSurfaceRef iosurface = CVPixelBufferGetIOSurface(pixbuf);
    CHECK_NULL(iosurface);
    CHECK_NULL(CGLGetCurrentContext());
    
    GLint index[glc->mOpenGLConfig->n_textures];
    for (size_t i = 0; i < glc->mOpenGLConfig->n_textures; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(glc->mOpenGLConfig->e_target, glc->mTextures[i]);
        CHECK_GL_ERROR();
        
        CGLError err = CGLTexImageIOSurface2D(CGLGetCurrentContext(),
                                              glc->mOpenGLConfig->e_target,
                                              glc->mOpenGLConfig->a_format[i].internalformat,
                                              (GLsizei)(frame->video.width / glc->mPixelDescriptor->planes[i].hss),
                                              (GLsizei)(frame->video.height / glc->mPixelDescriptor->planes[i].vss),
                                              glc->mOpenGLConfig->a_format[i].format,
                                              glc->mOpenGLConfig->a_format[i].type,
                                              iosurface, i);
        
        if (err != kCGLNoError) {
            ERROR("CGLTexImageIOSurface2D failed. %d|%s", err, CGLErrorString(err));
        }
        
        index[i] = i;
    }
    
    glUniform1iv(glc->mTextureLocation, glc->mOpenGLConfig->n_textures, index);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    CHECK_GL_ERROR();
}
#endif
static MediaError drawFrame(const sp<OpenGLContext>& glc, const sp<MediaFrame>& frame) {
    
    // setting up texture rectangle
    GLfloat x = 0;
    GLfloat y = 0;
    GLfloat w = frame->video.width;
    GLfloat h = frame->video.height;
    
    if (frame->video.rect.w || frame->video.rect.h) {
        x = frame->video.rect.x;
        y = frame->video.rect.y;
        w = frame->video.rect.w;
        h = frame->video.rect.h;
    }
    
    x /= frame->video.width;
    w /= frame->video.width;
    y /= frame->video.height;
    h /= frame->video.height;
    
    // texture rectangle, vec2, each point in a row vector
    const GLfloat rect[8] = {
        x,      y + h,      // top left,
        x + w,  y + h,      // top right
        x,      y,          // bottom left
        x + w,  y           // bottom right
    };
    
    glVertexAttribPointer(glc->mTextureCoord, 2, GL_FLOAT, 0, 0, rect);
    
#ifdef __APPLE__
    if (frame->video.format == kPixelFormatVideoToolbox) {
        drawVideoToolboxFrame(glc, frame);
    } else
#endif
    {
        GLint index[glc->mOpenGLConfig->n_textures];
        for (size_t i = 0; i < glc->mOpenGLConfig->n_textures; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(glc->mOpenGLConfig->e_target, glc->mTextures[i]);
            CHECK_GL_ERROR();
            
            glTexImage2D(glc->mOpenGLConfig->e_target, 0,
                         glc->mOpenGLConfig->a_format[i].internalformat,
                         (GLsizei)(frame->video.width / glc->mPixelDescriptor->planes[i].hss),
                         (GLsizei)(frame->video.height / glc->mPixelDescriptor->planes[i].vss),
                         0,
                         glc->mOpenGLConfig->a_format[i].format,
                         glc->mOpenGLConfig->a_format[i].type,
                         (const GLvoid *)frame->planes.buffers[i].data);
            index[i] = i;
        }
        
        glUniform1iv(glc->mTextureLocation, glc->mOpenGLConfig->n_textures, index);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        CHECK_GL_ERROR();
    }

#ifdef __APPLE__
    glSwapAPPLE();
#else
    glFlush();  // always assume single buffer here, let client handle swap buffers
#endif
    CHECK_GL_ERROR();
    
    return kMediaNoError;
}

static const GLfloat MAT4_ROTATE_90[] = {
    0, -1, 0, 0,
    1, 0, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

static const GLfloat MAT4_ROTATE_180[] = {
    -1, 0, 0, 0,
    0, -1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

static const GLfloat MAT4_ROTATE_270[] = {
    0, 1, 0, 0,
    -1, 0, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

#if 0
MediaError OpenGLObject::setModelMatrix(const Matrix<float, 4>& matrix) {
    mOpenGL->mModelMatrix = matrix;
    return mOpenGL->applyMVP();
}

MediaError OpenGLObject::setViewMatrix(const Matrix<float, 4>& matrix) {
    mOpenGL->mViewMatrix = matrix;
    return mOpenGL->applyMVP();
}

MediaError OpenGLObject::setProjectionMatrix(const Matrix<float, 4>& matrix) {
    mOpenGL->mProjectionMatrix = matrix;
    return mOpenGL->applyMVP();
}

MediaError OpenGLObject::scale(float factor) {
    if (factor < 0) factor = 0;
    else if (factor > 1) factor = 1;
    Matrix<GLfloat, 4> matrix;
    for (size_t i = 0; i < 4; ++i) matrix[i][i] = factor;
    mOpenGL->mModelMatrix = mOpenGL->mModelMatrix * matrix;
    return mOpenGL->applyMVP();
}

MediaError OpenGLObject::translation(float x, float y) {
    Matrix<GLfloat, 4> matrix;
    matrix[0][3] = x;
    matrix[1][3] = y;
    printMAT4(matrix);
    mOpenGL->mModelMatrix = mOpenGL->mModelMatrix * matrix;
    return mOpenGL->applyMVP();
}
#endif

struct OpenGLOut : public MediaDevice {
    ImageFormat         mFormat;
    sp<OpenGLContext>   mOpenGL;
    
    OpenGLOut() : MediaDevice(), mOpenGL(NULL) { }
    
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
        
        if (options->contains(kKeyOpenGLContext)) {
            void * openGL = options->findPointer(kKeyOpenGLContext);
#ifdef __APPLE__
            if (openGL) CGLSetCurrentContext((CGLContextObj)openGL);
#endif
        }
#ifdef __APPLE__
        CHECK_NULL(CGLGetCurrentContext());
#endif
        
        const OpenGLConfig * config = getOpenGLConfig(mFormat.format);
        mOpenGL             = initOpenGLContext(mFormat, config);
        if (mOpenGL.isNIL()) return kMediaErrorBadFormat;
        return kMediaNoError;
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
            const GLfloat * matrix = MAT4_Identity;
            if (angle == kRotate90)         matrix = MAT4_ROTATE_90;
            else if (angle == kRotate180)   matrix = MAT4_ROTATE_180;
            else if (angle == kRotate270)   matrix = MAT4_ROTATE_270;
            
            mOpenGL->mModelMatrix = mOpenGL->mModelMatrix * Matrix<GLfloat, 4>(matrix);
            return mOpenGL->applyMVP();
        }
        return kMediaErrorInvalidOperation;
    }

    virtual MediaError push(const sp<MediaFrame> &input) {
        if (input == NULL) {
            INFO("eos...");
            return kMediaNoError;
        }
        
        DEBUG("write : %s", GetImageFrameString(input).c_str());
        return drawFrame(mOpenGL, input);
    }
    
    virtual sp<MediaFrame> pull() {
        return NULL;
    }

    virtual MediaError reset() {
        //glClearColor(0, 0, 0, 0);
        //glClear(GL_COLOR_BUFFER_BIT);
        //glFlush();
        return kMediaNoError;
    }
};

sp<MediaDevice> CreateOpenGLOut(const sp<Message>& formats, const sp<Message>& options) {
    sp<OpenGLOut> gl = new OpenGLOut();
    if (gl->prepare(formats, options) == kMediaNoError)
        return gl;
    return NULL;
}
__END_NAMESPACE_MPX
